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

#ifndef __XILINX_RT_PROFILE_H
#define __XILINX_RT_PROFILE_H

#include "rt_perf_counters.h"
#include "rt_profile_device.h"
#include "rt_profile_results.h"
#include "rt_profile_xocl.h"
#include "xrt/util/time.h"
//#include <chrono>
//#include <time.h>

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

namespace XCL {
  class WriterI;
  class TimeTrace;
  class KernelTrace;
  class BufferTrace;
  class DeviceTrace;
  class ProfileRuleChecks;

  // **************************************************************************
  // Top-level profile class
  // **************************************************************************
  class RTProfile {
  public:
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

  public:
    RTProfile(int& flags);
    ~RTProfile();

  public:
    void turnOnProfile(e_profile_mode mode) { ProfileFlags |= mode; }
    void turnOffProfile(e_profile_mode mode) { ProfileFlags &= ~mode; }

  public:
    void turnOnFile(e_write_file file) {
      FileFlags |= file;
    }

    bool isSummaryFileOn() const { return FileFlags & RTProfile::FILE_SUMMARY;}
    bool isTimelineTraceFileOn() const { return FileFlags & RTProfile::FILE_TIMELINE_TRACE; }

  public:
    int getProfileFlags() { return ProfileFlags; }
    bool isDeviceProfileOn() const;
    bool isApplicationProfileOn() const { return ProfileFlags & RTProfile::PROFILE_APPLICATION;}

    int getMigrateMemCalls() const { return MigrateMemCalls;}

    void setTransferTrace(const std::string traceStr);
    void setStallTrace(const std::string traceStr);
    e_device_trace getTransferTrace() {return DeviceTraceOption;}
    e_stall_trace getStallTrace() {return StallTraceOption;}

  public:
    // Following functions are thread safe
    // attach or detach observer writers
    void attach(WriterI* writer);
    void detach(WriterI* writer);

  public:
    void setKernelClockFreqMHz(const std::string &deviceName, unsigned int kernelClockRateMHz);
    void setDeviceClockFreqMHz(double deviceClockRateMHz);
    void setDeviceTraceClockFreqMHz(double deviceTraceClockRateMHz);
    void setGlobalMemoryBitWidth(uint32_t bitWidth);
    unsigned int getKernelClockFreqMHz(std::string &deviceName);
    uint32_t getGlobalMemoryBitWidth();
    uint32_t getTraceSamplesThreshold();
    uint32_t getSampleIntervalMsec();
    void logDeviceTrace(std::string deviceName, std::string binaryName, xclPerfMonType type,
        xclTraceResultsVector& traceVector);
    void logDeviceCounters(std::string deviceName, std::string binaryName, xclPerfMonType type,
        xclCounterResults& counterResults, uint64_t timeNsec, bool firstReadAfterProgram);

  public:
    // log buffer read and writes.
    void logDataTransfer(uint64_t objId, e_profile_command_kind objKind,
        e_profile_command_state objStage, size_t objSize, uint32_t contextId,
        uint32_t numDevices, std::string deviceName, uint32_t commandQueueId,
        uint64_t address, const std::string& bank, std::thread::id threadId,
        const std::string eventString = "", const std::string dependString = "",
        double timestampMsec = 0.0);
    void logBufferWrite(size_t size, double duration, uint32_t contextId, uint32_t numDevices) {
        PerfCounters.logBufferWrite(size, duration, contextId, numDevices);
    }

    // log Kernel execution.
    void logKernelExecution(uint64_t objId, uint32_t programId, uint64_t eventId, e_profile_command_state objStage,
        std::string kernelName, std::string xclbinName, uint32_t contextId,
        uint32_t commandQueueId, const std::string& deviceName, uid_t uid,
        const size_t* globalWorkSize, size_t workGroupSize,
        const size_t* localWorkDim, const std::string& cu_name,
        const std::string eventString = "", const std::string dependString = "",
        double timeStampMsec = 0.0);

  void logDependency(e_profile_command_kind objKind,
      const std::string eventString, const std::string dependString);

    // log user or cl API function calls
    void logFunctionCallStart(const char* functionName, long long queueAddress);
    void logFunctionCallEnd(const char* functionName, long long queueAddress);


  public:
    void writeProfileSummary();

    // Summaries of counts
    void writeAPISummary(WriterI* writer) const;
    void writeKernelSummary(WriterI* writer) const;
    void writeStallSummary(WriterI* writer) const;
    void writeKernelStreamSummary(WriterI* writer) const;
    void writeComputeUnitSummary(WriterI* writer) const;
    void writeHostTransferSummary(WriterI* writer) const;
    void writeKernelTransferSummary(WriterI* writer) const;
    void writeDeviceTransferSummary(WriterI* writer) const;
    // Top offenders lists
    void writeTopKernelSummary(WriterI* writer) const;
    void writeTopKernelTransferSummary(WriterI* writer) const;
    void writeTopDataTransferSummary(WriterI* writer, bool isRead) const;
    void writeTopDeviceTransferSummary(WriterI* writer, bool isRead) const;
    // Profile Rule Checks
    void getProfileRuleCheckSummary();
    void writeProfileRuleCheckSummary(WriterI* writer);
    // Unified summaries
    void writeAcceleratorSummary(WriterI* writer) const;
    void writeTopHardwareSummary(WriterI* writer) const;

    // Timeline Trace
    void writeTimelineTrace(double traceTime, const char* functionName,
        const char* eventName) const;
    void writeTimelineTrace(double traceTime, const std::string& commandString,
        const std::string& stageString, const std::string& eventString,
        const std::string& dependString, uint64_t objId, size_t size) const;
    void writeTimelineTrace(double traceTime, const std::string& commandString,
        const std::string& stageString, const std::string& eventString,
        const std::string& dependString, size_t size, uint64_t address,
        const std::string& bank, std::thread::id threadId) const;
    void writeTimelineTrace(double traceTime,
      const std::string& commandString, const std::string& stageString,
      const std::string& eventString, const std::string& dependString) const;

    std::string getProjectName() const {return CurrentBinaryName;}
    std::string getDeviceNames() const;
    std::string getDeviceNames(const std::string& sep) const;
    double getGlobalMemoryMaxBandwidthMBps() const;

    // Functions required by PRCs
    double getDeviceStartTime(const std::string& deviceName) const;
    double getTotalKernelExecutionTime(const std::string& deviceName) const;
    uint32_t getComputeUnitCalls(const std::string& deviceName, const std::string& cuName) const;
    void getKernelFromComputeUnit(const std::string& cuName, std::string& kernelName) const;
    void getTraceStringFromComputeUnit(const std::string& deviceName, const std::string& cuName, std::string& traceString) const;

  public:
    double getTraceTime();

    // Get timestamp in msec given time in nsec
    double getTimestampMsec(uint64_t timeNsec) {
      return (timeNsec / 1.0e6);
    }

  public:
    void addToActiveDevices(const std::string& deviceName);
    bool isDeviceActive(const std::string& deviceName) const;
    void setSlotComputeUnitName(int slotnum, const std::string& cuName);
    void getSlotNames(int slotnum, std::string& cuName, std::string& kernelName) const;

    void addToThreadIds(const std::thread::id& threadId) {
      ThreadIdSet.insert(threadId);
    }
    const std::set<std::thread::id>& getThreadIds() {return ThreadIdSet;}

    bool getLoggingTrace(int index);
    void setLoggingTrace(int index, bool value);
    uint64_t getLoggingTraceUsec();
    void setLoggingTraceUsec(uint64_t value);

  private:
    uint32_t getCounterValue(xclPerfMonCounterType type, uint32_t slotnum,
        xclCounterResults& results) const;
    double getDeviceTimeStamp(double hostTimeStamp, std::string& deviceName);
    void commandKindToString(e_profile_command_kind objKind,
        std::string& commandString) const;
    void commandStageToString(e_profile_command_state objStage,
        std::string& stageString) const;
    void setTimeStamp(e_profile_command_state objStage, TimeTrace* traceObject, double timeStamp);
    xclPerfMonEventID getFunctionEventID(const std::string &functionName, long long queueAddress);

    void setArgumentsBank(const std::string& deviceName);

  public:
    void getArgumentsBank(const std::string& deviceName, const std::string& cuName,
    	                  const std::string& portName, std::string& argNames,
						  std::string& memoryName) const;

  private:
    typedef std::tuple<std::string, std::string, std::string, std::string, uint32_t> CUPortArgsBankType;
    std::vector<CUPortArgsBankType> CUPortVector;

  public:
    std::vector<CUPortArgsBankType> getCUPortVector() const {return CUPortVector;}
    std::map<std::string, int> getCUPortsToMemoryMap() const {return CUPortsToMemoryMap;}

  private:
    bool IsZynq = false;
    bool GetFirstCUTimestamp = true;
    bool FunctionStartLogged;
    int& ProfileFlags;
    int FileFlags; //Which files we want to write out.
    int OclSlotIndex;
    int HostSlotIndex;
    int MigrateMemCalls;
    e_device_trace DeviceTraceOption;
    e_stall_trace StallTraceOption;
    uint32_t CurrentContextId;
    std::string CurrentKernelName;
    std::string CurrentDeviceName;
    std::string CurrentBinaryName;
    PerformanceCounter PerfCounters;
    std::set<std::thread::id> ThreadIdSet;
    std::map<int, std::string> SlotComputeUnitNameMap;
    std::map<std::string, std::queue<double>> KernelStartsMap;
    std::map<std::string, std::string> ComputeUnitKernelNameMap;
    std::map<std::string, std::string> ComputeUnitKernelTraceMap;
    std::map<std::string, xclCounterResults> FinalCounterResultsMap;
    std::map<std::string, xclCounterResults> RolloverCounterResultsMap;
    std::map<std::string, xclCounterResults> RolloverCountsMap;
    std::map<std::string, std::vector<std::string>> DeviceBinaryDataSlotsMap;
    std::map<std::string, std::vector<std::string>> DeviceBinaryCuSlotsMap;
    std::map<std::string, std::vector<std::string>> DeviceBinaryStrSlotsMap;
    std::map<uint64_t, KernelTrace*> KernelTraceMap;
    std::map<uint64_t, BufferTrace*> BufferTraceMap;
    std::map<uint64_t, DeviceTrace*> DeviceTraceMap;
    std::mutex LogMutex;
    RTProfileDevice* DeviceProfile;
    ProfileRuleChecks* RuleChecks;

  private:
    std::vector<WriterI*> Writers;
    std::set<std::string> ActiveDevices;
    std::map<std::string, int> CUPortsToMemoryMap;

  // Platform data and Device data
  private:
    bool mLoggingTrace[XCL_PERF_MON_TOTAL_PROFILE] = {false};
    uint64_t mLoggingTraceUsec = 0;

  public:
    std::map<xdp::profile::device::key,xdp::profile::device::data> device_data;
  };

};
#endif



