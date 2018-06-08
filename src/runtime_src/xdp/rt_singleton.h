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

// Copyright 2014 Xilinx, Inc. All rights reserved.
#ifndef __XILINX_RT_SINGLETON_H
#define __XILINX_RT_SINGLETON_H

#include <CL/opencl.h>
#include <string>
#include <map>
#include "xdp/profile/rt_profile.h"
#include "xdp/debug/rt_debug.h"
#include "driver/include/xclperf.h"
#include "xocl/core/platform.h"

// Use XCL::RTSingleton::Instance() to get to the singleton runtime object
// Runtime code base can now get access to the singleton and make certain
// decisions based upon the contents of the singleton

namespace XCL {
  class WriterI;

  /**
   * Check that the rtsingleton is in an active state.
   *
   * This function can be called during static global exit()
   * to check if it is no longer safe to rely on the singleton
   *
   * @return 
   *   true as long as main is running, false after the singleton dtor
   *   has been called during static global destruction.
   */
  bool active();

  class RTSingleton {
  public:
    static RTSingleton* Instance();
    cl_int getStatus() {
      return Status;
    }

    enum e_flow_mode {CPU = 0, COSIM_EM, HW_EM, DEVICE};
    enum e_ocl_profile_mode {NONE = 0, STREAM, PIPE, MEMORY, ACTIVITY};

  public:
    void turnOnProfile(RTProfile::e_profile_mode mode) {
      ProfileFlags |= mode;
      ProfileMgr->turnOnProfile(mode);
    }
    void turnOffProfile(RTProfile::e_profile_mode mode) {
      ProfileFlags &= ~mode;
      ProfileMgr->turnOffProfile(mode);
    }

  public:
    inline xocl::platform* getcl_platform_id() {return Platform.get(); }
    inline RTProfile* getProfileManager() {return ProfileMgr; }
    inline RTDebug* getDebugManager() { return DebugMgr ; }
    inline const int& getProfileFlag() { return ProfileFlags; }

  public:
    inline bool deviceCountersProfilingOn() { return getProfileFlag() & RTProfile::PROFILE_DEVICE_COUNTERS; }
    inline bool deviceTraceProfilingOn() { return getProfileFlag() & RTProfile::PROFILE_DEVICE_TRACE; }
    inline bool deviceOclProfilingOn() {
      return (isOclProfilingOn() && getFlowMode() == HW_EM); }
    inline bool kernelStreamProfilingOn(unsigned slotnum) {
      return (deviceTraceProfilingOn() && getOclProfileMode(slotnum) == STREAM && getFlowMode() != CPU);
    }
    inline bool kernelPipeProfilingOn(unsigned slotnum) {
      return (deviceTraceProfilingOn() && getOclProfileMode(slotnum) == PIPE && getFlowMode() != CPU);
    }
    inline bool kernelMemoryProfilingOn(unsigned slotnum) {
      return (deviceTraceProfilingOn() && getOclProfileMode(slotnum) == MEMORY && getFlowMode() != CPU);
    }
    inline bool applicationProfilingOn() { return getProfileFlag() & RTProfile::PROFILE_APPLICATION; }
    inline bool profilingOn() { return getProfileFlag() & RTProfile::PROFILE_ALL; }

    inline bool isOclProfilingOn() {return OclProfilingOn;}
    inline e_ocl_profile_mode getOclProfileMode(unsigned slotnum) {
      auto iter = OclProfileMode.find(slotnum);
      if (iter != OclProfileMode.end())
        return iter->second;
      return NONE;
    }

    inline void setFlowMode(e_flow_mode mode) {
      FlowMode = mode;

      // Turn off device profiling if cpu flow or old emulation flow
      if (mode == CPU || mode == COSIM_EM) {
        turnOffProfile(RTProfile::PROFILE_DEVICE);
      }
    }
    inline e_flow_mode getFlowMode() { return FlowMode; }
    void getFlowModeName(std::string& str);

  public:
    void logFinalTrace(xclPerfMonType type);
    unsigned getProfileNumberSlots(xclPerfMonType type, std::string& deviceName);
    void getProfileSlotName(xclPerfMonType type, std::string& deviceName,
                            unsigned slotnum, std::string& slotName);
    void getProfileKernelName(const std::string& deviceName, const std::string& cuName, std::string& kernelName);
    void setOclProfileMode(unsigned slotnum, std::string type);
    size_t getDeviceTimestamp(std::string& deviceName);
    double getReadMaxBandwidthMBps();
    double getWriteMaxBandwidthMBps();

  public:
    ~RTSingleton();
  private:
    RTSingleton();
    void startProfiling();
    void endProfiling();

  private:
    cl_int Status;

  private:
    // Share ownership of the global platform
    std::shared_ptr<xocl::platform> Platform;

    // Run time profiler
    RTProfile* ProfileMgr = nullptr;

    // Debug manager
    RTDebug* DebugMgr = nullptr;

    // Profile report writers
    std::vector<WriterI*> Writers;

    e_flow_mode FlowMode = CPU;
    bool OclProfilingOn = true;
    int ProfileFlags;
    std::map<unsigned, e_ocl_profile_mode> OclProfileMode;
  };
};

#endif


