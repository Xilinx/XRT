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

//#include <CL/opencl.h>
#include "../../driver/include/xclperf.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <cassert>
#include <cstring>
#include <boost/format.hpp>

#include "xdp_profile.h"
#include "xdp_perf_counters.h"
#include "xdp_profile_results.h"
#include "xdp_profile_writers.h"

// Uncomment to use device-based timestamps in timeline trace
//#define USE_DEVICE_TIMELINE

namespace XDP {
  // ***********************
  // Top-Level Profile Class
  // ***********************
  XDPProfile::XDPProfile(int& flag)
  : ProfileFlags(flag),
    FileFlags(0),
    FlowMode(DEVICE),
    PerfCounters()
  {
    HostSlotIndex = XPAR_SPM0_HOST_SLOT;
    memset(&CUPortsToDDRBanks, 0, MAX_DDR_BANKS*sizeof(int));
  }

  XDPProfile::~XDPProfile()
  {
    if (ProfileFlags)
      writeProfileSummary();

    FinalCounterResultsMap.clear();
    RolloverCounterResultsMap.clear();
    RolloverCountsMap.clear();
  }

  // ***************************************************************************
  // Helper Functions
  // ***************************************************************************
  void XDPProfile::attach(WriterI* writer)
  {
    std::lock_guard < std::mutex > lock(LogMutex);
    auto itr = std::find(Writers.begin(), Writers.end(), writer);
    if (itr == Writers.end())
      Writers.push_back(writer);
  }

  void XDPProfile::detach(WriterI* writer)
  {
    std::lock_guard < std::mutex > lock(LogMutex);
    auto itr = std::find(Writers.begin(), Writers.end(), writer);
    if (itr != Writers.end())
      Writers.erase(itr);
  }

  uint32_t XDPProfile::getProfileNumberSlots(xclPerfMonType type,
      const std::string& deviceName) const
  {
	// TODO: for now, assume single device (ignore deviceName)
    auto iter = NumberSlotMap.find(type);
	if (iter != NumberSlotMap.end())
      return iter->second;
    return 0;
  }

  void XDPProfile::getFlowModeName(std::string& str) const
  {
    if (FlowMode == CPU)
      str = "CPU Emulation";
    else if (FlowMode == HW_EM)
      str = "Hardware Emulation";
    else
      str = "System Run";
  }

  void XDPProfile::commandKindToString(e_profile_command_kind objKind,
      std::string& commandString) const
  {
    switch (objKind) {
    case READ_BUFFER:
      commandString = "READ_BUFFER";
      break;
    case WRITE_BUFFER:
      commandString = "WRITE_BUFFER";
      break;
    case EXECUTE_KERNEL:
      commandString = "KERNEL";
      break;
    case DEVICE_KERNEL_READ:
      commandString = "KERNEL_READ";
      break;
    case DEVICE_KERNEL_WRITE:
      commandString = "KERNEL_WRITE";
      break;
    case DEVICE_KERNEL_EXECUTE:
      commandString = "KERNEL_EXECUTE";
      break;
    case DEVICE_BUFFER_READ:
      commandString = "READ_BUFFER_DEVICE";
      break;
    case DEVICE_BUFFER_WRITE:
      commandString = "WRITE_BUFFER_DEVICE";
      break;
    case DEPENDENCY_EVENT:
      commandString = "DEPENDENCY_EVENT";
      break;
    default:
      assert(0);
      break;
    }
  }

  void XDPProfile::commandStageToString(e_profile_command_state objStage,
      std::string& stageString) const
  {
    switch (objStage) {
    case QUEUE:
      stageString = "QUEUE";
      break;
    case SUBMIT:
      stageString = "SUBMIT";
      break;
    case START:
      stageString = "START";
      break;
    case END:
      stageString = "END";
      break;
    case COMPLETE:
      stageString = "COMPLETE";
      break;
    default:
      assert(0);
      break;
    }
  }

  void XDPProfile::getProfileSlotName(xclPerfMonType type, std::string& deviceName,
                                      unsigned slotnum, std::string& slotName) const
  {
    if (type == XCL_PERF_MON_ACCEL) {
      auto iter = SlotComputeUnitNameMap.find(slotnum);
      if (iter != SlotComputeUnitNameMap.end())
        slotName = iter->second;
    }

    if (type == XCL_PERF_MON_MEMORY) {
      auto iter = SlotComputeUnitPortNameMap.find(slotnum);
      if (iter != SlotComputeUnitPortNameMap.end())
        slotName = iter->second;
    }
  }

  void XDPProfile::getProfileKernelName(const std::string& deviceName, const std::string& cuName,
                                        std::string& kernelName) const
  {
    auto iter = ComputeUnitKernelNameMap.find(cuName);
    if (iter != ComputeUnitKernelNameMap.end())
      kernelName = iter->second;
  }

  // Set kernel clock freq on device
  void XDPProfile::setKernelClockFreqMHz(const std::string &deviceName, unsigned int kernelClockRateMHz) {
    DeviceKernelClockFreqMap[deviceName] = kernelClockRateMHz;
  }

  // Get kernel clock freq on device
  unsigned int XDPProfile::getKernelClockFreqMHz(std::string &deviceName) const
  {
    auto iter = DeviceKernelClockFreqMap.find(deviceName);
    if (iter != DeviceKernelClockFreqMap.end())
      return iter->second;
    return 300;
  }

  // Get device clock freq (in MHz)
  double XDPProfile::getDeviceClockFreqMHz() const
  {
    return 300.0;
  }

  // Get global memory clock freq (in MHz)
  double XDPProfile::getGlobalMemoryClockFreqMHz() const
  {
    return 300.0;
  }

  // Get global memory bit width
  uint32_t XDPProfile::getGlobalMemoryBitWidth() const
  {
    return XPAR_AXI_PERF_MON_0_SLOT0_DATA_WIDTH;
  }

  // Max. achievable bandwidth between kernels and DDR global memory = 60% of 10.7 GBps for PCIe Gen 3
  double XDPProfile::getGlobalMemoryMaxBandwidthMBps() const
  {
    double maxBandwidthMBps =
        0.6 * (getGlobalMemoryBitWidth() / 8) * getGlobalMemoryClockFreqMHz();
    return maxBandwidthMBps;
  }

  // Max. achievable read bandwidth between host and DDR global memory
  // TODO: this should be a call to the HAL function xclGetReadMaxBandwidthMBps()
  double XDPProfile::getReadMaxBandwidthMBps() const
  {
    return 9600.0;
  }

  // Max. achievable write bandwidth between host and DDR global memory
  // TODO: this should be a call to the HAL function xclGetWriteMaxBandwidthMBps()
  double XDPProfile::getWriteMaxBandwidthMBps() const
  {
    return 9600.0;
  }

  unsigned long
  XDPProfile::time_ns()
  {
    static auto zero = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    auto integral_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now-zero).count();
    return integral_duration;
  }

  double XDPProfile::getTimestampMsec(uint64_t timeNsec) {
    return (timeNsec / 1.0e6);
  }

  double XDPProfile::getTraceTime() {
    auto nsec = time_ns();
    return getTimestampMsec(nsec);
  }

  // Log device counters results
  void XDPProfile::logDeviceCounters(std::string deviceName, std::string binaryName, xclPerfMonType type,
      xclCounterResults& counterResults, uint64_t timeNsec, bool firstReadAfterProgram)
  {
    // Number of monitor slots
    uint32_t numSlots = 0;
    std::string key = deviceName + "|" + binaryName;
    std::string slotName = "";

    printf("logDeviceCounters: first read = %d, device: %s\n", firstReadAfterProgram, deviceName.c_str());

    // If not already defined, zero out rollover values for this device
    if (FinalCounterResultsMap.find(key) == FinalCounterResultsMap.end()) {
      FinalCounterResultsMap[key] = counterResults;

      xclCounterResults rolloverResults;
      memset(&rolloverResults, 0, sizeof(xclCounterResults));
      //rolloverResults.NumSlots = counterResults.NumSlots;
      RolloverCounterResultsMap[key] = rolloverResults;
      RolloverCountsMap[key] = rolloverResults;
    }
    else {
      /*
       * Log SPM Counters
       */
      numSlots = getProfileNumberSlots(XCL_PERF_MON_MEMORY, deviceName);
      // Traverse all monitor slots (host and all CU ports)
   	   bool deviceDataExists = (DeviceBinaryDataSlotsMap.find(key) == DeviceBinaryDataSlotsMap.end()) ? false : true;
       for (unsigned int s=0; s < numSlots; ++s) {
        getProfileSlotName(XCL_PERF_MON_MEMORY, deviceName, s, slotName);
        if (!deviceDataExists)
          DeviceBinaryDataSlotsMap[key].push_back(slotName);
        uint32_t prevWriteBytes   = FinalCounterResultsMap[key].WriteBytes[s];
        uint32_t prevReadBytes    = FinalCounterResultsMap[key].ReadBytes[s];
        uint32_t prevWriteTranx   = FinalCounterResultsMap[key].WriteTranx[s];
        uint32_t prevReadTranx    = FinalCounterResultsMap[key].ReadTranx[s];
        uint32_t prevWriteLatency = FinalCounterResultsMap[key].WriteLatency[s];
        uint32_t prevReadLatency  = FinalCounterResultsMap[key].ReadLatency[s];

        // Check for rollover of byte counters; if detected, add 2^32
        // Otherwise, if first read after program with binary, then capture bytes from previous xclbin
        if (!firstReadAfterProgram) {
          if (counterResults.WriteBytes[s] < prevWriteBytes)
            RolloverCountsMap[key].WriteBytes[s]    += 1;
          if (counterResults.ReadBytes[s] < prevReadBytes)
            RolloverCountsMap[key].ReadBytes[s]     += 1;
          if (counterResults.WriteTranx[s] < prevWriteTranx)
            RolloverCountsMap[key].WriteTranx[s]    += 1;
          if (counterResults.ReadTranx[s] < prevReadTranx)
            RolloverCountsMap[key].ReadTranx[s]     += 1;
          if (counterResults.WriteLatency[s] < prevWriteLatency)
            RolloverCountsMap[key].WriteLatency[s]  += 1;
          if (counterResults.ReadLatency[s] < prevReadLatency)
            RolloverCountsMap[key].ReadLatency[s]   += 1;
        }
        else {
          RolloverCounterResultsMap[key].WriteBytes[s]    += prevWriteBytes;
          RolloverCounterResultsMap[key].ReadBytes[s]     += prevReadBytes;
          RolloverCounterResultsMap[key].WriteTranx[s]    += prevWriteTranx;
          RolloverCounterResultsMap[key].ReadTranx[s]     += prevReadTranx;
          RolloverCounterResultsMap[key].WriteLatency[s]  += prevWriteLatency;
          RolloverCounterResultsMap[key].ReadLatency[s]   += prevReadLatency;
        }
   	  }
      /*
       * Log SAM Counters
       */     
      numSlots = getProfileNumberSlots(XCL_PERF_MON_ACCEL, deviceName);
      for (unsigned int s=0; s < numSlots; ++s) {
        uint32_t prevCuExecCount      = FinalCounterResultsMap[key].CuExecCount[s];
        uint32_t prevCuExecCycles     = FinalCounterResultsMap[key].CuExecCycles[s];
        uint32_t prevCuStallExtCycles = FinalCounterResultsMap[key].CuStallExtCycles[s];
        uint32_t prevCuStallIntCycles = FinalCounterResultsMap[key].CuStallIntCycles[s];
        uint32_t prevCuStallStrCycles = FinalCounterResultsMap[key].CuStallStrCycles[s];
        if (!firstReadAfterProgram) {
          if (counterResults.CuExecCycles[s] < prevCuExecCycles)
            RolloverCountsMap[key].CuExecCycles[s]     += 1;
          if (counterResults.CuStallExtCycles[s] < prevCuStallExtCycles)
            RolloverCountsMap[key].CuStallExtCycles[s] += 1;
          if (counterResults.CuStallIntCycles[s] < prevCuStallIntCycles)
            RolloverCountsMap[key].CuStallIntCycles[s] += 1;
          if (counterResults.CuStallStrCycles[s] < prevCuStallStrCycles)
            RolloverCountsMap[key].CuStallStrCycles[s] += 1;
        }
        else {
          RolloverCounterResultsMap[key].CuExecCount[s]      += prevCuExecCount;
          RolloverCounterResultsMap[key].CuExecCycles[s]     += prevCuExecCycles;
          RolloverCounterResultsMap[key].CuStallExtCycles[s] += prevCuStallExtCycles;
          RolloverCounterResultsMap[key].CuStallIntCycles[s] += prevCuStallIntCycles;
          RolloverCounterResultsMap[key].CuStallStrCycles[s] += prevCuStallStrCycles;
        }
      }
      FinalCounterResultsMap[key] = counterResults;
    }
    /*
     * Update Stats Database
     */
    uint32_t kernelClockMhz = getKernelClockFreqMHz(deviceName);
    double deviceCyclesMsec = kernelClockMhz * 1000.0 ;
    std::string cuName = "";
    std::string kernelName ="";
    bool deviceDataExists = (DeviceBinaryCuSlotsMap.find(key) == DeviceBinaryCuSlotsMap.end()) ? false : true;
    xclCounterResults rolloverResults = RolloverCounterResultsMap.at(key);
    xclCounterResults rolloverCounts = RolloverCountsMap.at(key);
    for (unsigned int s=0; s < numSlots; ++s) {
      getProfileSlotName(XCL_PERF_MON_ACCEL, deviceName, s, cuName);
      getProfileKernelName(deviceName, cuName, kernelName);
      if (!deviceDataExists)
        DeviceBinaryCuSlotsMap[key].push_back(cuName);
      uint32_t cuExecCount = counterResults.CuExecCount[s] + rolloverResults.CuExecCount[s];
      uint64_t cuExecCycles = counterResults.CuExecCycles[s] + rolloverResults.CuExecCycles[s]
                                + (rolloverCounts.CuExecCycles[s] * 4294967296UL);
      uint32_t cuMaxExecCycles  = counterResults.CuMaxExecCycles[s];
      uint32_t cuMinExecCycles  = counterResults.CuMinExecCycles[s];
      double cuRunTimeMsec = (double) cuExecCycles / deviceCyclesMsec;
      double cuMaxExecCyclesMsec = (double) cuMaxExecCycles / deviceCyclesMsec;
      double cuMinExecCyclesMsec = (double) cuMinExecCycles / deviceCyclesMsec;
      PerfCounters.logComputeUnitStats(cuName, kernelName, deviceName, cuRunTimeMsec, cuMaxExecCyclesMsec,
                                        cuMinExecCyclesMsec, cuExecCount, kernelClockMhz);
    }
  }

  void XDPProfile::writeTimelineTrace(double traceTime, const std::string& commandString,
            const std::string& stageString, const std::string& eventString,
            const std::string& dependString, size_t size, uint64_t address,
            const std::string& bank, std::thread::id threadId)
  {
    if(!this->isTimelineTraceFileOn())
      return;

    for (auto w : Writers) {
      w->writeTimeline(traceTime, commandString, stageString, eventString, dependString,
                       size, address, bank, threadId);
    }
  }

  void XDPProfile::logDataTransfer(uint64_t objId, const std::string& commandString,
            const std::string& stageString, const std::string& eventString,
            const std::string& dependString, size_t size, uint64_t address,
            const std::string& bank, std::thread::id threadId)
   {
     double timeStamp = getTraceTime();
     //double deviceTimeStamp = timeStamp; //getDeviceTimeStamp(timeStamp, deviceName);

     // Collect time trace
     BufferTrace* traceObject = nullptr;
     auto itr = BufferTraceMap.find(objId);
     if (itr == BufferTraceMap.end()) {
       traceObject = BufferTrace::reuse();
       BufferTraceMap[objId] = traceObject;
     }
     else {
       traceObject = itr->second;
     }

     if (stageString == "START")
       traceObject->Start= timeStamp;
     else
       traceObject->End= timeStamp;

     // clEnqueueNDRangeKernel returns END with no START
     // if data transfer was already completed.
     // We can safely discard those events
     if (stageString == "END" && (traceObject->getStart() > 0.0)) {
       // Collect performance counters
       if (commandString == "READ_BUFFER") {
         PerfCounters.logBufferRead(size, (traceObject->End - traceObject->Start), 0 /*contextId*/, 1 /*numDevices*/);
         PerfCounters.pushToSortedTopUsage(traceObject, true);
       }
       else if (commandString == "WRITE_BUFFER") {
         PerfCounters.logBufferWrite(size, (traceObject->End - traceObject->Start), 0 /*contextId*/, 1 /*numDevices*/);
         PerfCounters.pushToSortedTopUsage(traceObject, false);
       }
       else {
         assert(0);
       }

       // Mark and keep top trace data
       // Data can be additionally streamed to a data transfer record
       traceObject->Address = address;
       traceObject->Size = size;
       traceObject->ContextId = 0 /*contextId*/;
       traceObject->CommandQueueId = 0 /*commandQueueId*/;
       auto itr = BufferTraceMap.find(objId);
       BufferTraceMap.erase(itr);

       // Store thread IDs into set
       addToThreadIds(threadId);
     }

     //writeDataTransferTrace(timeStamp, commandString, stageString, eventString, dependString,
     //                   size, address, bank, threadId, ofs);
     writeTimelineTrace(timeStamp, commandString, stageString, eventString, dependString,
                        size, address, bank, threadId);
   }

  // Complete training to convert device timestamp to host time domain
  // NOTE: see description of PTP @ http://en.wikipedia.org/wiki/Precision_Time_Protocol
  void XDPProfile::trainDeviceHostTimestamps(std::string deviceName, xclPerfMonType type)
  {
    using namespace std::chrono;
    typedef duration<uint64_t, std::ratio<1, 1000000000>> duration_ns;
    duration_ns time_span =
        duration_cast<duration_ns>(high_resolution_clock::now().time_since_epoch());
    uint64_t currentOffset = static_cast<uint64_t>(time_ns());
    uint64_t currentTime = time_span.count();
    mTrainProgramStart[type] = static_cast<double>(currentTime - currentOffset);
  }

  // Convert device timestamp to host time domain (in msec)
  double XDPProfile::convertDeviceToHostTimestamp(uint64_t deviceTimestamp, xclPerfMonType type,
                                      const std::string& deviceName)
  {
    // Return y = m*x + b with b relative to program start
    return (mTrainSlope[type] * (double)deviceTimestamp)/1e6 + (mTrainOffset[type]-mTrainProgramStart[type])/1e6;
  }

  // Write current trace vector to timeline trace stream
  // NOTE: This function assumes a system run! (i.e., not HW emulation)
  void XDPProfile::logTrace(xclPerfMonType type, const std::string deviceName, std::string binaryName,
                xclTraceResultsVector& traceVector)
  {
    //printf("[logTrace] Logging %u device trace samples...\n", traceVector.mLength);

    // Log device trace results: store in queues and report events as they are completed
    //bool isHwEmu = false;
    //uint8_t flags = 0;
    //uint32_t prevHostTimestamp = 0xFFFFFFFF;
    uint32_t slotID = 0;
    uint32_t timestamp = 0;
    //uint64_t deviceStartTimestamp = 0;
    //uint64_t hostTimestampNsec = 0;
    uint64_t startTime = 0;
    double y1, y2, x1, x2;
    DeviceTrace kernelTrace;
    TraceResultVector resultVector;

    //
    // Parse recently offloaded trace results
    //
    for (unsigned int i=0; i < traceVector.mLength; i++) {
      xclTraceResults trace = traceVector.mArray[i];
      //printf("[logTrace] Parsing trace sample %d...\n", i);

      // ***************
      // Clock Training
      // ***************

      // for hw first two packets are for clock training
      // 1000 is to account for delay in sending from host
      // TODO: Calculate the delay instead of hard coding
      if (i == 0) {
        y1 = static_cast <double> (trace.HostTimestamp) + 1000;
        x1 = static_cast <double> (trace.Timestamp);
        continue;
      }
      if (i == 1) {
        y2 = static_cast <double> (trace.HostTimestamp) + 1000;
        x2 = static_cast <double> (trace.Timestamp);
        mTrainSlope[type] = (y2 - y1) / (x2 - x1);
        mTrainOffset[type] = y2 - mTrainSlope[type] * x2;
        trainDeviceHostTimestamps(deviceName, type);
      }

      if (trace.Overflow == 1)
        trace.Timestamp += LOOP_ADD_TIME_SPM;
      timestamp = trace.Timestamp;
      if (trace.TraceID >= 64 && trace.TraceID <= 544) {
        slotID = ((trace.TraceID - 64) / 16);
      }
      else {
        // SPM Trace IDs (Slots 0-30)
        if (trace.TraceID >= 2 && trace.TraceID <= 61)
          slotID = trace.TraceID/2;
        else
          // Unsupported
          continue;
      }
      uint32_t s = slotID;

      //
      // SAM Trace
      //
      if (trace.TraceID >= 64) {
        uint32_t cuEvent       = trace.TraceID & XSAM_TRACE_CU_MASK;
        uint32_t stallIntEvent = trace.TraceID & XSAM_TRACE_STALL_INT_MASK;
        uint32_t stallStrEvent = trace.TraceID & XSAM_TRACE_STALL_STR_MASK;
        uint32_t stallExtEvent = trace.TraceID & XSAM_TRACE_STALL_EXT_MASK;
        // Common Params for all event types
        kernelTrace.SlotNum = s;
        kernelTrace.Name = "OCL Region";
        kernelTrace.Kind = DeviceTrace::DEVICE_KERNEL;
        kernelTrace.EndTime = timestamp;
        kernelTrace.BurstLength = 0;
        kernelTrace.NumBytes = 0;
        kernelTrace.End = convertDeviceToHostTimestamp(timestamp, type, deviceName);
        if (cuEvent) {
          if (mAccelMonStartedEvents[s] & XSAM_TRACE_CU_MASK) {
            kernelTrace.Type = "Kernel";
            startTime = mAccelMonCuTime[s];
            kernelTrace.StartTime = startTime;
            kernelTrace.Start = convertDeviceToHostTimestamp(startTime, type, deviceName);
            kernelTrace.TraceStart = kernelTrace.Start;
            resultVector.insert(resultVector.begin(), kernelTrace);
          }
          else {
            mAccelMonCuTime[s] = timestamp;
          }
        }

        if (stallIntEvent) {
          if (mAccelMonStartedEvents[s] & XSAM_TRACE_STALL_INT_MASK) {
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
          if (mAccelMonStartedEvents[s] & XSAM_TRACE_STALL_STR_MASK) {
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
          if (mAccelMonStartedEvents[s] & XSAM_TRACE_STALL_EXT_MASK) {
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
      }
      //
      // SPM Trace (Read)
      //
      else if (IS_READ(trace.TraceID)) {
        if (trace.EventType == XCL_PERF_MON_START_EVENT) {
          mReadStarts[s].push(timestamp);
        }
        else if (trace.EventType == XCL_PERF_MON_END_EVENT) {
          if (trace.Reserved == 1) {
            startTime = timestamp;
          }
          else {
            if (mReadStarts[s].empty()) {
              startTime = timestamp;
            }
            else {
              startTime = mReadStarts[s].front();
              mReadStarts[s].pop();
            }
          }

          DeviceTrace readTrace;
          readTrace.SlotNum = slotID;
          readTrace.Type = "Read";
          readTrace.StartTime = startTime;
          readTrace.EndTime = timestamp;
          readTrace.BurstLength = timestamp - startTime + 1;
          readTrace.Start = convertDeviceToHostTimestamp(startTime, type, deviceName);
          readTrace.End = convertDeviceToHostTimestamp(timestamp, type, deviceName);
          resultVector.push_back(readTrace);
          mPerfMonLastTranx[slotID] = timestamp;
        }
      }
      //
      // SPM Trace (Write)
      //
      else if (IS_WRITE(trace.TraceID)) {
        if (trace.EventType == XCL_PERF_MON_START_EVENT) {
          mWriteStarts[s].push(timestamp);
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
              mWriteStarts[s].pop();
            }
          }

          DeviceTrace writeTrace;
          writeTrace.SlotNum = slotID;
          writeTrace.Type = "Write";
          writeTrace.StartTime = startTime;
          writeTrace.EndTime = timestamp;
          writeTrace.BurstLength = timestamp - startTime + 1;
          writeTrace.Start = convertDeviceToHostTimestamp(startTime, type, deviceName);
          writeTrace.End = convertDeviceToHostTimestamp(timestamp, type, deviceName);
          resultVector.push_back(writeTrace);
          mPerfMonLastTranx[slotID] = timestamp;
        }
      } // if SPM write
    } // for i

    // Try to approximate CU Ends from data transfers
    for (int i = 0; i < XSAM_MAX_NUMBER_SLOTS; i++) {
      if (mAccelMonStartedEvents[i] & XSAM_TRACE_CU_MASK) {
        kernelTrace.SlotNum = i;
        kernelTrace.Name = "OCL Region";
        kernelTrace.Type = "Kernel";
        kernelTrace.Kind = DeviceTrace::DEVICE_KERNEL;
        kernelTrace.StartTime = mAccelMonCuTime[i];
        kernelTrace.Start = convertDeviceToHostTimestamp(kernelTrace.StartTime, type, deviceName);
        kernelTrace.BurstLength = 0;
        kernelTrace.NumBytes = 0;
        uint64_t lastTimeStamp = 0;
        std::string cuNameSAM = mAccelNames[i];

        for (int j = 0; j < XSPM_MAX_NUMBER_SLOTS; j++) {
          std::string cuPortName = mAccelPortNames[j];
          std::string cuNameSPM = cuPortName.substr(0, cuPortName.find_first_of("/"));
          if (cuNameSAM == cuNameSPM && lastTimeStamp < mPerfMonLastTranx[j])
            lastTimeStamp = mPerfMonLastTranx[j];
        }

        if (lastTimeStamp < mAccelMonLastTranx[i])
          lastTimeStamp = mAccelMonLastTranx[i];
        if (lastTimeStamp) {
          printf("Incomplete CU profile trace detected. Timeline trace will have approximate CU End\n");
          kernelTrace.EndTime = lastTimeStamp;
          kernelTrace.End = convertDeviceToHostTimestamp(kernelTrace.EndTime, type, deviceName);
          // Insert is needed in case there are only stalls
          resultVector.insert(resultVector.begin(), kernelTrace);
        }
      }
    }

    // Clear vectors
    std::fill_n(mAccelMonStartedEvents,XSAM_MAX_NUMBER_SLOTS,0);
    mDeviceTrainVector.clear();
    mHostTrainVector.clear();

    // Write out results to timeline trace stream
    //writeTrace(resultVector, deviceName, binaryName, ofs);
    for (auto w : Writers) {
      w->writeDeviceTrace(this, resultVector, deviceName, binaryName);
    }
    resultVector.clear();
    printf("[logTrace] Done logging device trace samples\n");
  }

  // Get a device timestamp
  double XDPProfile::getDeviceTimeStamp(double hostTimeStamp, std::string& deviceName)
  {
    double deviceTimeStamp = hostTimeStamp;

    /*
    // In HW emulation, use estimated host timestamp based on device clock cycles (in psec from HAL)
    if (XCL::RTSingleton::Instance()->getFlowMode() == XCL::RTSingleton::HW_EM) {
      size_t dts = XCL::RTSingleton::Instance()->getDeviceTimestamp(deviceName);
      deviceTimeStamp = dts / 1000000.0;
    }
    */
    return deviceTimeStamp;
  }

  // ***************************************************************************
  // Writer Functions
  // ***************************************************************************
  void XDPProfile::writeKernelSummary(WriterI* writer) const
  {
    PerfCounters.writeKernelSummary(writer);
  }

  void XDPProfile::writeComputeUnitSummary(WriterI* writer) const
  {
    PerfCounters.writeComputeUnitSummary(writer);
  }

  void XDPProfile::writeHostTransferSummary(WriterI* writer) const
  {
    uint64_t totalReadBytes    = 0;
    uint64_t totalWriteBytes   = 0;
    uint64_t totalReadLatency  = 0;
    uint64_t totalWriteLatency = 0;
    double totalReadTimeMsec   = 0.0;
    double totalWriteTimeMsec  = 0.0;

    // Get total bytes and total time (currently derived from latency)
    // across all devices
    //
    // CR 951564: Use APM counters to calculate throughput (i.e., byte count and total time)
    // NOTE: for now, we only use this for writes (see PerformanceCounter::writeHostTransferSummary)
    auto iter = FinalCounterResultsMap.begin();
    for (; iter != FinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      std::string deviceName = key.substr(0, key.find_first_of("|"));
      if (!isDeviceActive(deviceName))
        continue;

      // Get results
      xclCounterResults counterResults = iter->second;

      xclCounterResults rolloverCounts;
      if (RolloverCountsMap.find(key) != RolloverCountsMap.end())
        rolloverCounts = RolloverCountsMap.at(key);
      else
        memset(&rolloverCounts, 0, sizeof(xclCounterResults));
      uint32_t  numHostSlots = getProfileNumberSlots(XCL_PERF_MON_HOST, deviceName);
      for (uint32_t s=HostSlotIndex; s < HostSlotIndex + numHostSlots; s++) {
        totalReadBytes += counterResults.ReadBytes[s]
                          + (rolloverCounts.ReadBytes[s] * 4294967296UL);
        totalWriteBytes += counterResults.WriteBytes[s]
                          + (rolloverCounts.WriteBytes[s] * 4294967296UL);
        // Total transfer time = sum of all tranx latencies
        // msec = cycles / (1000 * (Mcycles/sec))
        totalReadLatency += counterResults.ReadLatency[s]
                            + (rolloverCounts.ReadLatency[s] * 4294967296UL);
        totalWriteLatency += counterResults.WriteLatency[s]
                            + (rolloverCounts.WriteLatency[s] * 4294967296UL);
      }
    }
    totalReadTimeMsec = totalReadLatency / (1000.0 * getDeviceClockFreqMHz());
    totalWriteTimeMsec = totalWriteLatency / (1000.0 * getDeviceClockFreqMHz());

    // Get maximum throughput rates
    double readMaxBandwidthMBps = 0.0;
    double writeMaxBandwidthMBps = 0.0;
    if (getFlowMode() != CPU) {
      readMaxBandwidthMBps = getReadMaxBandwidthMBps();
      writeMaxBandwidthMBps = getWriteMaxBandwidthMBps();
    }

    PerfCounters.writeHostTransferSummary(writer, true,  totalReadBytes,  totalReadTimeMsec,  readMaxBandwidthMBps);
    PerfCounters.writeHostTransferSummary(writer, false, totalWriteBytes, totalWriteTimeMsec, writeMaxBandwidthMBps);
  }

  void XDPProfile::writeStallSummary(WriterI* writer) const
  {
    auto iter = FinalCounterResultsMap.begin();
    double deviceCyclesMsec = (getDeviceClockFreqMHz() * 1000.0);
    for (; iter != FinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      std::string deviceName = key.substr(0, key.find_first_of("|"));
      if (!isDeviceActive(deviceName) || (DeviceBinaryCuSlotsMap.find(key) == DeviceBinaryCuSlotsMap.end()))
        continue;

    // Get results
      xclCounterResults counterResults = iter->second;

      xclCounterResults rolloverResults;
      if (RolloverCounterResultsMap.find(key) != RolloverCounterResultsMap.end())
        rolloverResults = RolloverCounterResultsMap.at(key);
      else
        memset(&rolloverResults, 0, sizeof(xclCounterResults));

      xclCounterResults rolloverCounts;
      if (RolloverCountsMap.find(key) != RolloverCountsMap.end())
        rolloverCounts = RolloverCountsMap.at(key);
      else
        memset(&rolloverCounts, 0, sizeof(xclCounterResults));

      std::string cuName = "";

      uint32_t numSlots = DeviceBinaryCuSlotsMap.at(key).size();
      for (unsigned int s=0; s < numSlots; ++s) {
        cuName = DeviceBinaryCuSlotsMap.at(key)[s];
        uint32_t cuExecCount = counterResults.CuExecCount[s] + rolloverResults.CuExecCount[s];
        uint64_t cuExecCycles = counterResults.CuExecCycles[s] + rolloverResults.CuExecCycles[s]
                                  + (rolloverCounts.CuExecCycles[s] * 4294967296UL);
        uint64_t cuStallExtCycles = counterResults.CuStallExtCycles[s] + rolloverResults.CuStallExtCycles[s]
                                  + (rolloverCounts.CuStallExtCycles[s] * 4294967296UL);
        uint64_t cuStallStrCycles = counterResults.CuStallStrCycles[s] + rolloverResults.CuStallStrCycles[s]
                                  + (rolloverCounts.CuStallStrCycles[s] * 4294967296UL);
        uint64_t cuStallIntCycles = counterResults.CuStallIntCycles[s] + rolloverResults.CuStallIntCycles[s]
                                  + (rolloverCounts.CuStallIntCycles[s] * 4294967296UL);
        double cuRunTimeMsec = (double) cuExecCycles / deviceCyclesMsec;
        double cuStallExt =    (double) cuStallExtCycles / deviceCyclesMsec;
        double cuStallStr =    (double) cuStallStrCycles / deviceCyclesMsec;
        double cuStallInt =    (double) cuStallIntCycles / deviceCyclesMsec;
        writer->writeStallSummary(cuName, cuExecCount, cuRunTimeMsec,
                                  cuStallExt, cuStallStr, cuStallInt);
      }
    }
  }

  void XDPProfile::writeKernelTransferSummary(WriterI* writer) const
  {
    auto iter = FinalCounterResultsMap.begin();
    for (; iter != FinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      std::string deviceName = key.substr(0, key.find_first_of("|"));
      if (!isDeviceActive(deviceName) || (DeviceBinaryDataSlotsMap.find(key) == DeviceBinaryDataSlotsMap.end()))
        continue;

      // Get results
      xclCounterResults counterResults = iter->second;

      xclCounterResults rolloverResults;
      if (RolloverCounterResultsMap.find(key) != RolloverCounterResultsMap.end())
        rolloverResults = RolloverCounterResultsMap.at(key);
      else
        memset(&rolloverResults, 0, sizeof(xclCounterResults));

      xclCounterResults rolloverCounts;
      if (RolloverCountsMap.find(key) != RolloverCountsMap.end())
        rolloverCounts = RolloverCountsMap.at(key);
      else
        memset(&rolloverCounts, 0, sizeof(xclCounterResults));

      // Number of monitor slots
      uint32_t numSlots = DeviceBinaryDataSlotsMap.at(key).size();
      uint32_t numHostSlots = getProfileNumberSlots(XCL_PERF_MON_HOST, deviceName);

      // Total kernel time = sum of all kernel executions
      //double totalKernelTimeMsec = PerfCounters.getTotalKernelExecutionTime(deviceName);
      double maxTransferRateMBps = getGlobalMemoryMaxBandwidthMBps();

      unsigned int s = 0;
      if (HostSlotIndex == 0)
        s = numHostSlots;
      for (; s < numSlots; ++s) {
        if (s == HostSlotIndex)
          continue;

   	    std::string cuPortName = DeviceBinaryDataSlotsMap.at(key)[s];
        std::string cuName = cuPortName.substr(0, cuPortName.find_first_of("/"));
        std::string portName = cuPortName.substr(cuPortName.find_first_of("/")+1);
        std::transform(portName.begin(), portName.end(), portName.begin(), ::tolower);

        // TODO: currently we don't know the arguments or DDR bank;
        // in OpenCL, this was known by the runtime
        uint32_t ddrBank = 0;
        std::string argNames = "N/A";
        //getArgumentsBank(deviceName, cuName, portName, argNames, ddrBank);

        double totalCUTimeMsec = PerfCounters.getComputeUnitTotalTime(deviceName, cuName);

        uint64_t totalReadBytes    = counterResults.ReadBytes[s] + rolloverResults.ReadBytes[s]
                                     + (rolloverCounts.ReadBytes[s] * 4294967296UL);
        uint64_t totalWriteBytes   = counterResults.WriteBytes[s] + rolloverResults.WriteBytes[s]
                                     + (rolloverCounts.WriteBytes[s] * 4294967296UL);
        uint64_t totalReadTranx    = counterResults.ReadTranx[s] + rolloverResults.ReadTranx[s]
                                     + (rolloverCounts.ReadTranx[s] * 4294967296UL);
        uint64_t totalWriteTranx   = counterResults.WriteTranx[s] + rolloverResults.WriteTranx[s]
                                     + (rolloverCounts.WriteTranx[s] * 4294967296UL);

        // Total transfer time = sum of all tranx latencies
        // msec = cycles / (1000 * (Mcycles/sec))
        uint64_t totalReadLatency  = counterResults.ReadLatency[s] + rolloverResults.ReadLatency[s]
                                     + (rolloverCounts.ReadLatency[s] * 4294967296UL);
        double totalReadTimeMsec   = totalReadLatency / (1000.0 * getDeviceClockFreqMHz());
        uint64_t totalWriteLatency = counterResults.WriteLatency[s] + rolloverResults.WriteLatency[s]
                                     + (rolloverCounts.WriteLatency[s] * 4294967296UL);
        double totalWriteTimeMsec  = totalWriteLatency / (1000.0 * getDeviceClockFreqMHz());

        printf("writeKernelTransferSummary: s=%d, reads=%ld, writes=%ld, %s time = %f msec\n",
            s, totalReadTranx, totalWriteTranx, cuName.c_str(), totalCUTimeMsec);

        // First do READ, then WRITE
        if (totalReadTranx > 0) {
          PerfCounters.writeKernelTransferSummary(writer, deviceName, cuPortName, argNames, ddrBank,
        	  true,  totalReadBytes, totalReadTranx, totalCUTimeMsec, totalReadTimeMsec, maxTransferRateMBps);
        }
        if (totalWriteTranx > 0) {
          PerfCounters.writeKernelTransferSummary(writer, deviceName, cuPortName, argNames, ddrBank,
        	  false, totalWriteBytes, totalWriteTranx, totalCUTimeMsec, totalWriteTimeMsec, maxTransferRateMBps);
        }
      }
    }
  }
/*
  void XDPProfile::writeTopKernelSummary(WriterI* writer) const
  {
    PerfCounters.writeTopKernelSummary(writer);
  }
*/
  void XDPProfile::writeTopKernelTransferSummary(WriterI* writer) const
  {
    // Iterate over all devices
    auto iter = FinalCounterResultsMap.begin();
    for (; iter != FinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      std::string deviceName = key.substr(0, key.find_first_of("|"));
      if (!isDeviceActive(deviceName) || (DeviceBinaryDataSlotsMap.find(key) == DeviceBinaryDataSlotsMap.end()))
        continue;

      // Get results
      xclCounterResults counterResults = iter->second;

      xclCounterResults rolloverResults;
      if (RolloverCounterResultsMap.find(key) != RolloverCounterResultsMap.end())
        rolloverResults = RolloverCounterResultsMap.at(key);
      else
        memset(&rolloverResults, 0, sizeof(xclCounterResults));

      xclCounterResults rolloverCounts;
      if (RolloverCountsMap.find(key) != RolloverCountsMap.end())
        rolloverCounts = RolloverCountsMap.at(key);
      else
        memset(&rolloverCounts, 0, sizeof(xclCounterResults));

      // Number of monitor slots
      uint32_t numSlots = DeviceBinaryDataSlotsMap.at(key).size();
      uint32_t numHostSlots = getProfileNumberSlots(XCL_PERF_MON_HOST, deviceName);

      double maxTransferRateMBps = getGlobalMemoryMaxBandwidthMBps();

      //double totalReadTimeMsec  = PerfCounters.getTotalKernelExecutionTime(deviceName);
      //double totalWriteTimeMsec = totalReadTimeMsec;

      // Maximum bytes per AXI data transfer
      // NOTE: this assumes the entire global memory bit width with a burst of 256 (max burst length of AXI4)
      //       AXI standard also limits a transfer to 4K total bytes
      uint32_t maxBytesPerTransfer = (getGlobalMemoryBitWidth() / 8) * 256;
      if (maxBytesPerTransfer > 4096)
        maxBytesPerTransfer = 4096;

      // Gather unique names of monitored CUs on this device
      std::map<std::string, uint64_t> cuNameTranxMap;
      unsigned int s;
      if (HostSlotIndex == 0)
        s = numHostSlots;
      else 
        s = 0;
      for (; s < numSlots; ++s) {
        if (s == HostSlotIndex)
          continue;

        std::string cuPortName = DeviceBinaryDataSlotsMap.at(key)[s];
        std::string cuName = cuPortName.substr(0, cuPortName.find_first_of("/"));
        cuNameTranxMap[cuName] = 0;
      }

      // Get their total tranx counts
      auto cuIter = cuNameTranxMap.begin();
      for (; cuIter != cuNameTranxMap.end(); ++cuIter) {
        std::string cuName = cuIter->first;

        uint64_t totalReadTranx  = 0;
        uint64_t totalWriteTranx = 0;
        if (HostSlotIndex == 0)
          s = numHostSlots;
        else 
          s = 0;
        for (; s < numSlots; ++s) {
          if (s == HostSlotIndex)
            continue;

          std::string cuPortName = DeviceBinaryDataSlotsMap.at(key)[s];
          std::string cuSlotName = cuPortName.substr(0, cuPortName.find_first_of("/"));

          if (cuSlotName == cuName) {
            totalReadTranx  += counterResults.ReadTranx[s] + rolloverResults.ReadTranx[s]
                               + (rolloverCounts.ReadTranx[s] * 4294967296UL);
            totalWriteTranx += counterResults.WriteTranx[s] + rolloverResults.WriteTranx[s]
                               + (rolloverCounts.WriteTranx[s] * 4294967296UL);
          }
        }

        cuNameTranxMap[cuName] = (totalReadTranx + totalWriteTranx);
      }

      // Sort the CUs by their tranx count
      std::vector<std::pair<std::string, uint64_t>> cuPairs(cuNameTranxMap.begin(),
          cuNameTranxMap.end());
      std::sort(cuPairs.begin(), cuPairs.end(),
          [](const std::pair<std::string, uint64_t>& A, const std::pair<std::string, uint64_t>& B) {
               return (A.second > B.second);
             });

      // Now report them in order of total tranx counts
      for (const auto &pair : cuPairs) {
        std::string cuName = pair.first;

        uint64_t totalReadBytes  = 0;
        uint64_t totalWriteBytes = 0;
        uint64_t totalReadTranx  = 0;
        uint64_t totalWriteTranx = 0;
        if (HostSlotIndex == 0)
          s = numHostSlots;
        else 
          s = 0;
        for (; s < numSlots; ++s) {
          if (s == HostSlotIndex)
            continue;

          std::string cuPortName = DeviceBinaryDataSlotsMap.at(key)[s];
          std::string cuSlotName = cuPortName.substr(0, cuPortName.find_first_of("/"));

          if (cuSlotName == cuName) {
            totalReadBytes  += counterResults.ReadBytes[s] + rolloverResults.ReadBytes[s]
                               + (rolloverCounts.ReadBytes[s] * 4294967296UL);
            totalWriteBytes += counterResults.WriteBytes[s] + rolloverResults.WriteBytes[s]
                               + (rolloverCounts.WriteBytes[s] * 4294967296UL);
            totalReadTranx  += counterResults.ReadTranx[s] + rolloverResults.ReadTranx[s]
                               + (rolloverCounts.ReadTranx[s] * 4294967296UL);
            totalWriteTranx += counterResults.WriteTranx[s] + rolloverResults.WriteTranx[s]
                               + (rolloverCounts.WriteTranx[s] * 4294967296UL);
          }
        }

        double totalCUTimeMsec = PerfCounters.getComputeUnitTotalTime(deviceName, cuName);

        PerfCounters.writeTopKernelTransferSummary(writer, deviceName, cuName, totalWriteBytes,
            totalReadBytes, totalWriteTranx, totalReadTranx, totalCUTimeMsec, totalCUTimeMsec,
            maxBytesPerTransfer, maxTransferRateMBps);
      }
    }
  }

  void XDPProfile::writeDeviceTransferSummary(WriterI* writer) const
  {
    PerfCounters.writeDeviceTransferSummary(writer, true);
    PerfCounters.writeDeviceTransferSummary(writer, false);
  }

  void XDPProfile::writeTopDataTransferSummary(WriterI* writer, bool isRead) const
  {
    PerfCounters.writeTopDataTransferSummary(writer, isRead);
  }

  void XDPProfile::writeTopDeviceTransferSummary(WriterI* writer, bool isRead) const
  {
    PerfCounters.writeTopDeviceTransferSummary(writer, isRead);
  }

#if 0
  void XDPProfile::getProfileRuleCheckSummary()
  {
    RuleChecks->getProfileRuleCheckSummary(this);
  }

  void XDPProfile::writeProfileRuleCheckSummary(WriterI* writer)
  {
    RuleChecks->writeProfileRuleCheckSummary(writer, this);
  }
#endif

  void XDPProfile::writeProfileSummary()
  {
    if (!this->isApplicationProfileOn())
      return;

    for (auto w : Writers) {
      w->writeSummary(this);
    }
  }
}

