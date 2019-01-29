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

#ifndef __XDP_CORE_RT_PROFILE_H
#define __XDP_CORE_RT_PROFILE_H

#include "rt_util.h"
#include "driver/include/xclperf.h"
#include "xdp/profile/plugin/base_plugin.h"

#include <set>
#include <vector>
#include <string>
#include <limits>
#include <cstdint>
#include <thread>
#include <iostream>
#include <memory>

// Separator used for CU port and memory resource (must match HW linker)
#define PORT_MEM_SEP "-"

namespace xdp {
  class SummaryWriter;
  class TraceLogger;
  class ProfileWriterI;
  class TraceWriterI;
  class TraceParser;
  class ProfileCounters;

  // **************************************************************************
  // Top-level profile class
  // **************************************************************************
  class RTProfile {
  public:
    RTProfile(int& flags, std::shared_ptr<XDPPluginI> mPluginHandle);
    ~RTProfile();

  public:
    // Profiling options and settings
    void turnOnProfile(RTUtil::e_profile_mode mode) { mProfileFlags |= mode; }
    void turnOffProfile(RTUtil::e_profile_mode mode) { mProfileFlags &= ~mode; }
    void turnOnFile(RTUtil::e_write_file file) { mFileFlags |= file; }
    int getProfileFlags() { return mProfileFlags; }
    bool isDeviceProfileOn() const;
    bool isApplicationProfileOn() const { return mProfileFlags & RTUtil::PROFILE_APPLICATION; }
    void setTransferTrace(const std::string traceStr);
    void setStallTrace(const std::string traceStr);
    RTUtil::e_device_trace getTransferTrace() { return mDeviceTraceOption; }
    RTUtil::e_stall_trace getStallTrace() { return mStallTraceOption; }

  public:
    // Attach or detach observer writers
    // NOTE: the following functions are thread safe
    void attach(ProfileWriterI* writer);
    void attach(TraceWriterI* writer);
    void detach(ProfileWriterI* writer);
    void detach(TraceWriterI* writer);

  public:
    // Settings: clock freqs, bit widths, etc.
    void setTraceClockFreqMHz(unsigned int kernelClockRateMHz);
    void setDeviceClockFreqMHz(double deviceClockRateMHz);
    void setDeviceTraceClockFreqMHz(double deviceTraceClockRateMHz);
    void setGlobalMemoryBitWidth(uint32_t bitWidth);
    uint32_t getGlobalMemoryBitWidth();
    uint32_t getTraceSamplesThreshold();
    uint32_t getSampleIntervalMsec();

  public:
    void writeProfileSummary();
    void addDeviceName(std::string deviceName) { mDeviceNames.push_back(deviceName); }
    std::string getDeviceNames(const std::string& sep) const;
    std::string getProjectName() const;
    int getMigrateMemCalls() const;
    const std::set<std::thread::id>& getThreadIds();
    // Functions required by guidance
    double getDeviceStartTime(const std::string& deviceName) const;
    double getTotalKernelExecutionTime(const std::string& deviceName) const;
    uint32_t getComputeUnitCalls(const std::string& deviceName, const std::string& cuName) const;

  public:
    bool getLoggingTrace(int index);
    void setLoggingTrace(int index, bool value);
    TraceParser* getTraceParser() {return mTraceParser; }

  public:
    // External access to writer
    void writeAPISummary(ProfileWriterI* writer) const;
    void writeKernelSummary(ProfileWriterI* writer) const;
    void writeStallSummary(ProfileWriterI* writer) const;
    void writeKernelStreamSummary(ProfileWriterI* writer);
    void writeComputeUnitSummary(ProfileWriterI* writer) const;
    void writeHostTransferSummary(ProfileWriterI* writer) const;
    void writeKernelTransferSummary(ProfileWriterI* writer);
    void writeDeviceTransferSummary(ProfileWriterI* writer) const;
    // Top offenders lists
    void writeTopKernelSummary(ProfileWriterI* writer) const;
    void writeTopKernelTransferSummary(ProfileWriterI* writer) const;
    void writeTopDataTransferSummary(ProfileWriterI* writer, bool isRead) const;
    void writeTopDeviceTransferSummary(ProfileWriterI* writer, bool isRead) const;
    // Unified summaries
    void writeAcceleratorSummary(ProfileWriterI* writer) const;
    void writeTopHardwareSummary(ProfileWriterI* writer) const;

  public:
    // External access to logger
    void logFunctionCallStart(const char* functionName, long long queueAddress, unsigned int functionID);
    void logFunctionCallEnd(const char* functionName, long long queueAddress, unsigned int functionID);

    // Log host buffer reads and writes
    void logDataTransfer(uint64_t objId, RTUtil::e_profile_command_kind objKind,
        RTUtil::e_profile_command_state objStage, size_t objSize, uint32_t contextId,
        uint32_t numDevices, std::string deviceName, uint32_t commandQueueId,
        uint64_t address, const std::string& bank, std::thread::id threadId,
        const std::string eventString = "", const std::string dependString = "",
        double timeStampMsec = 0.0);

    // Log Kernel execution
    void logKernelExecution(uint64_t objId, uint32_t programId, uint64_t eventId,
        RTUtil::e_profile_command_state objStage, std::string kernelName, std::string xclbinName,
        uint32_t contextId, uint32_t commandQueueId, const std::string& deviceName, uid_t uid,
        const size_t* globalWorkSize, size_t workGroupSize, const size_t* localWorkDim,
        const std::string& cu_name, const std::string eventString = "", const std::string dependString = "",
        double timeStampMsec = 0.0);

    // Log a dependency (e.g., a kernel waiting on a host write)
    void logDependency(RTUtil::e_profile_command_kind objKind,
       const std::string eventString, const std::string dependString);

    // Log device trace
    void logDeviceTrace(std::string deviceName, std::string binaryName, xclPerfMonType type,
        xclTraceResultsVector& traceVector);

    // Log device counters (used in profile summary)
    void logDeviceCounters(std::string deviceName, std::string binaryName, xclPerfMonType type,
        xclCounterResults& counterResults, uint64_t timeNsec, bool firstReadAfterProgram);

  private:
    int& mProfileFlags;
    int mFileFlags; // Which files we want to write out
    RTUtil::e_device_trace mDeviceTraceOption;
    RTUtil::e_stall_trace mStallTraceOption;
    bool mLoggingTrace[XCL_PERF_MON_TOTAL_PROFILE] = {false};
    ProfileCounters* mProfileCounters = nullptr;
    TraceParser* mTraceParser;
    TraceLogger* mLogger;
    SummaryWriter* mWriter;
    std::vector<std::string> mDeviceNames;
    std::shared_ptr<XDPPluginI> mPluginHandle;
  };

} // xdp

#endif
