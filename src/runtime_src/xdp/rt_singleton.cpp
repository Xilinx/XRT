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

#include "rt_singleton.h"

#include "xdp/profile/writer/csv_profile.h"
#include "xdp/profile/writer/csv_trace.h"
#include "xdp/profile/writer/unified_csv_profile.h"
#include "xdp/appdebug/appdebug.h"

// TODO: remove these dependencies
#include "xrt/util/config_reader.h"
#include "xdp/profile/plugin/ocl/xocl_plugin.h"
#include "xdp/profile/plugin/ocl/xocl_profile.h"
#include "xdp/profile/plugin/ocl/xocl_profile_cb.h"

namespace xdp {

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

    // Use base plugin as default
    // NOTE: this needs to be the base class once the owner of XoclPlugin
    //       is identified in plugins/ocl
    //Plugin = new XDPPluginI();
    Plugin = new XoclPlugin();

    // share ownership of the global platform
    Platform = xocl::get_shared_platform();

    if (xrt::config::get_app_debug()) {
      appdebug::register_xocl_appdebug_callbacks();
    }

    if (applicationProfilingOn()) {
      xdp::register_xocl_profile_callbacks();
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

  // Turn on/off profiling
  void RTSingleton::turnOnProfile(RTUtil::e_profile_mode mode) {
    ProfileFlags |= mode;
    ProfileMgr->turnOnProfile(mode);
  }

  void RTSingleton::turnOffProfile(RTUtil::e_profile_mode mode) {
    ProfileFlags &= ~mode;
    ProfileMgr->turnOffProfile(mode);
  }

  // Kick off profiling and open writers
  void RTSingleton::startProfiling() {
    if (xrt::config::get_profile() == false)
      return;

    // Find default flow mode
    // NOTE: it will be modified in clCreateProgramWithBinary (if run)
    FlowMode = (std::getenv("XCL_EMULATION_MODE")) ? HW_EM : DEVICE;

    // Turn on application profiling
    turnOnProfile(RTUtil::PROFILE_APPLICATION);

    // Turn on device profiling (as requested)
    std::string data_transfer_trace = xrt::config::get_data_transfer_trace();
    // TEMPORARY - TURN ON DATA TRANSFER TRACE WHEN TIMELINE TRACE IS ON (HW EM ONLY)
    //if ((FlowMode == HW_EM) && xrt::config::get_timeline_trace())
    //  data_transfer_trace = "fine";

    std::string stall_trace = xrt::config::get_stall_trace();
    ProfileMgr->setTransferTrace(data_transfer_trace);
    ProfileMgr->setStallTrace(stall_trace);

    turnOnProfile(RTUtil::PROFILE_DEVICE_COUNTERS);
    // HW trace is controlled at HAL layer
    if ((FlowMode == DEVICE) || (data_transfer_trace.find("off") == std::string::npos)) {
      turnOnProfile(RTUtil::PROFILE_DEVICE_TRACE);
    }

#if 0
    // Issue warning for device_profile setting (not supported after 2018.2)
    if (xrt::config::get_device_profile()) {
      xrt::message::send(xrt::message::severity_level::WARNING,
          "The setting device_profile will be deprecated after 2018.2. Please use data_transfer_trace.");
    }
#endif

    std::string profileFile("");
    std::string profileFile2("");
    std::string timelineFile("");
    std::string timelineFile2("");

    if (ProfileMgr->isApplicationProfileOn()) {
      ProfileMgr->turnOnFile(RTUtil::FILE_SUMMARY);
      profileFile = "sdaccel_profile_summary";
      profileFile2 = "sdx_profile_summary";
    }

    if (xrt::config::get_timeline_trace()) {
      ProfileMgr->turnOnFile(RTUtil::FILE_TIMELINE_TRACE);
      timelineFile = "sdaccel_timeline_trace";
      timelineFile2 = "sdx_timeline_trace";
    }

    // CSV writers
    CSVProfileWriter* csvProfileWriter = new CSVProfileWriter(profileFile, "Xilinx");
    CSVTraceWriter*   csvTraceWriter   = new CSVTraceWriter(timelineFile, "Xilinx");

    ProfileWriters.push_back(csvProfileWriter);
    TraceWriters.push_back(csvTraceWriter);

    ProfileMgr->attach(csvProfileWriter);
    ProfileMgr->attach(csvTraceWriter);

    if (std::getenv("SDX_NEW_PROFILE")) {
      UnifiedCSVProfileWriter* csvProfileWriter2 = new UnifiedCSVProfileWriter(profileFile2, "Xilinx");
      ProfileWriters.push_back(csvProfileWriter2);
      ProfileMgr->attach(csvProfileWriter2);
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
    XDP_LOG("Trace logged for type %d after %d iterations\n", type, iter);
  }

  unsigned RTSingleton::getProfileNumberSlots(xclPerfMonType type, std::string& deviceName) {
    unsigned numSlots = xdp::profile::platform::get_profile_num_slots(Platform.get(),
        deviceName, type);
    return numSlots;
  }

  void RTSingleton::getProfileSlotName(xclPerfMonType type, std::string& deviceName,
                                       unsigned slotnum, std::string& slotName) {
    xdp::profile::platform::get_profile_slot_name(Platform.get(), deviceName,
        type, slotnum, slotName);
  }

  unsigned RTSingleton::getProfileSlotProperties(xclPerfMonType type, std::string& deviceName, unsigned slotnum) {
    return xdp::profile::platform::get_profile_slot_properties(Platform.get(), deviceName, type, slotnum);
  }

  // Set OCL profile mode based on profile type string
  // NOTE: this corresponds to strings defined in regiongen_new/ipihandler.cxx
  void RTSingleton::setOclProfileMode(unsigned slotnum, std::string type) {
    if (slotnum >= XAPM_MAX_NUMBER_SLOTS)
	  return;

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

  // TODO: the next 3 functions should be moved to the plugin
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

  // Add to the active devices
  // Called thru device::load_program in xocl/core/device.cpp
  // NOTE: this is the entry point into XDP when a new device gets loaded
  void RTSingleton::addToActiveDevices(const std::string& deviceName)
  {
    XDP_LOG("addToActiveDevices: device = %s\n", deviceName.c_str());

    // Store arguments and banks for each CU and its ports
    Plugin->setArgumentsBank(deviceName);

    // Store name of device to profiler
    ProfileMgr->addDeviceName(deviceName);

    // TODO: Grab device-level metadata here!!! (e.g., device name, CU/kernel names)
  }

} // xdp
