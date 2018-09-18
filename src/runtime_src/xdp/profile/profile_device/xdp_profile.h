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

#ifndef __XILINX_XDP_PROFILE_H
#define __XILINX_XDP_PROFILE_H

#include <limits>
#include <cstdint>
#include <map>
#include <set>
#include <vector>
#include <tuple>
#include <string>
#include <thread>
#include <mutex>
#include <queue>

#include "xclhal2.h"
#include "xdp_perf_counters.h"
#include "xdp_profile_results.h"

namespace XDP {
  class WriterI;
  class TimeTrace;
  class KernelTrace;
  class BufferTrace;
  class DeviceTrace;

  // **************************************************************************
  // Top-level profile class
  // **************************************************************************
  class XDPProfile {
  public:

    enum e_flow_mode {
      CPU = 0,
	  HW_EM,
	  DEVICE
    };

    //This enum controls the "collection" of data.
    enum e_profile_mode {
      // Keep PROFILE_OFF 0 always
      PROFILE_OFF = 0x0,
      PROFILE_APPLICATION = 0x1 << 1,
      PROFILE_DEVICE_COUNTERS = 0x1 << 2,
      PROFILE_DEVICE_TRACE = 0x1 << 3,
      PROFILE_DEVICE = PROFILE_DEVICE_COUNTERS | PROFILE_DEVICE_TRACE,
      PROFILE_ALL = PROFILE_APPLICATION | PROFILE_DEVICE,
    };

    enum e_profile_command_kind {
      READ_BUFFER = 0x1,
      WRITE_BUFFER = 0x2,
      EXECUTE_KERNEL = 0x3,
      DEVICE_KERNEL_READ = 0x4,
      DEVICE_KERNEL_WRITE = 0x5,
      DEVICE_KERNEL_EXECUTE = 0x6,
      DEVICE_BUFFER_READ = 0x7,
      DEVICE_BUFFER_WRITE = 0x8,
      DEPENDENCY_EVENT = 0x9
    };

    enum e_profile_command_state {
      QUEUE = 0x1,
      SUBMIT = 0x2,
      START = 0x3,
      END = 0x4,
      COMPLETE = 0x5
    };

    enum e_write_file {
      FILE_SUMMARY = 0x1,
      FILE_TIMELINE_TRACE = 0x2
    };

    enum e_device_trace {
      DEVICE_TRACE_OFF = 0x0,
      DEVICE_TRACE_FINE = 0x1,
      DEVICE_TRACE_COARSE = 0x2
    };

    enum e_stall_trace {
      STALL_TRACE_OFF = 0x0,
      STALL_TRACE_EXT = 0x1,
      STALL_TRACE_INT = 0x1 << 1,
      STALL_TRACE_STR = 0x1 << 2,
      STALL_TRACE_ALL = STALL_TRACE_EXT | STALL_TRACE_INT | STALL_TRACE_STR
    };

    typedef std::vector<DeviceTrace> TraceResultVector;

  public:
    XDPProfile(int& flags);
    ~XDPProfile();

  public:
    // Following functions are thread safe
    // attach or detach observer writers
    void attach(WriterI* writer);
    void detach(WriterI* writer);

    void logDeviceCounters(std::string deviceName, std::string binaryName, xclPerfMonType type,
        xclCounterResults& counterResults, uint64_t timeNsec, bool firstReadAfterProgram);

    void writeTimelineTrace(double traceTime, const std::string& commandString,
              const std::string& stageString, const std::string& eventString,
              const std::string& dependString, size_t size, uint64_t address,
              const std::string& bank, std::thread::id threadId);
/*
    void writeDataTransferTrace(double traceTime, const std::string& commandString,
            const std::string& stageString, const std::string& eventString,
            const std::string& dependString, size_t size, uint64_t address,
            const std::string& bank, std::thread::id threadId,  std::ofstream& ofs);
*/
    void logDataTransfer(uint64_t objId, const std::string& commandString,
            const std::string& stageString, const std::string& eventString,
            const std::string& dependString, size_t size, uint64_t address,
            const std::string& bank, std::thread::id threadId);

    void logTrace(xclPerfMonType type, const std::string deviceName, std::string binaryName,
                xclTraceResultsVector& traceVector);
  public:
    void turnOnProfile(e_profile_mode mode) { ProfileFlags |= mode; }
    void turnOffProfile(e_profile_mode mode) { ProfileFlags &= ~mode; }

  public:
    void turnOnFile(e_write_file file) {
      FileFlags |= file;
    }

    int getProfileFlags() { return ProfileFlags; }
    bool isDeviceProfileOn() const {return (FlowMode == CPU) ? false : (ProfileFlags & XDPProfile::PROFILE_DEVICE_COUNTERS);}
    bool isApplicationProfileOn() const { return ProfileFlags & XDPProfile::PROFILE_APPLICATION;}
    bool isSummaryFileOn() const { return FileFlags & XDPProfile::FILE_SUMMARY;}
    bool isTimelineTraceFileOn() const { return FileFlags & XDPProfile::FILE_TIMELINE_TRACE; }

    e_flow_mode getFlowMode() const {return FlowMode;}
    void getFlowModeName(std::string& str) const;

    void setProfileNumberSlots(xclPerfMonType type, uint32_t numSlots)
    {
      // TODO: for now, assume single device
      NumberSlotMap[type] = numSlots;
    }
    uint32_t getProfileNumberSlots(xclPerfMonType type, const std::string& deviceName) const;

  public:
    void setKernelClockFreqMHz(const std::string &deviceName, unsigned int kernelClockRateMHz);
    unsigned int getKernelClockFreqMHz(std::string &deviceName) const;
    void trainDeviceHostTimestamps(std::string deviceName, xclPerfMonType type);
    double convertDeviceToHostTimestamp(uint64_t deviceTimestamp, xclPerfMonType type,
                                      const std::string& deviceName);

  public:
    void writeProfileSummary();

    // Summaries of counts
    //void writeAPISummary(WriterI* writer) const;
    void writeKernelSummary(WriterI* writer) const;
    void writeStallSummary(WriterI* writer) const;
    void writeComputeUnitSummary(WriterI* writer) const;
    void writeHostTransferSummary(WriterI* writer) const;
    void writeKernelTransferSummary(WriterI* writer) const;
    void writeDeviceTransferSummary(WriterI* writer) const;
    // Top offenders lists
    void writeTopKernelSummary(WriterI* writer) const;
    void writeTopKernelTransferSummary(WriterI* writer) const;
    void writeTopDataTransferSummary(WriterI* writer, bool isRead) const;
    void writeTopDeviceTransferSummary(WriterI* writer, bool isRead) const;

    // for now, always return true
    bool isDeviceActive(const std::string& deviceName) const {return true;}
    std::string getDeviceName() const {return CurrentDeviceName;}
    std::string getProjectName() const {return CurrentBinaryName;}
    double getDeviceClockFreqMHz() const;
    double getGlobalMemoryClockFreqMHz() const;
    uint32_t getGlobalMemoryBitWidth() const;
    double getGlobalMemoryMaxBandwidthMBps() const;
    double getReadMaxBandwidthMBps() const;
    double getWriteMaxBandwidthMBps() const;
    unsigned long time_ns();
    double getTimestampMsec(uint64_t timeNsec);
    double getTraceTime();
    double getDeviceTimeStamp(double hostTimeStamp, std::string& deviceName);

    void setProfileSlotName(xclPerfMonType type, std::string& deviceName,
                            unsigned slotnum, std::string& slotName) {
      if (type == XCL_PERF_MON_ACCEL)
        SlotComputeUnitNameMap[slotnum] = slotName;
      else if (type == XCL_PERF_MON_MEMORY)
        SlotComputeUnitPortNameMap[slotnum] = slotName;
    }
    void getProfileSlotName(xclPerfMonType type, std::string& deviceName,
                            unsigned slotnum, std::string& slotName) const;
    void setProfileKernelName(const std::string& deviceName, const std::string& cuName,
                              std::string& kernelName) {
      ComputeUnitKernelNameMap[cuName] = kernelName;
    }
    void getProfileKernelName(const std::string& deviceName, const std::string& cuName,
                              std::string& kernelName) const;

  public:
    void addToThreadIds(const std::thread::id& threadId) {
      ThreadIdSet.insert(threadId);
    }
    const std::set<std::thread::id>& getThreadIds() {return ThreadIdSet;}

  private:
    void commandKindToString(e_profile_command_kind objKind,
        std::string& commandString) const;
    void commandStageToString(e_profile_command_state objStage,
        std::string& stageString) const;

  private:
    int ProfileFlags;
    int FileFlags; //Which files we want to write out
    int HostSlotIndex;
    e_flow_mode FlowMode;
    std::string CurrentDeviceName;
    std::string CurrentBinaryName;
    PerformanceCounter PerfCounters;
    std::set<std::thread::id> ThreadIdSet;
    std::map<xclPerfMonType, uint32_t> NumberSlotMap;
    std::map<int, std::string> SlotComputeUnitNameMap;
    std::map<int, std::string> SlotComputeUnitPortNameMap;
    std::map<std::string, std::queue<double>> KernelStartsMap;
    std::map<std::string, std::string> ComputeUnitKernelNameMap;
    std::map<std::string, std::string> ComputeUnitKernelTraceMap;
    std::map<std::string, xclCounterResults> FinalCounterResultsMap;
    std::map<std::string, xclCounterResults> RolloverCounterResultsMap;
    std::map<std::string, xclCounterResults> RolloverCountsMap;
    std::map<std::string, std::vector<std::string>> DeviceBinaryDataSlotsMap;
    std::map<std::string, std::vector<std::string>> DeviceBinaryCuSlotsMap;
    std::map<std::string, unsigned int> DeviceKernelClockFreqMap;
    std::map<uint64_t, KernelTrace*> KernelTraceMap;
    std::map<uint64_t, BufferTrace*> BufferTraceMap;
    std::map<uint64_t, DeviceTrace*> DeviceTraceMap;
    std::mutex LogMutex;

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

    //RTProfileDevice* DeviceProfile;
    //ProfileRuleChecks* RuleChecks;

  private:
    std::vector<WriterI*> Writers;
    std::set<std::string> ActiveDevices;

    const static int MAX_DDR_BANKS = 8;
    int CUPortsToDDRBanks[MAX_DDR_BANKS];

  // Platform data and Device data
  //private:
  //  bool mLoggingTrace[XCL_PERF_MON_TOTAL_PROFILE] = {false};
  //  uint64_t mLoggingTraceUsec = 0;

  //public:
  //  std::map<xdp::profile::device::key,xdp::profile::device::data> device_data;
  };

};
#endif



