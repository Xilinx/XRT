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
#ifndef __XDP_RT_SINGLETON_H
#define __XDP_RT_SINGLETON_H

#include <CL/opencl.h>
#include "xdp/profile/plugin/base_plugin.h"
#include "xdp/debug/rt_debug.h"
#include "xdp/profile/core/rt_profile.h"

#include <cstdlib>
#include <cstdio>
#include <string>
#include <chrono>
#include <iostream>
#include <map>

// Use XDP::RTSingleton::Instance() to get to the singleton runtime object
// Runtime code base can now get access to the singleton and make certain
// decisions based upon the contents of the singleton

namespace xdp {
  class ProfileWriterI;
  class TraceWriterI;
  class XDPPluginI;

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
    ~RTSingleton();

  private:
    RTSingleton();

  public:
    // Singleton instance
    static RTSingleton* Instance();
    cl_int getStatus() {
      return Status;
    }

    enum e_flow_mode {CPU = 0, COSIM_EM, HW_EM, DEVICE};
    enum e_ocl_profile_mode {NONE = 0, STREAM, PIPE, MEMORY, ACTIVITY};

  public:
    // Turn on/off profiling
    void turnOnProfile(RTUtil::e_profile_mode mode);
    void turnOffProfile(RTUtil::e_profile_mode mode);

  private:
    // Start/end profiling
    void startProfiling();
    void endProfiling();

  public:
    // Inline functions: platform ID, profile/debug managers, profile flags
    inline xocl::platform* getcl_platform_id() { return Platform.get(); }
    inline RTProfile* getProfileManager() { return ProfileMgr; }
    inline RTDebug* getDebugManager() { return DebugMgr; }
    inline const int& getProfileFlag() { return ProfileFlags; }
    // Access to plugins from XDP
    inline XDPPluginI* getPlugin() { assert(Plugin != NULL); return Plugin; }
    inline void attachPlugin(XDPPluginI* plugin) { Plugin = plugin; }

  public:
    // Profile settings
    inline bool deviceCountersProfilingOn() { return getProfileFlag() & RTUtil::PROFILE_DEVICE_COUNTERS; }
    inline bool deviceTraceProfilingOn() { return getProfileFlag() & RTUtil::PROFILE_DEVICE_TRACE; }
    inline bool applicationProfilingOn() { return getProfileFlag() & RTUtil::PROFILE_APPLICATION; }

    inline void setFlowMode(e_flow_mode mode) {
      FlowMode = mode;
      // Turn off device profiling if cpu flow or old emulation flow
      if (mode == CPU || mode == COSIM_EM) {
        turnOffProfile(RTUtil::PROFILE_DEVICE);
      }
    }
    inline e_flow_mode getFlowMode() { return FlowMode; }
    void getFlowModeName(std::string& str);
    inline bool isHwEmu() { return (getFlowMode() == HW_EM); }

  public:
    // Misc. exposed profile functions
    void logFinalTrace(xclPerfMonType type);
    unsigned getProfileNumberSlots(xclPerfMonType type, std::string& deviceName);
    void getProfileSlotName(xclPerfMonType type, std::string& deviceName,
                            unsigned slotnum, std::string& slotName);
    unsigned getProfileSlotProperties(xclPerfMonType type, std::string& deviceName, unsigned slotnum);

    // Objects released (used by guidance)
    void setObjectsReleased(bool objectsReleased) {IsObjectsReleased = objectsReleased;}
    bool isObjectsReleased() {return IsObjectsReleased;}

    // Add to active devices
    void addToActiveDevices(const std::string& deviceName);

  private:
    // Status of singleton
    cl_int Status;

    // Share ownership of the global platform
    std::shared_ptr<xocl::platform> Platform;

    // Run time profiler
    RTProfile* ProfileMgr = nullptr;

    // Debug manager
    RTDebug* DebugMgr = nullptr;

    // XDP plugin
    XDPPluginI* Plugin;

    // Report writers
    std::vector<ProfileWriterI*> ProfileWriters;
    std::vector<TraceWriterI*> TraceWriters;

    e_flow_mode FlowMode = CPU;
    bool IsObjectsReleased = false;
    int ProfileFlags;

  };

} // xdp

#endif