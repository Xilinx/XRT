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

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <chrono>

#include "profiler.h"
#include "xdp/profile/debug.h"
#include "xdp/rt_singleton.h"
#include "xdp/profile/core/rt_profile.h"

#include "driver/include/xclperf.h"

#include "xrt/util/message.h"

namespace Profiling {

  static bool pActive = false;

  bool active()
  {
    return pActive;
  }

  Profiler* Profiler::Instance() {
    static Profiler singleton;
    return &singleton;
  };

  Profiler::Profiler()
  {
    pActive = true;
    Plugin = new xdp::XoclPlugin();
    auto rts = xdp::RTSingleton::Instance();
    assert(rts);
    // attach plugin before xdp tries to access it
    rts->attachPlugin(Plugin);
  }

  Profiler::~Profiler()
  {
    pActive = false;

    auto rts = xdp::RTSingleton::Instance();
    if (rts)
      rts->setObjectsReleased(mEndDeviceProfilingCalled);

    if (!mEndDeviceProfilingCalled && rts && rts->applicationProfilingOn()) {
      xrt::message::send(xrt::message::severity_level::WARNING,
          "Profiling may contain incomplete information. Please ensure all OpenCL objects are released by your host code (e.g., clReleaseProgram()).");

      // Before deleting, do a final read of counters and force flush of trace buffers
      endDeviceProfiling();
    }
  }

  // Start device profiling
  void Profiler::startDeviceProfiling(size_t numComputeUnits)
  {
    auto rts = xdp::RTSingleton::Instance();
    // Start counters
    if (rts->deviceCountersProfilingOn())
      xdp::profile::platform::start_device_counters(rts->getcl_platform_id(),XCL_PERF_MON_MEMORY);

    // Start trace
    if (rts->deviceTraceProfilingOn())
      xdp::profile::platform::start_device_trace(rts->getcl_platform_id(),XCL_PERF_MON_MEMORY, numComputeUnits);

    if (rts->isHwEmu())
      xdp::profile::platform::start_device_trace(rts->getcl_platform_id(),XCL_PERF_MON_ACCEL, numComputeUnits);

    mProfileRunning = true;
  }

  // End device profiling (for a given program)
  // Perform final read of counters and force flush of trace buffers
  void Profiler::endDeviceProfiling()
  {
    // Only needs to be called once
    if (mEndDeviceProfilingCalled)
   	  return;

    auto rts = xdp::RTSingleton::Instance();

    if (rts && rts->applicationProfilingOn()) {
      // Write end of app event to trace buffer (Zynq only)
      xdp::profile::platform::write_host_event(rts->getcl_platform_id(),
          XCL_PERF_MON_END_EVENT, XCL_PERF_MON_PROGRAM_END);

      XOCL_DEBUGF("Final calls to read device counters and trace\n");

      xdp::profile::platform::log_device_counters(rts->getcl_platform_id(),XCL_PERF_MON_MEMORY, false, true);

      // Only called for hw emulation
      // Log accel trace before data trace as that is used for timestamp calculations
      if (rts->isHwEmu()) {
        xdp::profile::platform::log_device_counters(rts->getcl_platform_id(),XCL_PERF_MON_ACCEL, true, true);
        rts->logFinalTrace(XCL_PERF_MON_ACCEL);
        xdp::profile::platform::log_device_counters(rts->getcl_platform_id(),XCL_PERF_MON_STR, true, true);
        rts->logFinalTrace(XCL_PERF_MON_STR);
      }

      rts->logFinalTrace(XCL_PERF_MON_MEMORY);

      // Gather info for guidance
      // NOTE: this needs to be done here before the device clears its list of CUs
      // See xocl::device::unload_program as called from xocl::program::~program
      Plugin->getGuidanceMetadata( rts->getProfileManager() );

      // Record that this was called indirectly by host code
      mEndDeviceProfilingCalled = true;
    }
  }

  // Get timestamp difference in usec (used for debug)
  uint32_t
  Profiler::getTimeDiffUsec(std::chrono::steady_clock::time_point start,
                            std::chrono::steady_clock::time_point end)
  {
    using namespace std::chrono;
    // using duration_us = duration<uint64_t, std::ratio<1, 1000000>>;
    typedef duration<uint64_t, std::ratio<1, 1000000>> duration_us;
    duration_us time_span = duration_cast<duration_us>(end - start);
    return time_span.count();
  }

  // Get device counters
  void Profiler::getDeviceCounters(bool firstReadAfterProgram, bool forceReadCounters)
  {
    auto rts = xdp::RTSingleton::Instance();
    if (!Instance()->isProfileRunning() || !rts->deviceCountersProfilingOn())
      return;

    XOCL_DEBUGF("getDeviceCounters: START (firstRead: %d, forceRead: %d)\n", firstReadAfterProgram, forceReadCounters);

    xdp::profile::platform::log_device_counters(rts->getcl_platform_id(),XCL_PERF_MON_MEMORY,
                                                firstReadAfterProgram, forceReadCounters);

    XOCL_DEBUGF("getDeviceCounters: END\n");
  }

  // Get device trace
  void Profiler::getDeviceTrace(bool forceReadTrace)
  {
    auto rts = xdp::RTSingleton::Instance();
    if (!Instance()->isProfileRunning() || (!rts->deviceTraceProfilingOn() && !rts->isHwEmu()))
      return;

    XOCL_DEBUGF("getDeviceTrace: START (forceRead: %d)\n", forceReadTrace);

    if (rts->deviceTraceProfilingOn())
      xdp::profile::platform::log_device_trace(rts->getcl_platform_id(),XCL_PERF_MON_MEMORY, forceReadTrace);

    if (rts->isHwEmu())
      xdp::profile::platform::log_device_trace(rts->getcl_platform_id(),XCL_PERF_MON_ACCEL, forceReadTrace);

    XOCL_DEBUGF("getDeviceTrace: END\n");
  }

  /*
   * Callback functions called from xocl
   */
  void cb_get_device_trace(bool forceReadTrace)
  {
    Profiling::Profiler::Instance()->getDeviceTrace(forceReadTrace);
  }

  void cb_get_device_counters(bool firstReadAfterProgram, bool forceReadCounters)
  {
    Profiling::Profiler::Instance()->getDeviceCounters(firstReadAfterProgram, firstReadAfterProgram);
  }

  void cb_start_device_profiling(size_t numComputeUnits)
  {
    Profiling::Profiler::Instance()->startDeviceProfiling(numComputeUnits);
  }

  void cb_reset_device_profiling()
  {
    // Reset profiling flag
    Profiling::Profiler::Instance()->resetDeviceProfilingFlag();
  }

  void cb_end_device_profiling()
  {
    Profiling::Profiler::Instance()->endDeviceProfiling();
  }

} // Profiling
