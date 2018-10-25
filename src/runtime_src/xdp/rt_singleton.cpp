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

#include "rt_singleton.h"
#include "xdp/appdebug/appdebug.h"
#include "xocl/core/platform.h"
#include "xocl/core/execution_context.h"
#include "xrt/util/config_reader.h"

#include "xdp/profile/profile.h"
#include "xdp/profile/rt_profile.h"
#include "xdp/profile/rt_profile_writers.h"
#include "xdp/profile/rt_profile_xocl.h"

#include <cstdlib>
#include <cstdio>
#include <string>
#include <chrono>
#include <iostream>

namespace XCL {

  static bool gActive = false;
  static bool gDead = false;

  bool
  active() {
    return gActive;
  }

  RTSingleton* 
  RTSingleton::Instance() {
    if (gDead) {
      std::cout << "RTSingleton is dead\n";
      return nullptr;
    }
    
    static RTSingleton singleton;
    return &singleton;
  }

  RTSingleton::RTSingleton()
  : Status( CL_SUCCESS ),
    Platform( nullptr ),
    ProfileMgr( nullptr ),
    DebugMgr( nullptr ),
    ProfileFlags( 0 )
  {
    ProfileMgr = new RTProfile(ProfileFlags);
    startProfiling();

    DebugMgr = new RTDebug();

    // share ownership of the global platform
    Platform = xocl::get_shared_platform();

    if (xrt::config::get_app_debug()) {
      appdebug::register_xocl_appdebug_callbacks();
    }

    if (applicationProfilingOn()) {
      XCL::register_xocl_profile_callbacks();
    }
#ifdef PMD_OCL
    return;
#endif

    gActive = true;
  };

  RTSingleton::~RTSingleton() {
    gActive = false;

    endProfiling();

    gDead = true;

    // Destruct in reverse order of construction
    delete ProfileMgr;
    delete DebugMgr;
  }

  // Kick off profiling and open writers
  void RTSingleton::startProfiling() {
    if (xrt::config::get_profile() == false)
      return;

    // Find default flow mode
    // NOTE: it will be modified in clCreateProgramWithBinary (if run)
    FlowMode = (std::getenv("XCL_EMULATION_MODE")) ? HW_EM : DEVICE;

    // Turn on application profiling
    turnOnProfile(RTProfile::PROFILE_APPLICATION);

    // Turn on device profiling (as requested)
    std::string data_transfer_trace = xrt::config::get_data_transfer_trace();
    // TEMPORARY - TURN ON DATA TRANSFER TRACE WHEN TIMELINE TRACE IS ON (HW EM ONLY)
    //if ((FlowMode == HW_EM) && xrt::config::get_timeline_trace())
    //  data_transfer_trace = "fine";

    std::string stall_trace = xrt::config::get_stall_trace();
    ProfileMgr->setTransferTrace(data_transfer_trace);
    ProfileMgr->setStallTrace(stall_trace);

    turnOnProfile(RTProfile::PROFILE_DEVICE_COUNTERS);
    // HW trace is controlled at HAL layer
    if ((FlowMode == DEVICE) || xrt::config::get_device_profile() ||
        (data_transfer_trace.find("off") == std::string::npos)) {
      turnOnProfile(RTProfile::PROFILE_DEVICE_TRACE);
    }

    // Issue warning for device_profile setting (not supported after 2018.2)
    if (xrt::config::get_device_profile()) {
      xrt::message::send(xrt::message::severity_level::WARNING,
          "The setting device_profile will be deprecated after 2018.2. Please use data_transfer_trace.");
    }

    std::string profileFile("");
    std::string profileFile2("");
    std::string timelineFile("");
    std::string timelineFile2("");

    if (ProfileMgr->isApplicationProfileOn()) {
      //always on by default.
      ProfileMgr->turnOnFile(RTProfile::FILE_SUMMARY);
      profileFile = "sdaccel_profile_summary";
      profileFile2 = "sdx_profile_summary";
    }

    if (xrt::config::get_timeline_trace()) {
      ProfileMgr->turnOnFile(RTProfile::FILE_TIMELINE_TRACE);
      timelineFile = "sdaccel_timeline_trace";
      timelineFile2 = "sdx_timeline_trace";
    }

    // HTML and CSV writers
    //HTMLWriter* htmlWriter = new HTMLWriter(profileFile, timelineFile, "Xilinx");
    CSVWriter* csvWriter = new CSVWriter(profileFile, timelineFile, "Xilinx");

    //Writers.push_back(htmlWriter);
    Writers.push_back(csvWriter);

    //ProfileMgr->attach(htmlWriter);
    ProfileMgr->attach(csvWriter);

    if (std::getenv("SDX_NEW_PROFILE")) {
      UnifiedCSVWriter* csvWriter2 = new UnifiedCSVWriter(profileFile2, timelineFile2, "Xilinx");
      Writers.push_back(csvWriter2);
      ProfileMgr->attach(csvWriter2);
    }

    // Add functions to callback for profiling kernel/CU scheduling
    xocl::add_command_start_callback(xdp::profile::get_cu_start);
    xocl::add_command_done_callback(xdp::profile::get_cu_done);
  }

  // Wrap up profiling by writing files
  void RTSingleton::endProfiling() {
    if (applicationProfilingOn()) {
      // Write out reports
      ProfileMgr->writeProfileSummary();

      // Close writers
      for (auto& w: Writers) {
        ProfileMgr->detach(w);
        delete w;
      }
    }
  }

  // Log final trace for a given profile type
  // NOTE: this is a bit tricky since trace logging is accessed by multiple
  // threads. We have to wait since this is the only place where we flush.
  void RTSingleton::logFinalTrace(xclPerfMonType type) {
    const unsigned int wait_msec = 1;
    const unsigned int max_iter = 100;
    unsigned int iter = 0;
    cl_int ret = -1;

    while (ret == -1 && iter < max_iter) {
      ret = xdp::profile::platform::log_device_trace(Platform.get(),type, true);
      if (ret == -1) 
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_msec));
      iter++;
    }
    XOCL_DEBUGF("Trace logged for type %d after %d iterations\n", type, iter);
  }

  unsigned RTSingleton::getProfileNumberSlots(xclPerfMonType type, std::string& deviceName) {
    unsigned numSlots = xdp::profile::platform::get_profile_num_slots(Platform.get(),
        deviceName, type);
    //XOCL_DEBUG(std::cout,"Profiling: type = "type," slots = ",numSlots,"\n");
    return numSlots;
  }

  void RTSingleton::getProfileSlotName(xclPerfMonType type, std::string& deviceName,
                                       unsigned slotnum, std::string& slotName) {
    xdp::profile::platform::get_profile_slot_name(Platform.get(), deviceName,
        type, slotnum, slotName);
    //XOCL_DEBUG(std::cout,"Profiling: type = "type," slot ",slotnum," name = ",slotName.c_str(),"\n");
  }

  unsigned RTSingleton::getProfileSlotProperties(xclPerfMonType type, std::string& deviceName, unsigned slotnum) {
    return xdp::profile::platform::get_profile_slot_properties(Platform.get(), deviceName, type, slotnum);
  }

  void RTSingleton::getProfileKernelName(const std::string& deviceName, const std::string& cuName, std::string& kernelName) {
    xdp::profile::platform::get_profile_kernel_name(Platform.get(), deviceName, cuName, kernelName);
  }

  // Set OCL profile mode based on profile type string
  // NOTE: this corresponds to strings defined in regiongen_new/ipihandler.cxx
  void RTSingleton::setOclProfileMode(unsigned slotnum, std::string type) {
    if (slotnum >= XAPM_MAX_NUMBER_SLOTS)
	  return;

    XOCL_DEBUG(std::cout,"OCL profiling: mode for slot ",slotnum," = ",type.c_str(),"\n");

    if (type.find("stream") != std::string::npos || type.find("STREAM") != std::string::npos)
      OclProfileMode[slotnum] = STREAM;
    else if (type.find("pipe") != std::string::npos || type.find("PIPE") != std::string::npos)
      OclProfileMode[slotnum] = PIPE;
    else if (type.find("memory") != std::string::npos || type.find("MEMORY") != std::string::npos)
      OclProfileMode[slotnum] = MEMORY;
    else if (type.find("activity") != std::string::npos || type.find("ACTIVITY") != std::string::npos)
      OclProfileMode[slotnum] = ACTIVITY;
    else
      OclProfileMode[slotnum] = NONE;
  }

  size_t RTSingleton::getDeviceTimestamp(std::string& deviceName) {
    return xdp::profile::platform::get_device_timestamp(Platform.get(),deviceName);
  }

  double RTSingleton::getReadMaxBandwidthMBps() {
    return xdp::profile::platform::get_device_max_read(Platform.get());
  }

  double RTSingleton::getWriteMaxBandwidthMBps() {
    return xdp::profile::platform::get_device_max_write(Platform.get());
  }

  void RTSingleton::getFlowModeName(std::string& str) {
    if (FlowMode == CPU)
      str = "CPU Emulation";
    else if (FlowMode == COSIM_EM)
      str = "Co-Sim Emulation";
    else if (FlowMode == HW_EM)
      str = "Hardware Emulation";
    else
      str = "System Run";
  }
};



