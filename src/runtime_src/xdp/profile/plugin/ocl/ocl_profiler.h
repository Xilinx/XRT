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

#ifndef __XDP_CORE_PROFILER_H
#define __XDP_CORE_PROFILER_H

#include <CL/opencl.h>
#include <string>
#include <chrono>
#include <vector>
#include "xocl_plugin.h"
#include "xocl_profile.h"
#include "xdp/profile/core/rt_util.h"
#include "xdp/profile/writer/csv_profile.h"
#include "xdp/profile/writer/csv_trace.h"
#include "xdp/profile/writer/unified_csv_profile.h"

// Use Profiling::Profiler::Instance() to get to the singleton runtime object
// Runtime code base can access the singleton and make decisions based on the
// contents of the singleton

namespace Profiling {

  class Profiler {
  public:
    static Profiler* Instance();
    static bool InstanceExists() {
      return (mRTInstance != nullptr);
    }

  public:
    Profiler();
    ~Profiler();

  // Callback API
  public:
    void turnOnProfile(xdp::RTUtil::e_profile_mode mode);
    void turnOffProfile(xdp::RTUtil::e_profile_mode mode);
    void startDeviceProfiling(size_t numComputeUnits);
    void endDeviceProfiling();
    void getDeviceCounters(bool firstReadAfterProgram, bool forceReadCounters);
    void getDeviceTrace(bool forceReadTrace);
    void resetDeviceProfilingFlag() {mEndDeviceProfilingCalled = false;}
    void addToActiveDevices(const std::string& deviceName);
    void setKernelClockFreqMHz(const std::string &deviceName,
                               unsigned int clockRateMHz);

  public:
    inline xdp::XoclPlugin* getPlugin() { return Plugin; }
    inline xdp::RTProfile* getProfileManager() { return ProfileMgr; }

  // Device metadata
  public:
    std::map<xdp::profile::device::key, xdp::profile::device::data> DeviceData;

  // Profile settings
  public:
    inline bool deviceCountersProfilingOn() {
      return getProfileFlag() & xdp::RTUtil::PROFILE_DEVICE_COUNTERS;
    }
    inline bool deviceTraceProfilingOn() {
      return getProfileFlag() & xdp::RTUtil::PROFILE_DEVICE_TRACE;
    }
    inline bool applicationProfilingOn() {
      return getProfileFlag() & xdp::RTUtil::PROFILE_APPLICATION;
    }
    inline bool applicationTraceOn() {
      return getProfileFlag() & xdp::RTUtil::FILE_TIMELINE_TRACE;
    }
  
  private:
    void startProfiling();
    void endProfiling();
    void logFinalTrace(xclPerfMonType type);
    void setTraceFooterString();
    bool isProfileRunning() {return mProfileRunning;}
    inline const int& getProfileFlag() { return ProfileFlags; }
    uint32_t getTimeDiffUsec(std::chrono::steady_clock::time_point start,
                             std::chrono::steady_clock::time_point end);

  private:
    int ProfileFlags;
    bool mProfileRunning = false;
    bool mEndDeviceProfilingCalled = false;
    // Report writers
    std::vector<xdp::ProfileWriterI*> ProfileWriters;
    std::vector<xdp::TraceWriterI*> TraceWriters;
    static Profiler* mRTInstance;
    xdp::XoclPlugin* Plugin;
    xdp::RTProfile* ProfileMgr = nullptr;

  };

  /*
   * Callback functions called from xocl
   */
  void cb_get_device_trace(bool forceReadTrace);
  void cb_get_device_counters(bool firstReadAfterProgram, bool forceReadCounters);
  void cb_start_device_profiling(size_t numComputeUnits);
  void cb_reset_device_profiling();
  void cb_end_device_profiling();

};

#endif