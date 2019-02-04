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

#include "ocl_profiler.h"
#include "xdp/profile/config.h"
#include "xdp/profile/core/rt_profile.h"
#include "xrt/util/config_reader.h"
#include "xrt/util/message.h"
#include "driver/include/xclperf.h"

namespace xdp {

  static bool pDead = false;

  OCLProfiler* OCLProfiler::Instance()
  {
    if (pDead) {
      std::cout << "OCLProfiler is dead\n";
      return nullptr;
    }
    static OCLProfiler singleton;
    return &singleton;
  };

  OCLProfiler::OCLProfiler()
  : ProfileFlags( 0 )
  {
    Platform = xocl::get_shared_platform();
    Plugin = std::make_shared<XoclPlugin>(getclPlatformID());
    // Share ownership to ensure correct order of destruction
    ProfileMgr = std::make_unique<RTProfile>(ProfileFlags, Plugin);
    startProfiling();
  }

  OCLProfiler::~OCLProfiler()
  {
    Plugin->setObjectsReleased(mEndDeviceProfilingCalled);

    if (!mEndDeviceProfilingCalled && applicationProfilingOn()) {
      xrt::message::send(xrt::message::severity_level::WARNING,
          "Profiling may contain incomplete information. Please ensure all OpenCL objects are released by your host code (e.g., clReleaseProgram()).");

      // Before deleting, do a final read of counters and force flush of trace buffers
      endDeviceProfiling();
    }
    endProfiling();
    pDead = true;
  }

  // Start device profiling
  void OCLProfiler::startDeviceProfiling(size_t numComputeUnits)
  {
    auto platform = getclPlatformID();
    // Start counters
    if (deviceCountersProfilingOn())
      xoclp::platform::start_device_counters(platform, XCL_PERF_MON_MEMORY);

    // Start trace
    if (deviceTraceProfilingOn())
      xoclp::platform::start_device_trace(platform, XCL_PERF_MON_MEMORY, numComputeUnits);

    if ((Plugin->getFlowMode() == xdp::RTUtil::HW_EM))
      xoclp::platform::start_device_trace(platform, XCL_PERF_MON_ACCEL, numComputeUnits);

    mProfileRunning = true;
  }

  // End device profiling (for a given program)
  // Perform final read of counters and force flush of trace buffers
  void OCLProfiler::endDeviceProfiling()
  {
    // Only needs to be called once
    if (mEndDeviceProfilingCalled)
   	  return;
    
    auto platform = getclPlatformID();
    if (applicationProfilingOn()) {
      // Write end of app event to trace buffer (Zynq only)
      xoclp::platform::write_host_event(platform,
          XCL_PERF_MON_END_EVENT, XCL_PERF_MON_PROGRAM_END);

      XOCL_DEBUGF("Final calls to read device counters and trace\n");

      xoclp::platform::log_device_counters(platform, XCL_PERF_MON_MEMORY, false, true);

      // Only called for hw emulation
      // Log accel trace before data trace as that is used for timestamp calculations
      if ((Plugin->getFlowMode() == xdp::RTUtil::HW_EM)) {
        xoclp::platform::log_device_counters(platform, XCL_PERF_MON_ACCEL, true, true);
        logFinalTrace(XCL_PERF_MON_ACCEL);
        xoclp::platform::log_device_counters(platform, XCL_PERF_MON_STR, true, true);
        logFinalTrace(XCL_PERF_MON_STR);
      }

      logFinalTrace(XCL_PERF_MON_MEMORY);

      // Gather info for guidance
      // NOTE: this needs to be done here before the device clears its list of CUs
      // See xocl::device::unload_program as called from xocl::program::~program
      Plugin->getGuidanceMetadata( getProfileManager() );

      // Record that this was called indirectly by host code
      mEndDeviceProfilingCalled = true;
    }
  }

  // Get timestamp difference in usec (used for debug)
  uint32_t
  OCLProfiler::getTimeDiffUsec(std::chrono::steady_clock::time_point start,
                            std::chrono::steady_clock::time_point end)
  {
    using namespace std::chrono;
    // using duration_us = duration<uint64_t, std::ratio<1, 1000000>>;
    typedef duration<uint64_t, std::ratio<1, 1000000>> duration_us;
    duration_us time_span = duration_cast<duration_us>(end - start);
    return time_span.count();
  }

  // Get device counters
  void OCLProfiler::getDeviceCounters(bool firstReadAfterProgram, bool forceReadCounters)
  {
    if (!isProfileRunning() || !deviceCountersProfilingOn())
      return;

    XOCL_DEBUGF("getDeviceCounters: START (firstRead: %d, forceRead: %d)\n",
                 firstReadAfterProgram, forceReadCounters);

    xoclp::platform::log_device_counters(getclPlatformID(),XCL_PERF_MON_MEMORY,
                                                firstReadAfterProgram, forceReadCounters);

    XOCL_DEBUGF("getDeviceCounters: END\n");
  }

  // Get device trace
  void OCLProfiler::getDeviceTrace(bool forceReadTrace)
  {
    auto platform = getclPlatformID();
    if (!isProfileRunning() || 
        (!deviceTraceProfilingOn() && !(Plugin->getFlowMode() == xdp::RTUtil::HW_EM) ))
      return;

    XOCL_DEBUGF("getDeviceTrace: START (forceRead: %d)\n", forceReadTrace);

    if (deviceTraceProfilingOn())
      xoclp::platform::log_device_trace(platform, XCL_PERF_MON_MEMORY, forceReadTrace);

    if ((Plugin->getFlowMode() == xdp::RTUtil::HW_EM))
      xoclp::platform::log_device_trace(platform, XCL_PERF_MON_ACCEL, forceReadTrace);

    XOCL_DEBUGF("getDeviceTrace: END\n");
  }

  // Turn on/off profiling
  void OCLProfiler::turnOnProfile(xdp::RTUtil::e_profile_mode mode) {
    ProfileFlags |= mode;
    ProfileMgr->turnOnProfile(mode);
  }

  void OCLProfiler::turnOffProfile(xdp::RTUtil::e_profile_mode mode)
  {
    ProfileFlags &= ~mode;
    ProfileMgr->turnOffProfile(mode);
  }

    // Kick off profiling and open writers
  void OCLProfiler::startProfiling() {
    if (xrt::config::get_profile() == false)
      return;

    // Turn on device profiling (as requested)
    std::string data_transfer_trace = xrt::config::get_data_transfer_trace();
    std::string stall_trace = xrt::config::get_stall_trace();
    bool isEmulationOn = (std::getenv("XCL_EMULATION_MODE")) ? true : false;

    // Turn on application profiling
    turnOnProfile(xdp::RTUtil::PROFILE_APPLICATION);
    turnOnProfile(xdp::RTUtil::PROFILE_DEVICE_COUNTERS);
    // HW trace is controlled at HAL layer
    if (!(isEmulationOn) || (data_transfer_trace.find("off") == std::string::npos)) {
      turnOnProfile(xdp::RTUtil::PROFILE_DEVICE_TRACE);
    }

    ProfileMgr->setTransferTrace(data_transfer_trace);
    ProfileMgr->setStallTrace(stall_trace);

    // Enable profile summary if profile is on
    std::string profileFile("sdaccel_profile_summary");
    ProfileMgr->turnOnFile(xdp::RTUtil::FILE_SUMMARY);
    xdp::CSVProfileWriter* csvProfileWriter = new xdp::CSVProfileWriter(profileFile, "Xilinx", Plugin.get());
    ProfileWriters.push_back(csvProfileWriter);
    ProfileMgr->attach(csvProfileWriter);

    // Enable Trace File if profile is on and trace is enabled
    std::string timelineFile("");
    if (xrt::config::get_timeline_trace()) {
      timelineFile = "sdaccel_timeline_trace";
      ProfileMgr->turnOnFile(xdp::RTUtil::FILE_TIMELINE_TRACE);
    }
    xdp::CSVTraceWriter* csvTraceWriter = new xdp::CSVTraceWriter(timelineFile, "Xilinx", Plugin.get());
    TraceWriters.push_back(csvTraceWriter);
    ProfileMgr->attach(csvTraceWriter);

    // In Testing
    if (std::getenv("SDX_NEW_PROFILE")) {
      std::string profileFile2("sdx_profile_summary");
      std::string timelineFile2("sdx_timeline_trace");
      xdp::UnifiedCSVProfileWriter* csvProfileWriter2 = new xdp::UnifiedCSVProfileWriter(profileFile2, "Xilinx", Plugin.get());
      ProfileWriters.push_back(csvProfileWriter2);
      ProfileMgr->attach(csvProfileWriter2);
    }

    // Add functions to callback for profiling kernel/CU scheduling
    xocl::add_command_start_callback(xoclp::get_cu_start);
    xocl::add_command_done_callback(xoclp::get_cu_done);
  }

  // Wrap up profiling by writing files
  void OCLProfiler::endProfiling()
  {
    configureWriters();
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

  void OCLProfiler::configureWriters()
  {
    if (applicationTraceOn()) {
      setTraceFooterString();
    }
    // These tables are only enabled if a compatible monitor is present
    unsigned numStallSlots = 0;
    unsigned numStreamSlots = 0;
    if (applicationProfilingOn() && ProfileMgr->isDeviceProfileOn()) {
      for (auto device_id : Platform->get_device_range()) {
        std::string deviceName = device_id->get_unique_name();
        numStallSlots += xoclp::platform::get_profile_num_slots(getclPlatformID(),
                                                                deviceName,
                                                                XCL_PERF_MON_STALL);
        numStreamSlots += xoclp::platform::get_profile_num_slots(getclPlatformID(),
                                                                 deviceName,
                                                                 XCL_PERF_MON_STR);
      }
      for (auto& w: ProfileWriters) {
        if (Plugin->getFlowMode() == RTUtil::DEVICE && numStallSlots > 0) {
          w->enableStallTable();
        }
        if ((Plugin->getFlowMode() == RTUtil::DEVICE ||
              Plugin->getFlowMode() == RTUtil::HW_EM) &&
            numStreamSlots > 0) {
          w->enableStreamTable();
        }
      }
    }
  }

  // Log final trace for a given profile type
  // NOTE: this is a bit tricky since trace logging is accessed by multiple
  // threads. We have to wait since this is the only place where we flush.
  void OCLProfiler::logFinalTrace(xclPerfMonType type) {
    const unsigned int wait_msec = 1;
    const unsigned int max_iter = 100;
    unsigned int iter = 0;
    cl_int ret = -1;

    while (ret == -1 && iter < max_iter) {
      ret = xoclp::platform::log_device_trace(getclPlatformID(),type, true);
      if (ret == -1)
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_msec));
      iter++;
    }
    XDP_LOG("Trace logged for type %d after %d iterations\n", type, iter);
  }

  void OCLProfiler::setTraceFooterString() {
    std::stringstream trs;
    trs << "Project," << ProfileMgr->getProjectName() << ",\n";
    std::string stallProfiling = (ProfileMgr->getStallTrace() == xdp::RTUtil::STALL_TRACE_OFF) ? "false" : "true";
    trs << "Stall profiling," << stallProfiling << ",\n";
    std::string flowMode;
    xdp::RTUtil::getFlowModeName(Plugin->getFlowMode(), flowMode);
    trs << "Target," << flowMode << ",\n";
    std::string deviceNames = ProfileMgr->getDeviceNames("|");
    trs << "Platform," << deviceNames << ",\n";
    for (auto& threadId : ProfileMgr->getThreadIds())
      trs << "Read/Write Thread," << std::showbase << std::hex << std::uppercase
	      << threadId << std::endl;
    //
    // Platform/device info
    //
    auto platform = getclPlatformID();
    for (auto device_id : platform->get_device_range()) {
      std::string deviceName = device_id->get_unique_name();
      trs << "Device," << deviceName << ",begin\n";

      // DDR Bank addresses
      // TODO: this assumes start address of 0x0 and evenly divided banks
      unsigned int ddrBanks = device_id->get_ddr_bank_count();
      if (ddrBanks == 0) ddrBanks = 1;
      size_t ddrSize = device_id->get_ddr_size();
      size_t bankSize = ddrSize / ddrBanks;
      trs << "DDR Banks,begin\n";
      for (unsigned int b=0; b < ddrBanks; ++b)
        trs << "Bank," << std::dec << b << ","
		    << (boost::format("0X%09x") % (b * bankSize)) << std::endl;
      trs << "DDR Banks,end\n";
      trs << "Device," << deviceName << ",end\n";
    }
    //
    // Unused CUs
    //
    for (auto device_id : platform->get_device_range()) {
      std::string deviceName = device_id->get_unique_name();

      for (auto& cu : xocl::xocl(device_id)->get_cus()) {
        auto cuName = cu->get_name();

        if (ProfileMgr->getComputeUnitCalls(deviceName, cuName) == 0)
          trs << "UnusedComputeUnit," << cuName << ",\n";
      }
    }
    Plugin->setTraceFooterString(trs.str());
  }

  // Add to the active devices
  // Called thru device::load_program in xocl/core/device.cpp
  // NOTE: this is the entry point into XDP when a new device gets loaded
  void OCLProfiler::addToActiveDevices(const std::string& deviceName)
  {
    XDP_LOG("addToActiveDevices: device = %s\n", deviceName.c_str());
    // Store name of device to profiler
    ProfileMgr->addDeviceName(deviceName);
    // TODO: Grab device-level metadata here!!! (e.g., device name, CU/kernel names)
  }

  void OCLProfiler::setKernelClockFreqMHz(const std::string &deviceName, unsigned int clockRateMHz)
  {
    if (applicationProfilingOn()) {
      ProfileMgr->setTraceClockFreqMHz(clockRateMHz);
      Plugin->setKernelClockFreqMHz(deviceName, clockRateMHz);
    }
  }

  /*
   * Callback functions called from xocl
   */
  void cb_get_device_trace(bool forceReadTrace)
  {
    OCLProfiler::Instance()->getDeviceTrace(forceReadTrace);
  }

  void cb_get_device_counters(bool firstReadAfterProgram, bool forceReadCounters)
  {
    OCLProfiler::Instance()->getDeviceCounters(firstReadAfterProgram, firstReadAfterProgram);
  }

  void cb_start_device_profiling(size_t numComputeUnits)
  {
    OCLProfiler::Instance()->startDeviceProfiling(numComputeUnits);
  }

  void cb_reset_device_profiling()
  {
    // Reset profiling flag
    OCLProfiler::Instance()->resetDeviceProfilingFlag();
  }

  void cb_end_device_profiling()
  {
    OCLProfiler::Instance()->endDeviceProfiling();
  }

} // Profiling
