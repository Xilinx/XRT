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
#include "xrt/util/config_reader.h"

#include "driver/include/xclperf.h"

#include "xrt/util/message.h"

namespace Profiling {

  static bool pDead = false;

  Profiler* Profiler::Instance()
  {
    if (pDead) {
      std::cout << "Profiler is dead\n";
      return nullptr;
    }
    static Profiler singleton;
    return &singleton;
  };

  Profiler::Profiler()
  : ProfileFlags( 0 )
  {
    Plugin = new xdp::XoclPlugin();
    ProfileMgr = new xdp::RTProfile(ProfileFlags, Plugin);
    startProfiling();
  }

  Profiler::~Profiler()
  {
    pDead = true;
    setObjectsReleased(mEndDeviceProfilingCalled);

    if (!mEndDeviceProfilingCalled && applicationProfilingOn()) {
      xrt::message::send(xrt::message::severity_level::WARNING,
          "Profiling may contain incomplete information. Please ensure all OpenCL objects are released by your host code (e.g., clReleaseProgram()).");

      // Before deleting, do a final read of counters and force flush of trace buffers
      endDeviceProfiling();
    }
    endProfiling();
    // Destruct in reverse order of construction
    delete ProfileMgr;
    delete Plugin;
  }

  // Start device profiling
  void Profiler::startDeviceProfiling(size_t numComputeUnits)
  {
    auto rts = xdp::RTSingleton::Instance();
    // Start counters
    if (deviceCountersProfilingOn())
      xdp::profile::platform::start_device_counters(rts->getcl_platform_id(),XCL_PERF_MON_MEMORY);

    // Start trace
    if (deviceTraceProfilingOn())
      xdp::profile::platform::start_device_trace(rts->getcl_platform_id(),XCL_PERF_MON_MEMORY, numComputeUnits);

    if ((Plugin->getFlowMode() == xdp::RTUtil::HW_EM))
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

    if (applicationProfilingOn()) {
      // Write end of app event to trace buffer (Zynq only)
      xdp::profile::platform::write_host_event(rts->getcl_platform_id(),
          XCL_PERF_MON_END_EVENT, XCL_PERF_MON_PROGRAM_END);

      XOCL_DEBUGF("Final calls to read device counters and trace\n");

      xdp::profile::platform::log_device_counters(rts->getcl_platform_id(),XCL_PERF_MON_MEMORY, false, true);

      // Only called for hw emulation
      // Log accel trace before data trace as that is used for timestamp calculations
      if ((Plugin->getFlowMode() == xdp::RTUtil::HW_EM)) {
        xdp::profile::platform::log_device_counters(rts->getcl_platform_id(),XCL_PERF_MON_ACCEL, true, true);
        logFinalTrace(XCL_PERF_MON_ACCEL);
        xdp::profile::platform::log_device_counters(rts->getcl_platform_id(),XCL_PERF_MON_STR, true, true);
        logFinalTrace(XCL_PERF_MON_STR);
      }

      logFinalTrace(XCL_PERF_MON_MEMORY);

      // Gather info for guidance
      // NOTE: this needs to be done here before the device clears its list of CUs
      // See xocl::device::unload_program as called from xocl::program::~program
      Plugin->getGuidanceMetadata( ProfileMgr );

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
    if (!Instance()->isProfileRunning() || !deviceCountersProfilingOn())
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
    if (!Instance()->isProfileRunning() || (!deviceTraceProfilingOn() && !(Plugin->getFlowMode() == xdp::RTUtil::HW_EM)))
      return;

    XOCL_DEBUGF("getDeviceTrace: START (forceRead: %d)\n", forceReadTrace);

    if (deviceTraceProfilingOn())
      xdp::profile::platform::log_device_trace(rts->getcl_platform_id(),XCL_PERF_MON_MEMORY, forceReadTrace);

    if ((Plugin->getFlowMode() == xdp::RTUtil::HW_EM))
      xdp::profile::platform::log_device_trace(rts->getcl_platform_id(),XCL_PERF_MON_ACCEL, forceReadTrace);

    XOCL_DEBUGF("getDeviceTrace: END\n");
  }

  // Turn on/off profiling
  void Profiler::turnOnProfile(xdp::RTUtil::e_profile_mode mode) {
    ProfileFlags |= mode;
    ProfileMgr->turnOnProfile(mode);
  }

  void Profiler::turnOffProfile(xdp::RTUtil::e_profile_mode mode)
  {
    ProfileFlags &= ~mode;
    ProfileMgr->turnOffProfile(mode);
  }

    // Kick off profiling and open writers
  void Profiler::startProfiling() {
    if (xrt::config::get_profile() == false)
      return;

    // Turn on application profiling
    turnOnProfile(xdp::RTUtil::PROFILE_APPLICATION);

    // Turn on device profiling (as requested)
    std::string data_transfer_trace = xrt::config::get_data_transfer_trace();

    std::string stall_trace = xrt::config::get_stall_trace();
    ProfileMgr->setTransferTrace(data_transfer_trace);
    ProfileMgr->setStallTrace(stall_trace);

    turnOnProfile(xdp::RTUtil::PROFILE_DEVICE_COUNTERS);
    // HW trace is controlled at HAL layer
    bool isEmulationOn = (std::getenv("XCL_EMULATION_MODE")) ? true : false;
    if (!(isEmulationOn) || (data_transfer_trace.find("off") == std::string::npos)) {
      turnOnProfile(xdp::RTUtil::PROFILE_DEVICE_TRACE);
    }

    std::string profileFile("");
    std::string profileFile2("");
    std::string timelineFile("");
    std::string timelineFile2("");

    if (ProfileMgr->isApplicationProfileOn()) {
      ProfileMgr->turnOnFile(xdp::RTUtil::FILE_SUMMARY);
      profileFile = "sdaccel_profile_summary";
      profileFile2 = "sdx_profile_summary";
    }

    if (xrt::config::get_timeline_trace()) {
      ProfileMgr->turnOnFile(xdp::RTUtil::FILE_TIMELINE_TRACE);
      timelineFile = "sdaccel_timeline_trace";
      timelineFile2 = "sdx_timeline_trace";
    }

    // CSV writers
    xdp::CSVProfileWriter* csvProfileWriter = new xdp::CSVProfileWriter(profileFile, "Xilinx", Plugin);
    xdp::CSVTraceWriter*   csvTraceWriter   = new xdp::CSVTraceWriter(timelineFile, "Xilinx", Plugin);

    ProfileWriters.push_back(csvProfileWriter);
    TraceWriters.push_back(csvTraceWriter);

    ProfileMgr->attach(csvProfileWriter);
    ProfileMgr->attach(csvTraceWriter);

    if (std::getenv("SDX_NEW_PROFILE")) {
      xdp::UnifiedCSVProfileWriter* csvProfileWriter2 = new xdp::UnifiedCSVProfileWriter(profileFile2, "Xilinx", Plugin);
      ProfileWriters.push_back(csvProfileWriter2);
      ProfileMgr->attach(csvProfileWriter2);
    }

    // Add functions to callback for profiling kernel/CU scheduling
    xocl::add_command_start_callback(xdp::profile::get_cu_start);
    xocl::add_command_done_callback(xdp::profile::get_cu_done);
  }

  // Wrap up profiling by writing files
  void Profiler::endProfiling()
  {
    if (applicationProfilingOn()) {
      // Write out reports
      ProfileMgr->writeProfileSummary();

      // Close writers
      for (auto& w: ProfileWriters) {
        ProfileMgr->detach(w);
        delete w;
      }
      for (auto& w: TraceWriters) {
        ProfileMgr->detach(w);
        delete w;
      }
    }
  }

  // Log final trace for a given profile type
  // NOTE: this is a bit tricky since trace logging is accessed by multiple
  // threads. We have to wait since this is the only place where we flush.
  void Profiler::logFinalTrace(xclPerfMonType type) {
    const unsigned int wait_msec = 1;
    const unsigned int max_iter = 100;
    unsigned int iter = 0;
    cl_int ret = -1;

    auto rts = xdp::RTSingleton::Instance();

    while (ret == -1 && iter < max_iter) {
      ret = xdp::profile::platform::log_device_trace(rts->getcl_platform_id(),type, true);
      if (ret == -1)
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_msec));
      iter++;
    }
    XDP_LOG("Trace logged for type %d after %d iterations\n", type, iter);
  }

  // Add to the active devices
  // Called thru device::load_program in xocl/core/device.cpp
  // NOTE: this is the entry point into XDP when a new device gets loaded
  void Profiler::addToActiveDevices(const std::string& deviceName)
  {
    XDP_LOG("addToActiveDevices: device = %s\n", deviceName.c_str());
    // Store name of device to profiler
    ProfileMgr->addDeviceName(deviceName);
    // TODO: Grab device-level metadata here!!! (e.g., device name, CU/kernel names)
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
