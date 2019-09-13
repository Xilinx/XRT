/**
 * Copyright (C) 2016-2017 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include "trace_parser.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <chrono>
#include <bitset>

#define getBit(word, bit) (((word) >> bit) & 0x1)

namespace xdp {

  // Constructor
  TraceParser::TraceParser(XDPPluginI* Plugin)
    : PCIE_DELAY_OFFSET_MSEC(0.25),
      mStartTimeNsec(0),
      mPluginHandle(Plugin)
  {
    mNumTraceEvents = 0;
    // NOTE: setting this to 0x80000 causes runtime crash when running
    // HW emulation on 070_max_wg_size or 079_median1
    mMaxTraceEventsHwEm = 0x40000;
    mEmuTraceMsecOneCycle = 0.0;

    mTraceSamplesThreshold = MAX_TRACE_NUMBER_SAMPLES / 4;
    mSampleIntervalMsec = 10;

    mTraceClockRateMHz = 300.0;
    mDeviceClockRateMHz = 300.0;
    mGlobalMemoryClockRateMHz = 300.0;

    // Default bit width of global memory defined at APM monitoring slaves
    mGlobalMemoryBitWidth = XPAR_AXI_PERF_MON_0_SLOT0_DATA_WIDTH;

    // Analyzer assumes ID 0 as blank
    mCuEventID = 1;

    // Since device timestamps are in cycles and host timestamps are in msec,
    // then the slope of the line to convert from device to host timestamps
    // is in msec/cycle
    for (int i=0; i < XCL_PERF_MON_TOTAL_PROFILE; i++) {
      mTrainSlope[i] = 1000.0 / mTraceClockRateMHz;
      mTrainOffset[i] = 0.0;
    }
  }

  // Destructor
  TraceParser::~TraceParser() {
    ResetState();
  }

  void TraceParser::ResetState() {
    std::fill_n(mAccelMonStartedEvents,XAM_MAX_NUMBER_SLOTS,0);
    // Clear queues
    for (int i=0; i < XAIM_MAX_NUMBER_SLOTS; i++) {
      mWriteStarts[i].clear();
      mHostWriteStarts[i].clear();
      mReadStarts[i].clear();
      mHostReadStarts[i].clear();
    }
    for (int i=0; i< XASM_MAX_NUMBER_SLOTS; i++) {
      mStreamTxStarts[i].clear();
      mStreamStallStarts[i].clear();
      mStreamStarveStarts[i].clear();
      mStreamTxStartsHostTime[i].clear();
      mStreamStallStartsHostTime[i].clear();
      mStreamStarveStartsHostTime[i].clear();
    }
    for (int i=0; i< XAM_MAX_NUMBER_SLOTS; i++) {
      mAccelMonCuStarts[i].clear();
    }
  }

  // Log device trace results: store in queues and report events as they are completed
  void TraceParser::logTrace(const std::string& deviceName, xclPerfMonType type,
      xclTraceResultsVector& traceVector, TraceResultVector& resultVector) {
    if (traceVector.mLength == 0)
      return;

    // Hardware Emulation Trace
    bool isHwEmu = (mPluginHandle->getFlowMode() == xdp::RTUtil::HW_EM);
    if (isHwEmu && mPluginHandle->getSystemDPAEmulation() == false) {
      logTraceHWEmu(deviceName, traceVector, resultVector);
      return;
    }

    XDP_LOG("[profile_device] Logging %u device trace samples (total = %ld)...\n",
      traceVector.mLength, mNumTraceEvents);
    mNumTraceEvents += traceVector.mLength;

    uint64_t timestamp = 0;
    uint64_t startTime = 0;
    DeviceTrace kernelTrace;
    // Parse Start
    for (unsigned int i=0; i < traceVector.mLength; i++) {
      auto& trace = traceVector.mArray[i];
      XDP_LOG("[profile_device] Parsing trace sample %d...\n", i);
      packets_parsed++;

      timestamp = trace.Timestamp;

      if (trace.isClockTrain) {
        trainDeviceHostTimestamps(type, timestamp, trace.HostTimestamp);
      }

      // Overflow is taken care of in trace reader
      //if (trace.Overflow == 1)
      //  timestamp += LOOP_ADD_TIME_AIM;

      uint32_t s = 0;
      bool SAMPacket = (trace.TraceID >= MIN_TRACE_ID_AM && trace.TraceID <= MAX_TRACE_ID_AM);
      bool SPMPacket = (trace.TraceID >= MIN_TRACE_ID_AIM && trace.TraceID <= MAX_TRACE_ID_AIM);
      bool SSPMPacket = (trace.TraceID >= MIN_TRACE_ID_ASM && trace.TraceID < MAX_TRACE_ID_ASM);
      if (!SAMPacket && !SPMPacket && !SSPMPacket) {
        continue;
      }

      if (SSPMPacket) {
        s = trace.TraceID - MIN_TRACE_ID_ASM;
        bool isSingle =    trace.EventFlags & 0x10;
        bool txEvent =     trace.EventFlags & 0x8;
        bool stallEvent =  trace.EventFlags & 0x4;
        bool starveEvent = trace.EventFlags & 0x2;
        bool isStart =     trace.EventFlags & 0x1;
        unsigned int ipInfo = mPluginHandle->getProfileSlotProperties(XCL_PERF_MON_STR, deviceName, s);
        bool isRead = (ipInfo & 0x2) ? true : false;
        if (isStart) {
          if (txEvent)
            mStreamTxStarts[s].push_back(timestamp);
          else if (starveEvent)
            mStreamStarveStarts[s].push_back(timestamp);
          else if (stallEvent)
            mStreamStallStarts[s].push_back(timestamp);
        } else {
          DeviceTrace streamTrace;
          streamTrace.Kind =  DeviceTrace::DEVICE_STREAM;
          if (txEvent) {
            if (isSingle || mStreamTxStarts[s].empty()) {
              startTime = timestamp;
            } else {
              startTime = mStreamTxStarts[s].front();
              mStreamTxStarts[s].pop_front();
            }
            streamTrace.Type = isRead ? "Stream_Read" : "Stream_Write";
          } else if (starveEvent) {
            if (mStreamStarveStarts[s].empty()) {
              startTime = timestamp;
            } else {
              startTime = mStreamStarveStarts[s].front();
              mStreamStarveStarts[s].pop_front();
            }
            streamTrace.Type = "Stream_Starve";
          } else if (stallEvent) {
            if (mStreamStallStarts[s].empty()) {
              startTime = timestamp;
            } else {
              startTime = mStreamStallStarts[s].front();
              mStreamStallStarts[s].pop_front();
            }
            streamTrace.Type = "Stream_Stall";
          }
          streamTrace.SlotNum = s;
          streamTrace.Name = isRead ? "Kernel_Stream_Read" : "Kernel_Stream_Write";
          streamTrace.StartTime = startTime;
          streamTrace.EndTime = timestamp;
          streamTrace.BurstLength = timestamp - startTime + 1;
          streamTrace.Start = convertDeviceToHostTimestamp(startTime, type, deviceName);
          streamTrace.End = convertDeviceToHostTimestamp(timestamp, type, deviceName);
          resultVector.push_back(streamTrace);
          mStreamMonLastTranx[s] = timestamp;
        } // !isStart
      } else if (SAMPacket) {
        s = ((trace.TraceID - MIN_TRACE_ID_AM) / 16);
        uint32_t cuEvent       = trace.TraceID & XAM_TRACE_CU_MASK;
        uint32_t stallIntEvent = trace.TraceID & XAM_TRACE_STALL_INT_MASK;
        uint32_t stallStrEvent = trace.TraceID & XAM_TRACE_STALL_STR_MASK;
        uint32_t stallExtEvent = trace.TraceID & XAM_TRACE_STALL_EXT_MASK;
        // Common Params for all event types
        kernelTrace.SlotNum = s;
        kernelTrace.Name = "OCL Region";
        kernelTrace.Kind = DeviceTrace::DEVICE_KERNEL;
        kernelTrace.EndTime = timestamp;
        kernelTrace.BurstLength = 0;
        kernelTrace.NumBytes = 0;
        kernelTrace.End = convertDeviceToHostTimestamp(timestamp, type, deviceName);
        if (cuEvent) {
          if (!(trace.EventFlags & XAM_TRACE_CU_MASK)) {
            kernelTrace.Type = "Kernel";
            if (!mAccelMonCuStarts[s].empty()) {
              startTime = mAccelMonCuStarts[s].front();
              mAccelMonCuStarts[s].pop_front();
              kernelTrace.StartTime = startTime;
              kernelTrace.Start = convertDeviceToHostTimestamp(startTime, type, deviceName);
              kernelTrace.TraceStart = kernelTrace.Start;
              kernelTrace.EventID = mCuEventID++;
              resultVector.insert(resultVector.begin(), kernelTrace);
            }
          }
          else {
            mAccelMonCuStarts[s].push_back(timestamp);
          }
        }
        if (stallIntEvent) {
          if (mAccelMonStartedEvents[s] & XAM_TRACE_STALL_INT_MASK) {
            kernelTrace.Type = "Intra-Kernel Dataflow Stall";
            startTime = mAccelMonStallIntTime[s];
            kernelTrace.StartTime = startTime;
            kernelTrace.Start = convertDeviceToHostTimestamp(startTime, type, deviceName);
            kernelTrace.TraceStart = kernelTrace.Start;
            resultVector.push_back(kernelTrace);
          }
          else {
            mAccelMonStallIntTime[s] = timestamp;
          }
        }
        if (stallStrEvent) {
          if (mAccelMonStartedEvents[s] & XAM_TRACE_STALL_STR_MASK) {
            kernelTrace.Type = "Inter-Kernel Pipe Stall";
            startTime = mAccelMonStallStrTime[s];
            kernelTrace.StartTime = startTime;
            kernelTrace.Start = convertDeviceToHostTimestamp(startTime, type, deviceName);
            kernelTrace.TraceStart = kernelTrace.Start;
            resultVector.push_back(kernelTrace);
          }
          else {
            mAccelMonStallStrTime[s] = timestamp;
          }
        }
        if (stallExtEvent) {
          if (mAccelMonStartedEvents[s] & XAM_TRACE_STALL_EXT_MASK) {
            kernelTrace.Type = "External Memory Stall";
            startTime = mAccelMonStallExtTime[s];
            kernelTrace.StartTime = startTime;
            kernelTrace.Start = convertDeviceToHostTimestamp(startTime, type, deviceName);
            kernelTrace.TraceStart = kernelTrace.Start;
            resultVector.push_back(kernelTrace);
          }
          else {
            mAccelMonStallExtTime[s] = timestamp;
          }
        }
        // Update Events
        mAccelMonStartedEvents[s] ^= (trace.TraceID & 0xf);
        mAccelMonLastTranx[s] = timestamp;
      } else if (IS_READ(trace.TraceID)) {         // SPM Read Trace
        s = trace.TraceID/2;
        if (trace.EventType == XCL_PERF_MON_START_EVENT) {
          mReadStarts[s].push_back(timestamp);
        }
        else if (trace.EventType == XCL_PERF_MON_END_EVENT) {
           if (trace.Reserved == 1) {
            startTime = timestamp;
           }
           else {
            if(mReadStarts[s].empty()) {
              startTime = timestamp;
            } else {
              startTime = mReadStarts[s].front();
              mReadStarts[s].pop_front();
            }
           }
          DeviceTrace readTrace;
          readTrace.SlotNum = s;
          readTrace.Type = "Read";
          readTrace.StartTime = startTime;
          readTrace.EndTime = timestamp;
          readTrace.BurstLength = timestamp - startTime + 1;
          readTrace.Start = convertDeviceToHostTimestamp(startTime, type, deviceName);
          readTrace.End = convertDeviceToHostTimestamp(timestamp, type, deviceName);
          resultVector.push_back(readTrace);
          mPerfMonLastTranx[s] = timestamp;
        }
      } else if (IS_WRITE(trace.TraceID)) {           // SPM Write Trace
        s = trace.TraceID/2;
        if (trace.EventType == XCL_PERF_MON_START_EVENT) {
          mWriteStarts[s].push_back(timestamp);
        }
        else if (trace.EventType == XCL_PERF_MON_END_EVENT) {
          if (trace.Reserved == 1) {
            startTime = timestamp;
          }
          else {
            if(mWriteStarts[s].empty()) {
              startTime = timestamp;
            } else {
              startTime = mWriteStarts[s].front();
              mWriteStarts[s].pop_front();
            }
          }
          DeviceTrace writeTrace;
          writeTrace.SlotNum = s;
          writeTrace.Type = "Write";
          writeTrace.StartTime = startTime;
          writeTrace.EndTime = timestamp;
          writeTrace.BurstLength = timestamp - startTime + 1;
          writeTrace.Start = convertDeviceToHostTimestamp(startTime, type, deviceName);
          writeTrace.End = convertDeviceToHostTimestamp(timestamp, type, deviceName);
          resultVector.push_back(writeTrace);
          mPerfMonLastTranx[s] = timestamp;
        }
      }
    } // for i
    XDP_LOG("[profile_device] Done logging device trace samples\n");
  }

  // Try to approximate CU Ends from residue state and reset
  // Only applicable to Device Trace State
  void TraceParser::endLogTrace(const std::string& deviceName, xclPerfMonType type, TraceResultVector& resultVector) {
    if (mPluginHandle->getFlowMode() == xdp::RTUtil::HW_EM)
      return;
    DeviceTrace kernelTrace;
    bool warning = false;
    unsigned int numCu = mPluginHandle->getProfileNumberSlots(XCL_PERF_MON_ACCEL, deviceName);
    for (unsigned int i = 0; i < numCu; i++) {
      if (!mAccelMonCuStarts[i].empty()) {
        kernelTrace.SlotNum = i;
        kernelTrace.Name = "OCL Region";
        kernelTrace.Type = "Kernel";
        kernelTrace.Kind = DeviceTrace::DEVICE_KERNEL;
        kernelTrace.StartTime = mAccelMonCuStarts[i].front();
        kernelTrace.Start = convertDeviceToHostTimestamp(kernelTrace.StartTime, type, deviceName);
        kernelTrace.BurstLength = 0;
        kernelTrace.NumBytes = 0;
        uint64_t lastTimeStamp = 0;
        std::string cu;
        mPluginHandle->getProfileSlotName(XCL_PERF_MON_ACCEL, deviceName, i, cu);
        // Check if any memory port on current CU had a trace packet
        unsigned int numMem = mPluginHandle->getProfileNumberSlots(XCL_PERF_MON_MEMORY, deviceName);
        for (unsigned int j = 0; j < numMem; j++) {
          std::string port;
          mPluginHandle->getProfileSlotName(XCL_PERF_MON_MEMORY, deviceName, j, port);
          auto found = port.find(cu);
          if (found != std::string::npos && lastTimeStamp < mPerfMonLastTranx[j])
            lastTimeStamp = mPerfMonLastTranx[j];
        }
        // Check if any streaming port on current CU had a trace packet
        unsigned int numStream = mPluginHandle->getProfileNumberSlots(XCL_PERF_MON_STR, deviceName);
        for (unsigned int j = 0; j < numStream; j++) {
          std::string port;
          mPluginHandle->getProfileSlotName(XCL_PERF_MON_STR, deviceName, j, port);
          auto found = port.find(cu);
          if (found != std::string::npos && lastTimeStamp < mStreamMonLastTranx[j])
            lastTimeStamp = mStreamMonLastTranx[j];
        }
        // Default case
        if (lastTimeStamp < mAccelMonLastTranx[i])
          lastTimeStamp = mAccelMonLastTranx[i];
        if (lastTimeStamp) {
          if (!warning) {
            mPluginHandle->sendMessage(
            "Incomplete CU profile trace detected. Timeline trace will have approximate CU End");
            warning = true;
          }
          kernelTrace.EndTime = lastTimeStamp;
          kernelTrace.End = convertDeviceToHostTimestamp(kernelTrace.EndTime, type, deviceName);
          kernelTrace.EventID = mCuEventID++;
          // Insert is needed in case there are only stalls
          resultVector.insert(resultVector.begin(), kernelTrace);
        }
      }
    }
    ResetState();
  }

  void TraceParser::logTraceHWEmu(const std::string& deviceName,
          xclTraceResultsVector& traceVector, TraceResultVector& resultVector) {
    if (mNumTraceEvents >= mMaxTraceEventsHwEm)
      return;
    XDP_LOG("[profile_device] Logging %u device trace samples (total = %ld)...\n",
        traceVector.mLength, mNumTraceEvents);
    mNumTraceEvents += traceVector.mLength;

    // Find and set minimum timestamp in case of multiple Kernels
    uint64_t minHostTimestampNsec = traceVector.mArray[0].HostTimestamp;
    for (unsigned int i=0; i < traceVector.mLength; i++) {
      if (traceVector.mArray[i].HostTimestamp < minHostTimestampNsec)
        minHostTimestampNsec = traceVector.mArray[i].HostTimestamp;
    }
    getTimestampNsec(minHostTimestampNsec);
    DeviceTrace kernelTrace;
    uint32_t prevHostTimestamp = 0xFFFFFFFF;
    uint32_t timestamp = 0;
    uint64_t hostTimestampNsec = 0;
    uint64_t mPrevTimestamp = 0;
    for (unsigned int i=0; i < traceVector.mLength; i++) {
      auto& trace = traceVector.mArray[i];
      XDP_LOG("[profile_device] Parsing trace sample %d...\n", i);
      timestamp = trace.Timestamp + mPrevTimestamp;
      mPrevTimestamp = timestamp;
      if (trace.HostTimestamp == prevHostTimestamp && trace.Timestamp == 1) {
        XDP_LOG("[profile_device] Ignoring host timestamp: 0x%X\n",
                trace.HostTimestamp);
        continue;
      }
      hostTimestampNsec = getTimestampNsec(trace.HostTimestamp);
      XDP_LOG("[profile_device] Timestamp pair: Device: 0x%X, Host: 0x%X\n",
              timestamp, hostTimestampNsec);
      prevHostTimestamp = trace.HostTimestamp;

      uint32_t s = 0;
      bool SPMPacket = (trace.TraceID < MAX_TRACE_ID_AIM);
      bool SAMPacket = (trace.TraceID >= MIN_TRACE_ID_AM && trace.TraceID <= MAX_TRACE_ID_AM_HWEM);
      bool SSPMPacket = (trace.TraceID >= MIN_TRACE_ID_ASM && trace.TraceID < MAX_TRACE_ID_ASM);

      if (SPMPacket) {
        uint8_t flags = 0;
        s = trace.TraceID / 2;
        flags = trace.EventFlags;
        XDP_LOG("[profile_device] slot %d event flags = %s @ timestamp %d\n",
              s, std::bitset<8>(flags).to_string().c_str(), timestamp);
        
        // Write start
        if (getBit(flags, XAPM_WRITE_FIRST)) {
          mWriteStarts[s].push_back(timestamp);
          mHostWriteStarts[s].push_back(hostTimestampNsec);
        }
  
        // Write end
        // NOTE: does not support out-of-order tranx
        if (getBit(flags, XAPM_WRITE_LAST)) {
          if (mWriteStarts[s].empty()) {
            XDP_LOG("[profile_device] WARNING: Found write end with write start queue empty @ %d\n", timestamp);
            continue;
          }

          uint64_t startTime = mWriteStarts[s].front();
          uint64_t hostStartTime = mHostWriteStarts[s].front();  
          mWriteStarts[s].pop_front();
          mHostWriteStarts[s].pop_front();
  
          // Add write trace class to vector
          DeviceTrace writeTrace;
          writeTrace.SlotNum = s;
          writeTrace.Type = "Write";
          writeTrace.StartTime = startTime;
          writeTrace.EndTime = timestamp;
          writeTrace.Start = hostStartTime / 1e6;
          writeTrace.End = hostTimestampNsec / 1e6;
          if (writeTrace.Start == writeTrace.End) writeTrace.End += mEmuTraceMsecOneCycle;
          writeTrace.BurstLength = timestamp - startTime + 1;
  
          // Only report tranx that make sense
          if (writeTrace.End >= writeTrace.Start) {
            writeTrace.TraceStart = hostStartTime / 1e6;
            resultVector.push_back(writeTrace);
          }
        }
  
        // Read start
        if (getBit(flags, XAPM_READ_FIRST)) {
          mReadStarts[s].push_back(timestamp);
          mHostReadStarts[s].push_back(hostTimestampNsec);
        }
  
        // Read end
        // NOTE: does not support out-of-order tranx
        if (getBit(flags, XAPM_READ_LAST)) {
          if (mReadStarts[s].empty()) {
            XDP_LOG("[profile_device] WARNING: Found read end with read start queue empty @ %d\n", timestamp);
            continue;
          }

          uint64_t startTime = mReadStarts[s].front();
          uint64_t hostStartTime = mHostReadStarts[s].front();
          mReadStarts[s].pop_front();
          mHostReadStarts[s].pop_front();
  
          // Add read trace class to vector
          DeviceTrace readTrace;
          readTrace.SlotNum = s;
          readTrace.Type = "Read";
          readTrace.StartTime = startTime;
          readTrace.EndTime = timestamp;
          readTrace.Start = hostStartTime / 1e6;
          readTrace.End = hostTimestampNsec / 1e6;
          // Single Burst
          if (readTrace.Start == readTrace.End) readTrace.End += mEmuTraceMsecOneCycle;
          readTrace.BurstLength = timestamp - startTime + 1;
  
          // Only report tranx that make sense
          if (readTrace.End >= readTrace.Start) {
            readTrace.TraceStart = hostStartTime / 1e6;
            resultVector.push_back(readTrace);
          }
        }
      }
      else if (SAMPacket) {
        uint32_t cuEvent = trace.EventFlags & XAM_TRACE_CU_MASK;
        s = trace.TraceID - 64;
        // Common Params for all event types
        kernelTrace.SlotNum = s;
        kernelTrace.Name = "OCL Region";
        kernelTrace.Kind = DeviceTrace::DEVICE_KERNEL;
        kernelTrace.EndTime = timestamp;
        kernelTrace.End = hostTimestampNsec / 1e6;
        kernelTrace.BurstLength = 0;
        kernelTrace.NumBytes = 0;
        if (cuEvent) {
          if (mAccelMonStartedEvents[s] & XAM_TRACE_CU_MASK) {
            kernelTrace.Type = "Kernel";
            kernelTrace.StartTime = mAccelMonCuTime[s];
            kernelTrace.Start = mAccelMonCuHostTime[s] / 1e6;
            kernelTrace.EventID = mCuEventID++;
            resultVector.push_back(kernelTrace);
            // Divide by 2 just to be safe
            mEmuTraceMsecOneCycle = (kernelTrace.End - kernelTrace.Start) / (2 *(kernelTrace.EndTime - kernelTrace.StartTime));
          }
          else {
            mAccelMonCuHostTime[s] = hostTimestampNsec;
            mAccelMonCuTime[s] = timestamp;
          }
          mAccelMonStartedEvents[s] ^= XAM_TRACE_CU_MASK;
        }
      }
      else if (SSPMPacket) {
        s = trace.TraceID - MIN_TRACE_ID_ASM;
        kernelTrace.Kind = DeviceTrace::DEVICE_STREAM;

        bool isSingle    = trace.EventFlags & 0x10;
        bool txEvent     = trace.EventFlags & 0x8;
        bool stallEvent  = trace.EventFlags & 0x4;
        bool starveEvent = trace.EventFlags & 0x2;
        bool isStart     = trace.EventFlags & 0x1;
        uint64_t startTime = 0;
        uint64_t hostStartTime = 0;

        unsigned int ipInfo = mPluginHandle->getProfileSlotProperties(XCL_PERF_MON_STR, deviceName, s);
        bool isRead     = (ipInfo & 0x2) ? true : false;
        if (isStart) {
          if (txEvent) {
            mStreamTxStarts[s].push_back(timestamp);
            mStreamTxStartsHostTime[s].push_back(hostTimestampNsec);
          } else if (starveEvent) {
            mStreamStarveStarts[s].push_back(timestamp);
            mStreamStarveStartsHostTime[s].push_back(hostTimestampNsec);
          } else if (stallEvent) {
            mStreamStallStarts[s].push_back(timestamp);
            mStreamStallStartsHostTime[s].push_back(hostTimestampNsec);
          }
        } else {
          if (txEvent) {
            if (isSingle || mStreamTxStarts[s].empty()) {
              startTime = timestamp;
              hostStartTime = hostTimestampNsec;
            } else {
              startTime = mStreamTxStarts[s].front();
              hostStartTime = mStreamTxStartsHostTime[s].front();
              mStreamTxStarts[s].pop_front();
              mStreamTxStartsHostTime[s].pop_front();
            }
            kernelTrace.Type = isRead ? "Stream_Read" : "Stream_Write";
          } else if (starveEvent) {
            if (mStreamStarveStarts[s].empty()) {
              startTime = timestamp;
              hostStartTime = hostTimestampNsec;
            } else {
              startTime = mStreamStarveStarts[s].front();
              hostStartTime = mStreamStarveStartsHostTime[s].front();
              mStreamStarveStarts[s].pop_front();
              mStreamStarveStartsHostTime[s].pop_front();
            }
            kernelTrace.Type = "Stream_Starve";
          } else if (stallEvent) {
            if (mStreamStallStarts[s].empty()) {
              startTime = timestamp;
              hostStartTime = hostTimestampNsec;
            } else {
              startTime = mStreamStallStarts[s].front();
              hostStartTime = mStreamStallStartsHostTime[s].front();
              mStreamStallStarts[s].pop_front();
              mStreamStallStartsHostTime[s].pop_front();
            }
            kernelTrace.Type = "Stream_Stall";
          }
          kernelTrace.SlotNum = s;
          kernelTrace.Name = isRead ? "Kernel_Stream_Read" : "Kernel_Stream_Write";
          kernelTrace.StartTime = startTime;
          kernelTrace.EndTime = timestamp;
          kernelTrace.BurstLength = timestamp - startTime + 1;
          kernelTrace.Start = hostStartTime / 1e6;
          kernelTrace.End = hostTimestampNsec / 1e6;
          resultVector.push_back(kernelTrace);
        }
      }
      else continue;
    }
    std::fill_n(mAccelMonStartedEvents,XAM_MAX_NUMBER_SLOTS,0);
    XDP_LOG("[profile_device] Done logging device trace samples\n");
  }

  // Complete training to convert device timestamp to host time domain
  // NOTE: see description of PTP @ http://en.wikipedia.org/wiki/Precision_Time_Protocol
  // clock training relation is linear within small durations (1 sec)
  // x, y coordinates are used for clock training
  void TraceParser::trainDeviceHostTimestamps(xclPerfMonType type, uint64_t deviceTimestamp, uint64_t hostTimestamp) {
    static double y1 = 0.0;
    static double y2 = 0.0;
    static double x1 = 0.0;
    static double x2 = 0.0;
    if (!y1 && !x1) {
      y1 = static_cast <double> (hostTimestamp);
      x1 = static_cast <double> (deviceTimestamp);
    } else {
      y2 = static_cast <double> (hostTimestamp);
      x2 = static_cast <double> (deviceTimestamp);
      mTrainSlope[type] = (y2 - y1) / (x2 - x1);
      mTrainOffset[type] = y2 - mTrainSlope[type] * x2;
      // next time update x1, y1
      y1 = 0.0;
      x1 = 0.0;
    }
  }

  // Convert device timestamp to host time domain (in msec)
  double TraceParser::convertDeviceToHostTimestamp(uint64_t deviceTimestamp, xclPerfMonType type,
      const std::string& deviceName) {
    // Return y = m*x + b
    return (mTrainSlope[type] * (double)deviceTimestamp + mTrainOffset[type])/1e6;
  }

} // xdp
