/*
 * Copyright (C) 2018, Xilinx Inc - All rights reserved
 * Xilinx SDAccel Media Accelerator API
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
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <set>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <vector>
#include <queue>
#include <thread>
#include <boost/format.hpp>

#include "xperf.h"

// *****************************************************************************
//                               Helper Functions
// *****************************************************************************

//
// XDP: Helpers, classes, & members
//

// TODO: replace these with functions in XDP library
namespace XDP {

  unsigned long
  time_ns()
  {
    static auto zero = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    auto integral_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now-zero).count();
    return integral_duration;
  }

  double getTimestampMsec(uint64_t timeNsec) {
    return (timeNsec / 1.0e6);
  }

  double getTraceTime() {
    auto nsec = XDP::time_ns();
    return getTimestampMsec(nsec);
  }

  // Classes
  // Class to store time trace of kernel execution, buffer read, or buffer write
  // Timestamp is in double precision value of unit ms
  class TimeTrace {
  public:
    TimeTrace()
      : ContextId( 0 )
      , CommandQueueId( 0 )
      , Queue( 0.0 )
      , Submit( 0.0 )
      , Start( 0.0 )
      , End( 0.0 )
      , Complete( 0.0 )
      {}
    ~TimeTrace() {};
  public:
    double getDuration() const { return End - Start; }
    uint32_t getContextId() const { return ContextId; }
    uint32_t getCommandQueueId() const { return CommandQueueId; }
    double getQueue() const { return Queue; }
    double getSubmit() const { return Submit; }
    double getStart() const { return Start; }
    double getEnd() const { return End; }
    double getComplete() const { return Complete; }

  protected:
    void resetTimeStamps()
    {
      Queue = 0.0;
      Submit = 0.0;
      Start = 0.0;
      End = 0.0;
      Complete = 0.0;
    }

    //virtual void write(WriterI* writer) const = 0;
  public:
    // Ids
    uint32_t ContextId;
    uint32_t CommandQueueId;

    // Timestamps
    double Queue;
    double Submit;
    double Start;
    double End;
    double Complete;
  };

  class DeviceTrace: public TimeTrace {
  public:
    DeviceTrace()
      : Next(nullptr)
      , Size(0)
      {}
    ~DeviceTrace() {};
  public:
    static DeviceTrace* reuse();
    static void recycle(DeviceTrace* object);
  public:
    size_t getSize() const { return Size; }
  //public:
  //  void write(WriterI* writer) const override;
  public:
    DeviceTrace* Next;
    size_t Size;
    // Singly linked list to pool and recycle the DeviceTrace objects
    static DeviceTrace* RecycleHead;

    enum e_device_kind {
      DEVICE_KERNEL = 0x1,
      DEVICE_BUFFER = 0x2
    };

    std::string Name;
    std::string DeviceName;
    std::string Type;
    e_device_kind Kind = DEVICE_KERNEL;
    uint16_t SlotNum = 0;
    uint16_t BurstLength = 0;
    uint16_t NumBytes = 0;
    uint64_t StartTime = 0;
    uint64_t EndTime = 0;
    double TraceStart = 0.0;
  };

  typedef std::vector<DeviceTrace> TraceResultVector;

  // Helper functions
  const char * getToolVersion() { return "2018.2"; }
  std::string getCurrentDateTime();
  std::string getCurrentTimeMsec();
  std::string getCurrentExecutableName();

  void writeProfileHeader(std::ofstream& ofs);
  void writeProfileFooter(std::ofstream& ofs);
  void writeProfileSummary(std::ofstream& ofs);

  void writeTraceHeader(std::ofstream& ofs);
  void writeTraceFooter(xclDeviceInfo2 deviceInfo, std::ofstream& ofs);
  void trainDeviceHostTimestamps(std::string deviceName, xclPerfMonType type);
  double convertDeviceToHostTimestamp(uint64_t deviceTimestamp, xclPerfMonType type,
                                      const std::string& deviceName);
  void logTrace(xclPerfMonType type, const std::string deviceName, std::string binaryName,
                xclTraceResultsVector& traceVector, std::ofstream& ofs);
  void writeTrace(const TraceResultVector &resultVector, std::string deviceName,
                  std::string binaryName, std::ofstream& ofs);

  const char* cellStart() { return ""; }
  const char* cellEnd() { return ","; }
  void writeTableRowStart(std::ofstream& ofs) { ofs << "";}
  void writeTableRowEnd(std::ofstream& ofs) { ofs << "\n";}
  // Veraidic args function to take n number of any type of args and
  // stream it to a file
  // Move it to base class as new writer functionality needs it.
  // TODO: Windows doesnt support variadic functions till VS 2013.
  template<typename T>
  void writeTableCells(std::ofstream& ofs, T value)
  {
    ofs << cellStart();
    ofs << value;
    ofs << cellEnd();
  }
  template<typename T, typename... Args>
  void writeTableCells(std::ofstream& ofs, T first, Args... args)
  {
    writeTableCells(ofs, first);
    writeTableCells(ofs, args...);
  }

  // Members
  std::string deviceName;
  unsigned short mKernelClockFreq;
  std::string mDataTransferTrace;
  std::string mStallTrace;
  std::ofstream mProfileStream;
  std::ofstream mTraceStream;
  xclCounterResults mProfileResults;

  std::string mAccelNames[XSAM_MAX_NUMBER_SLOTS];
  std::string mAccelPortNames[XSPM_MAX_NUMBER_SLOTS];

  double mTrainSlope[XCL_PERF_MON_TOTAL_PROFILE];
  double mTrainOffset[XCL_PERF_MON_TOTAL_PROFILE];
  double mTrainProgramStart[XCL_PERF_MON_TOTAL_PROFILE];
  uint32_t mPrevTimestamp[XCL_PERF_MON_TOTAL_PROFILE];
  uint64_t mAccelMonCuTime[XSAM_MAX_NUMBER_SLOTS]       = { 0 };
  uint64_t mAccelMonCuHostTime[XSAM_MAX_NUMBER_SLOTS]   = { 0 };
  uint64_t mAccelMonStallIntTime[XSAM_MAX_NUMBER_SLOTS] = { 0 };
  uint64_t mAccelMonStallStrTime[XSAM_MAX_NUMBER_SLOTS] = { 0 };
  uint64_t mAccelMonStallExtTime[XSAM_MAX_NUMBER_SLOTS] = { 0 };
  uint8_t mAccelMonStartedEvents[XSAM_MAX_NUMBER_SLOTS] = { 0 };
  uint64_t mPerfMonLastTranx[XSPM_MAX_NUMBER_SLOTS]     = { 0 };
  uint64_t mAccelMonLastTranx[XSAM_MAX_NUMBER_SLOTS]    = { 0 };
  std::set<std::string> mDeviceFirstTimestamp;
  std::vector<uint32_t> mDeviceTrainVector;
  std::vector<uint64_t> mHostTrainVector;
  std::queue<uint64_t> mWriteStarts[XSPM_MAX_NUMBER_SLOTS];
  std::queue<uint64_t> mHostWriteStarts[XSPM_MAX_NUMBER_SLOTS];
  std::queue<uint64_t> mReadStarts[XSPM_MAX_NUMBER_SLOTS];
  std::queue<uint64_t> mHostReadStarts[XSPM_MAX_NUMBER_SLOTS];
  std::queue<uint32_t> mWriteLengths[XSPM_MAX_NUMBER_SLOTS];
  std::queue<uint32_t> mReadLengths[XSPM_MAX_NUMBER_SLOTS];
  std::queue<uint16_t> mWriteBytes[XSPM_MAX_NUMBER_SLOTS];
  std::queue<uint16_t> mReadBytes[XSPM_MAX_NUMBER_SLOTS];

  //
  // Device Trace
  //
  DeviceTrace* DeviceTrace::reuse()
  {
    if (RecycleHead) {
      DeviceTrace* element = RecycleHead;
      RecycleHead = RecycleHead->Next;
      return element;
    }
    else {
      return new DeviceTrace();
    }
  }

  // Return back the object to be reused later
  void DeviceTrace::recycle(DeviceTrace* object)
  {
    object->resetTimeStamps();
    object->Next = RecycleHead ? RecycleHead : nullptr;
    RecycleHead = object;
  }

  //void DeviceTrace::write(WriterI* writer) const
  //{
  //  writer->writeSummary(*this);
  //}

  // Get current date & time (format: YYYY-MM-DD HH:mm:SS)
  std::string getCurrentDateTime()
  {
    auto time = std::chrono::system_clock::to_time_t( std::chrono::system_clock::now() );
    struct tm tstruct = *(std::localtime(&time));
    char buf[80];
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
    return std::string(buf);
  }

  // Get current time (in msec)
  std::string getCurrentTimeMsec()
  {
    struct timespec now;
    int err;
    if ((err = clock_gettime(CLOCK_REALTIME, &now)) < 0)
      return "0";

    uint64_t nsec = (uint64_t) now.tv_sec * 1000000000UL + (uint64_t) now.tv_nsec;
    uint64_t msec = nsec / 1e6;
    return std::to_string(msec);
  }

  // Get current host executable name
  // TODO: does this really give the user host executable in this context?
  std::string getCurrentExecutableName()
  {
    std::string execName("");
#if defined(__linux__) && defined(__x86_64__)
    const int maxLength = 1024;
    char buf[maxLength];
    ssize_t len;
    if ((len=readlink("/proc/self/exe", buf, maxLength-1)) != -1) {
      buf[len]= '\0';
      execName = buf;
    }

    // Remove directory path and only keep filename
    const size_t lastSlash = execName.find_last_of("\\/");
    if (lastSlash != std::string::npos)
      execName.erase(0, lastSlash + 1);
#endif
    return execName;
  }

  // Write header to profile summary stream
  void writeProfileHeader(std::ofstream& ofs)
  {
    if (!ofs.is_open())
      return;

    ofs << "XMA Profile Summary" << std::endl;
    ofs << "Generated on: " << XDP::getCurrentDateTime() << "\n";
    ofs << "Msec since Epoch: " << XDP::getCurrentTimeMsec() << "\n";
    if (!XDP::getCurrentExecutableName().empty())
      ofs << "Profiled application: " << XDP::getCurrentExecutableName() << "\n";
    ofs << "Target platform: Xilinx" << std::endl;
    ofs << "Tool version: " << getToolVersion() << std::endl;
    ofs << std::endl;
  }

  // Write footer to profile summary stream
  void writeProfileFooter(std::ofstream& ofs)
  {
    if (!ofs.is_open())
      return;
    ofs << "\n";
  }

  // Write summary tables to profile summary stream
  void writeProfileSummary(std::ofstream& ofs)
  {
    if (!ofs.is_open())
      return;

    // TODO: write profile summary!!
  }

  // Write header to timeline trace stream
  void writeTraceHeader(std::ofstream& ofs)
  {
    if (!ofs.is_open())
      return;

    ofs << "SDAccel Timeline Trace" << std::endl;
    ofs << "Generated on: " << XDP::getCurrentDateTime() << "\n";
    ofs << "Msec since Epoch: " << XDP::getCurrentTimeMsec() << "\n";
    if (!XDP::getCurrentExecutableName().empty())
      ofs << "Profiled application: " << XDP::getCurrentExecutableName() << "\n";
    ofs << "Target platform: Xilinx" << std::endl;
    ofs << "Tool version: " << getToolVersion() << std::endl;
    ofs << std::endl;
    ofs << "Time_msec,Name,Event,Address_Port,Size,Latency_cycles,Start_cycles,End_cycles,Latency_usec,Start_msec,End_msec," << std::endl;
  }

  // Write footer to timeline trace stream
  void writeTraceFooter(xclDeviceInfo2 deviceInfo, std::ofstream& ofs)
  {
    if (!ofs.is_open())
      return;

    ofs << "Footer,begin\n";

    //
    // Settings (project name, stalls, target, & platform)
    //
    //std::string projectName = profile->getProjectName();
    //ofs << "Project," << projectName << ",\n";

    std::string stallProfiling = (mStallTrace == "off") ? "false" : "true";
    ofs << "Stall profiling," << stallProfiling << ",\n";

    std::string flowMode = "System Run";
    ofs << "Target," << flowMode << ",\n";

    // Platform/device info
    std::string deviceName = deviceInfo.mName;
    ofs << "Platform," << XDP::deviceName << ",\n";
    ofs << "Device," << XDP::deviceName << ",begin\n";

    // DDR Bank addresses
    // TODO: this assumes start address of 0x0 and evenly divided banks
    unsigned ddrBanks = deviceInfo.mDDRBankCount;
    if (ddrBanks == 0) ddrBanks = 1;
    size_t ddrSize = deviceInfo.mDDRSize;
    size_t bankSize = ddrSize / ddrBanks;
    ofs << "DDR Banks,begin\n";
    for (int b=0; b < ddrBanks; ++b)
      ofs << "Bank," << std::dec << b << ",0X" << std::hex << (b * bankSize) << std::endl;
    ofs << "DDR Banks,end\n";
    ofs << "Device," << XDP::deviceName << ",end\n";

    // TODO: Unused CUs

    ofs << "Footer,end\n";
  }

  // Complete training to convert device timestamp to host time domain
  // NOTE: see description of PTP @ http://en.wikipedia.org/wiki/Precision_Time_Protocol
  void trainDeviceHostTimestamps(std::string deviceName, xclPerfMonType type)
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
  double convertDeviceToHostTimestamp(uint64_t deviceTimestamp, xclPerfMonType type,
                                      const std::string& deviceName)
  {
    // Return y = m*x + b with b relative to program start
    return (mTrainSlope[type] * (double)deviceTimestamp)/1e6 + (mTrainOffset[type]-mTrainProgramStart[type])/1e6;
  }

  // Write current trace vector to timeline trace stream
  // NOTE: This function assumes a system run! (i.e., not HW emulation)
  void logTrace(xclPerfMonType type, const std::string deviceName, std::string binaryName,
                xclTraceResultsVector& traceVector, std::ofstream& ofs)
  {
    //printf("[logTrace] Logging %u device trace samples...\n", traceVector.mLength);

    // Log device trace results: store in queues and report events as they are completed
    bool isHwEmu = false;
    uint8_t flags = 0;
    uint32_t prevHostTimestamp = 0xFFFFFFFF;
    uint32_t slotID = 0;
    uint32_t timestamp = 0;
    uint64_t deviceStartTimestamp = 0;
    uint64_t hostTimestampNsec = 0;
    uint64_t startTime = 0;
    double y1, y2, x1, x2;
    DeviceTrace kernelTrace;
    TraceResultVector resultVector;

    //
    // Parse recently offloaded trace results
    //
    for (int i=0; i < traceVector.mLength; i++) {
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
    writeTrace(resultVector, deviceName, binaryName, ofs);
    resultVector.clear();
    printf("[logTrace] Done logging device trace samples\n");
  }

  void writeTrace(const TraceResultVector &resultVector, std::string deviceName,
                  std::string binaryName, std::ofstream& ofs)
  {
    if (!ofs.is_open())
      return;

    for (auto it = resultVector.begin(); it != resultVector.end(); it++) {
      DeviceTrace tr = *it;

#ifndef XDP_VERBOSE
      if (tr.Kind == DeviceTrace::DEVICE_BUFFER)
        continue;
#endif

      //auto rts = XCL::RTSingleton::Instance();
      //double deviceClockDurationUsec = (1.0 / (rts->getProfileManager()->getKernelClockFreqMHz(deviceName)));
      double deviceClockDurationUsec = (1.0 / mKernelClockFreq);

      std::stringstream startStr;
      startStr << std::setprecision(10) << tr.Start;
      std::stringstream endStr;
      endStr << std::setprecision(10) << tr.End;

      bool showKernelCUNames = true;
      bool showPortName = false;
      uint32_t ddrBank;
      std::string traceName;
      std::string cuName;
      std::string argNames;

      // Populate trace name string
      if (tr.Kind == DeviceTrace::DEVICE_KERNEL) {
        if (tr.Type == "Kernel") {
          traceName = "KERNEL";
        } else if (tr.Type.find("Stall") != std::string::npos) {
          traceName = "Kernel_Stall";
          showPortName = false;
        } else if (tr.Type == "Write") {
          showPortName = true;
          traceName = "Kernel_Write";
        } else {
          showPortName = true;
          traceName = "Kernel_Read";
        }
      }
      else {
        showKernelCUNames = false;
        if (tr.Type == "Write")
          traceName = "Host_Write";
        else
          traceName = "Host_Read";
      }

      traceName += ("|" + deviceName + "|" + binaryName);

      if (showKernelCUNames || showPortName) {
        std::string portName;
        std::string cuPortName;
        if (tr.Kind == DeviceTrace::DEVICE_KERNEL && (tr.Type == "Kernel" || tr.Type.find("Stall") != std::string::npos)) {
       	  cuName = mAccelNames[tr.SlotNum];
        }
        else {
       	  cuPortName = mAccelPortNames[tr.SlotNum];
          cuName = cuPortName.substr(0, cuPortName.find_first_of("/"));
          portName = cuPortName.substr(cuPortName.find_first_of("/")+1);
          std::transform(portName.begin(), portName.end(), portName.begin(), ::tolower);
        }
        // TODO: get kernel name
        std::string kernelName = "kernel";
        //XCL::RTSingleton::Instance()->getProfileKernelName(deviceName, cuName, kernelName);

        if (showKernelCUNames)
          traceName += ("|" + kernelName + "|" + cuName);

        if (showPortName) {
          // TODO: get arguments and DDR bank
          argNames = "a|b|c";
          ddrBank = 0;
          //rts->getProfileManager()->getArgumentsBank(deviceName, cuName, portName, argNames, ddrBank);
          traceName += ("|" + portName + "|" + std::to_string(ddrBank));
        }
      }

      if (tr.Type == "Kernel") {
        std::string workGroupSize;
        // TODO: get trace string (we don't know the CU name or the work group size)
        traceName = "KERNEL|" + deviceName + "|" + binaryName + "|" + "kernel" + "|1:1:1|" + cuName ;
        //rts->getProfileManager()->getTraceStringFromComputeUnit(deviceName, cuName, traceName);
        if (traceName.empty()) continue;

        //size_t pos = traceName.find_last_of("|");
        //workGroupSize = traceName.substr(pos + 1);
        //traceName = traceName.substr(0, pos);
        workGroupSize = "1";

        writeTableRowStart(ofs);
        writeTableCells(ofs, startStr.str(), traceName, "START", "", workGroupSize);
        writeTableRowEnd(ofs);

        writeTableRowStart(ofs);
        writeTableCells(ofs, endStr.str(), traceName, "END", "", workGroupSize);
        writeTableRowEnd(ofs);
        continue;
      }

      double deviceDuration = 1000.0*(tr.End - tr.Start);
      if (!(deviceDuration > 0.0)) deviceDuration = deviceClockDurationUsec;
      writeTableRowStart(ofs);
      writeTableCells(ofs, startStr.str(), traceName,
          tr.Type, argNames, tr.BurstLength, (tr.EndTime - tr.StartTime),
          tr.StartTime, tr.EndTime, deviceDuration,
          startStr.str(), endStr.str());
      writeTableRowEnd(ofs);
    }
  }


  // Get a device timestamp
  double getDeviceTimeStamp(double hostTimeStamp, std::string& deviceName)
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


  void writeDataTransferTrace(double traceTime, const std::string& commandString,
            const std::string& stageString, const std::string& eventString,
            const std::string& dependString, size_t size, uint64_t address,
            const std::string& bank, std::thread::id threadId,  std::ofstream& ofs)
  {
    if (!ofs.is_open())
      return;

    std::stringstream timeStr;
    timeStr << std::setprecision(10) << traceTime;

    // Write out DDR physical address and bank
    // NOTE: thread ID is only valid for START and END
    std::stringstream strAddress;
    strAddress << (boost::format("0X%09x") % address) << "|" << std::dec << bank;
    //strAddress << std::showbase << std::hex << std::uppercase << address
    //		   << "|" << std::dec << bank;
    if (stageString == "START" || stageString == "END")
      strAddress << "|" << std::showbase << std::hex << std::uppercase << threadId;

    writeTableRowStart(ofs);
  #ifndef _WINDOWS
    // TODO: Windows build support
    //    Variadic Template is not supported
    writeTableCells(ofs, timeStr.str(), commandString,
        stageString, strAddress.str(), size, "", "", "", "", "", "",
        eventString, dependString);
  #endif
    writeTableRowEnd(ofs);
  }

  void logDataTransfer(uint64_t objId, const std::string commandString,
      const std::string stageString, size_t objSize, uint32_t contextId,
      uint32_t numDevices, std::string deviceName, uint32_t commandQueueId,
      uint64_t address, const std::string& bank, std::thread::id threadId,
      const std::string eventString, const std::string dependString, double timestampMsec, std::ofstream& ofs)
  {
    double timeStamp = (timestampMsec > 0.0) ? timestampMsec : getTraceTime();
    double deviceTimeStamp = getDeviceTimeStamp(timeStamp, deviceName);
#ifdef USE_DEVICE_TIMELINE
    timeStamp = deviceTimeStamp;
#endif

    std::cout << "logDataTransfer: " << commandString << std::endl;
    writeDataTransferTrace(timeStamp, commandString, stageString, eventString, dependString,
                       objSize, address, bank, threadId, ofs);
  }
} // XDP namespace
XDP::DeviceTrace* XDP::DeviceTrace::RecycleHead = nullptr;

// *****************************************************************************
//                              Profile Counters
// *****************************************************************************

void
profile_start_summary(xclDeviceHandle s_handle)
{
  xclDeviceHandle dev_handle = s_handle;
  printf("xma_plg_start_profile: dev_handle=%p\n", dev_handle);

  // Start counters
  xclPerfMonStartCounters(dev_handle, XCL_PERF_MON_MEMORY);

  // Open profile summary file
  XDP::mProfileStream.open("xma_profile_summary.csv");

  // Write header
  XDP::writeProfileHeader(XDP::mProfileStream);
}

void
profile_read_summary(xclDeviceHandle s_handle)
{
  xclDeviceHandle dev_handle = s_handle;
  printf("xma_plg_read_profile: dev_handle=%p\n", dev_handle);

  // Read counters
  xclCounterResults counterResults;
  xclPerfMonReadCounters(dev_handle, XCL_PERF_MON_MEMORY, counterResults);

  // Store results
  // TODO: keep track of multiple calls to read and check for overflows
  XDP::mProfileResults = counterResults;
}

void
profile_end_summary(xclDeviceHandle s_handle)
{
  xclDeviceHandle dev_handle = s_handle;
  printf("xma_plg_end_profile: dev_handle=%p\n", dev_handle);

  if (!XDP::mProfileStream.is_open()) {
    printf("WARNING: Please run xma_plg_start_profile before starting application.");
    return;
  }

  // Stop counters
  xclPerfMonStopCounters(dev_handle, XCL_PERF_MON_MEMORY);

  // Write profile summary
  XDP::writeProfileSummary(XDP::mProfileStream);

  // Write footer & close
  XDP::writeProfileFooter(XDP::mProfileStream);
  XDP::mProfileStream.close();
}

// *****************************************************************************
//                               Timeline Trace
// *****************************************************************************

void
profile_initialize(xclDeviceHandle s_handle, const char* trace_file_name)
{
  // Open timeline trace file
  XDP::mTraceStream.open(trace_file_name);

  //Make an initialization call for time
  XDP::time_ns();

  // Write header
  XDP::writeTraceHeader(XDP::mTraceStream);

  xclDeviceInfo2 deviceInfo;
  xclGetDeviceInfo2(s_handle, &deviceInfo);
  XDP::mKernelClockFreq = deviceInfo.mOCLFrequency[0];
  XDP::deviceName = deviceInfo.mName;

}

void
profile_start_trace(xclDeviceHandle s_handle, const char* data_transfer_trace, const char* stall_trace)
{
  // Evaluate arguments
  XDP::mDataTransferTrace = data_transfer_trace;
  XDP::mStallTrace = stall_trace;
  xclDeviceHandle dev_handle = s_handle;
  uint32_t traceOption = (XDP::mDataTransferTrace == "coarse") ? 0x1 : 0x0;
  if (XDP::mStallTrace == "dataflow")    traceOption |= (0x1 << 2);
  else if (XDP::mStallTrace == "pipe")   traceOption |= (0x1 << 3);
  else if (XDP::mStallTrace == "memory") traceOption |= (0x1 << 4);
  else if (XDP::mStallTrace == "all")    traceOption |= (0x7 << 2);
  else printf("The stall_trace setting of %s is not recognized. Please use memory|dataflow|pipe|all|off.", XDP::mStallTrace);
  printf("xma_plg_start_trace: dev_handle=%p, traceOption=%d\n", dev_handle, traceOption);

  // Start trace (also reads debug_ip_layout)
  xclPerfMonStartTrace(dev_handle, XCL_PERF_MON_MEMORY, traceOption);
  xclPerfMonStartTrace(dev_handle, XCL_PERF_MON_ACCEL, traceOption);

  // Get accelerator names
  uint32_t numAccels = xclGetProfilingNumberSlots(dev_handle, XCL_PERF_MON_ACCEL);
  for (uint32_t i=0; i < numAccels; ++i) {
    char name[128];
    xclGetProfilingSlotName(dev_handle, XCL_PERF_MON_ACCEL, i, name, 128);
	XDP::mAccelNames[i] = name;
  }

  // Get accelerator port names
  uint32_t numAccelPorts = xclGetProfilingNumberSlots(dev_handle, XCL_PERF_MON_MEMORY);
  for (uint32_t i=0; i < numAccelPorts; ++i) {
    char name[128];
    xclGetProfilingSlotName(dev_handle, XCL_PERF_MON_MEMORY, i, name, 128);
	XDP::mAccelPortNames[i] = name;
  }

}

void
profile_read_trace(xclDeviceHandle s_handle)
{
  xclDeviceHandle dev_handle = s_handle;
  printf("xma_plg_read_trace: dev_handle=%p\n", dev_handle);

  if (!XDP::mTraceStream.is_open()) {
    printf("WARNING: Please run xma_plg_start_trace before starting application.");
    return;
  }

  // Get device level info
  // TODO; instead of calling this multiple times, we should run once then store the whole thing
  /*
  xclDeviceInfo2 deviceInfo;
  xclGetDeviceInfo2(dev_handle, &deviceInfo);
  XDP::mKernelClockFreq = deviceInfo.mOCLFrequency[0];
  std::string deviceName = deviceInfo.mName;
  // TODO: do we know this?
   */
  std::string binaryName = "binary";

  // Data transfers
  xclTraceResultsVector traceVector = {0};
  xclPerfMonReadTrace(dev_handle, XCL_PERF_MON_MEMORY, traceVector);
  XDP::logTrace(XCL_PERF_MON_MEMORY, XDP::deviceName, binaryName, traceVector, XDP::mTraceStream);

  // Accelerators
  xclPerfMonReadTrace(dev_handle, XCL_PERF_MON_ACCEL, traceVector);
  XDP::logTrace(XCL_PERF_MON_ACCEL, XDP::deviceName, binaryName, traceVector, XDP::mTraceStream);
}

void
profile_end_trace(xclDeviceHandle s_handle)
{
  xclDeviceHandle dev_handle = s_handle;
  printf("xma_plg_end_trace: dev_handle=%p\n", dev_handle);

  if (!XDP::mTraceStream.is_open()) {
    printf("WARNING: Please run xma_plg_start_trace before starting application.");
    return;
  }

  xclDeviceInfo2 deviceInfo;
  xclGetDeviceInfo2(dev_handle, &deviceInfo);

  // Final read of trace
  profile_read_trace(s_handle);

  // Stop trace
  xclPerfMonStopTrace(dev_handle, XCL_PERF_MON_MEMORY);
  xclPerfMonStopTrace(dev_handle, XCL_PERF_MON_ACCEL);

  // Write footer & close
  XDP::writeTraceFooter(deviceInfo, XDP::mTraceStream);
  XDP::mTraceStream.close();
}

int  xclSyncBOWithProfile(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir,
        size_t size, size_t offset)
{
  int rc;

  XDP::logDataTransfer (
    static_cast<uint64_t>(boHandle)
     ,((dir == XCL_BO_SYNC_BO_TO_DEVICE) ? "WRITE_BUFFER" : "READ_BUFFER")
     ,"START"
     ,size
     ,0
     ,1
     ,XDP::deviceName
     ,0
     ,-1
     ,"Unknown"
     ,std::this_thread::get_id()
     ,""
     ,""
     ,0
     ,XDP::mTraceStream);

  rc = xclSyncBO(handle, boHandle, dir, size, offset);

  XDP::logDataTransfer (
    static_cast<uint64_t>(boHandle)
     ,((dir == XCL_BO_SYNC_BO_TO_DEVICE) ? "WRITE_BUFFER" : "READ_BUFFER")
     ,"END"
     ,size
     ,0
     ,1
     ,XDP::deviceName
     ,0
     ,-1
     ,"Unknown"
     ,std::this_thread::get_id()
     ,""
     ,""
     ,0
     ,XDP::mTraceStream);

  return rc;
}

