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

#ifndef __XDP_CORE_LOGGER_H
#define __XDP_CORE_LOGGER_H

#include "rt_util.h"
#include "xdp/profile/collection/counters.h"
#include "xdp/profile/collection/results.h"
#include "xdp/profile/plugin/base_plugin.h"
#include "xdp/profile/device/trace_parser.h"

#include "driver/include/xclperf.h"

#include <limits>
#include <cstdint>
#include <string>
#include <mutex>
#include <map>
#include <queue>

namespace xdp {
  class ProfileCounters;
  class TimeTrace;
  class TraceWriter;
  class KernelTrace;
  class BufferTrace;
  class DeviceTrace;

  // **************************************************************************
  // XDP trace logger class
  // **************************************************************************
  class TraceLogger {
  public:
    TraceLogger(ProfileCounters* profileCounters, TraceParser * TraceParserHandle, XDPPluginI* Plugin);
    ~TraceLogger();

  public:
    // Attach or detach observer writers
    // NOTE: the following functions are thread safe
    void attach(TraceWriterI* writer);
    void detach(TraceWriterI* writer);

  public:
    // Log host function calls (e.g., OpenCL APIs)
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

  public:
    // Timeline trace writers
    void writeTimelineTrace(double traceTime, const char* functionName,
        const char* eventName, unsigned int functionID) const;
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

  public:
    int getMigrateMemCalls() const { return mMigrateMemCalls;}
    std::string getCurrentBinaryName() const {return mCurrentBinaryName;}
    const std::set<std::thread::id>& getThreadIds() {return mThreadIdSet;}

  private:
    // helpers
    double getDeviceTimeStamp(double hostTimeStamp, std::string& deviceName);
    void addToThreadIds(const std::thread::id& threadId) {
      mThreadIdSet.insert(threadId);
    }

  private:
    bool mGetFirstCUTimestamp = true;
    bool mFunctionStartLogged = false;
    int mMigrateMemCalls;
    uint32_t mCurrentContextId;
    std::string mCurrentKernelName;
    std::string mCurrentDeviceName;
    std::string mCurrentBinaryName;
    std::mutex mLogMutex;

    std::map<uint64_t, KernelTrace*> mKernelTraceMap;
    std::map<uint64_t, BufferTrace*> mBufferTraceMap;
    std::map<uint64_t, DeviceTrace*> mDeviceTraceMap;
    std::map<std::string, std::queue<double>> mKernelStartsMap;
    std::set<std::thread::id> mThreadIdSet;

    ProfileCounters* mProfileCounters;
    std::vector<TraceWriterI*> mTraceWriters;

  private:
      TraceParser * mTraceParserHandle;
      XDPPluginI * mPluginHandle;
  };

} // xdp

#endif
