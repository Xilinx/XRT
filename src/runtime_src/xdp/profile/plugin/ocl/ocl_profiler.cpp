/**
 * Copyright (C) 2016-2019 Xilinx, Inc
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

#include <sys/mman.h>

#include "ocl_profiler.h"
#include "xdp/profile/config.h"
#include "xdp/profile/core/rt_profile.h"
#include "xdp/profile/device/xdp_xrt_device.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/writer/json_profile.h"
#include "xdp/profile/writer/csv_profile.h"
#include "xrt/util/config_reader.h"
#include "xrt/util/message.h"
#include "xclperf.h"


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
      xrt::message::send(xrt::message::severity_level::XRT_WARNING,
          "Profiling may contain incomplete information. Please ensure all OpenCL objects are released by your host code (e.g., clReleaseProgram()).");

      // Before deleting, do a final read of counters and force flush of trace buffers
      endDeviceProfiling();
    }
    endProfiling();
    reset();
    pDead = true;
  }

  // Start device profiling
  void OCLProfiler::startDeviceProfiling(size_t numComputeUnits)
  {
    auto platform = getclPlatformID();

    // Start counters
    if (deviceCountersProfilingOn()) {
      startCounters();
    }

    // Start trace
    if (deviceTraceProfilingOn()) {
      startTrace();
    }

    // With new XDP flow, HW Emu should be similar to Device flow. So, multiple calls to trace/counters should not be needed.
    // But needed for older flow
    if ((Plugin->getFlowMode() == xdp::RTUtil::HW_EM) && Plugin->getSystemDPAEmulation() == false)
      xoclp::platform::start_device_trace(platform, XCL_PERF_MON_ACCEL, numComputeUnits);

    if ((Plugin->getFlowMode() == xdp::RTUtil::DEVICE)) {
      for (auto device : platform->get_device_range()) {
        auto power_profile = std::make_unique<OclPowerProfile>(device->get_xrt_device(), Plugin, device->get_unique_name());
        PowerProfileList.push_back(std::move(power_profile));
      }
    }
    mProfileRunning = true;
  }

  // End device profiling (for a given program)
  // Perform final read of counters and force flush of trace buffers
  void OCLProfiler::endDeviceProfiling()
  {
    // Only needs to be called once
    if (mEndDeviceProfilingCalled)
   	  return;

    if(!applicationProfilingOn()) {
      return;
    }

    // Log Counter Data
    logDeviceCounters(true, true, true);  // reads and logs device counters for all monitors in all flows

    // With new XDP flow, HW Emu should be similar to Device flow. So, multiple calls to trace/counters should not be needed.
    // But needed for older flow
    // Log Trace Data
    // Log accel trace before data trace as that is used for timestamp calculations
    if ((Plugin->getFlowMode() == xdp::RTUtil::HW_EM) && Plugin->getSystemDPAEmulation() == false) {
      logFinalTrace(XCL_PERF_MON_ACCEL);
      logFinalTrace(XCL_PERF_MON_STR);
    }

    logFinalTrace(XCL_PERF_MON_MEMORY /* type should not matter */);  // reads and logs trace data for all monitors in HW flow

    endTrace();

    // Gather info for guidance
    // NOTE: this needs to be done here before the device clears its list of CUs
    // See xocl::device::unload_program as called from xocl::program::~program
    Plugin->getGuidanceMetadata( getProfileManager() );

    // Record that this was called indirectly by host code
    mEndDeviceProfilingCalled = true;
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

  void OCLProfiler::startCounters()
  {
    auto platform = getclPlatformID();

    for (auto device : platform->get_device_range()) {
      if(!device->is_active()) {
        continue;
      }
      auto itr = DeviceData.find(device);
      if (itr==DeviceData.end()) {
        itr = DeviceData.emplace(device,xdp::xoclp::platform::device::data()).first;
      }
      DeviceIntf* dInt = nullptr;
      auto xdevice = device->get_xrt_device();
      if ((Plugin->getFlowMode() == xdp::RTUtil::DEVICE) || (Plugin->getFlowMode() == xdp::RTUtil::HW_EM && Plugin->getSystemDPAEmulation())) {
        dInt = &(itr->second.mDeviceIntf);
        dInt->setDevice(new xdp::XrtDevice(xdevice));
        dInt->readDebugIPlayout();
      }       
      xdp::xoclp::platform::device::data* info = &(itr->second);

      // Set clock etc.
      double deviceClockMHz = xdevice->getDeviceClock().get();
      if(deviceClockMHz > 0) {
          getProfileManager()->setDeviceClockFreqMHz(deviceClockMHz);
      }
      info->mPerformingFlush = false;

      // Reset and Start counters
      if(dInt) {
        dInt->startCounters(XCL_PERF_MON_MEMORY);
        /* Configure AMs if context monitoring is supported
         * else disable alll AM data
         */
        std::string ctx_info = xrt_core::config::get_kernel_channel_info();
        dInt->configAmContext(ctx_info);
        Plugin->setCtxEn(!ctx_info.empty());
      } else {
        xdevice->startCounters(XCL_PERF_MON_MEMORY);
      }

      info->mSampleIntervalMsec = getProfileManager()->getSampleIntervalMsec();

      // configureDataflow
      if(dInt && (Plugin->getFlowMode() == xdp::RTUtil::DEVICE)) {
        /* If CU corresponding to Accel Monitors has AP Control Chain, then enable Dataflow on the Accel Monitors */
        unsigned int numMon = dInt->getNumMonitors(XCL_PERF_MON_ACCEL);
        auto ip_config = std::make_unique <bool []>(numMon);
        for (unsigned int i=0; i < numMon; i++) {
          char name[128];
          dInt->getMonitorName(XCL_PERF_MON_ACCEL, i, name, 128);
          std::string cuName(name); // Assumption : For Accel Monitor, monitor instance is named as the corresponding CU

          /* this ip_config only tells whether the corresponding CU has ap_control_chain :
           * could have been just a property on the monitor set at compile time (in debug_ip_layout)
           * Currently, isApCtrlChain retrieves info from xocl::device and conpute_unit. So, could not be moved
           * into DeviceIntf as it uses xrt::device
           */
          ip_config[i] = xoclp::platform::device::isAPCtrlChain(device, cuName) ? true : false;
        }

        dInt->configureDataflow(ip_config.get());
      } else {
          xdp::xoclp::platform::device::configureDataflow(device,XCL_PERF_MON_MEMORY);    // this populates montior IP data which is needed by summary writer
      }
    } // for all active devices
  }

  void OCLProfiler::startTrace()
  {
    auto platform = getclPlatformID();
    std::string trace_memory = "FIFO";

    for (auto device : platform->get_device_range()) {
      if(!device->is_active()) {
        continue;
      }
      auto itr = DeviceData.find(device);
      if (itr==DeviceData.end()) {
        itr = DeviceData.emplace(device,xdp::xoclp::platform::device::data()).first;
      }

      auto xdevice = device->get_xrt_device();
      DeviceIntf* dInt = nullptr;
      if((Plugin->getFlowMode() == xdp::RTUtil::DEVICE) || (Plugin->getFlowMode() == xdp::RTUtil::HW_EM && Plugin->getSystemDPAEmulation())) {
        dInt = &(itr->second.mDeviceIntf);
        dInt->setDevice(new xdp::XrtDevice(xdevice));
        dInt->readDebugIPlayout();
      }
      xdp::xoclp::platform::device::data* info = &(itr->second);

      // Since clock training is performed in mStartTrace, let's record this time
      // XCL_PERF_MON_MEMORY // any type
      info->mLastTraceTrainingTime[XCL_PERF_MON_MEMORY] = std::chrono::steady_clock::now();
      info->mPerformingFlush = false;
      info->mLastTraceNumSamples[XCL_PERF_MON_MEMORY] = 0;

      auto profileMgr = getProfileManager();

      // Start device trace if enabled
      xdp::RTUtil::e_device_trace deviceTrace = profileMgr->getTransferTrace();
      xdp::RTUtil::e_stall_trace stallTrace = profileMgr->getStallTrace();
      uint32_t traceOption = (deviceTrace == xdp::RTUtil::DEVICE_TRACE_COARSE) ? 0x1 : 0x0;
      if (deviceTrace != xdp::RTUtil::DEVICE_TRACE_OFF) traceOption   |= (0x1 << 1);
      if (stallTrace & xdp::RTUtil::STALL_TRACE_INT)    traceOption   |= (0x1 << 2);
      if (stallTrace & xdp::RTUtil::STALL_TRACE_STR)    traceOption   |= (0x1 << 3);
      if (stallTrace & xdp::RTUtil::STALL_TRACE_EXT)    traceOption   |= (0x1 << 4);
      XOCL_DEBUGF("Starting trace with option = 0x%x\n", traceOption);

      if(dInt) {
        // Configure monitor IP and FIFO if present
        dInt->startTrace(XCL_PERF_MON_MEMORY, traceOption);
        // Configure DMA if present
        if (dInt->hasTs2mm()) {
          info->ts2mm_en = allocateDeviceDDRBufferForTrace(dInt, xdevice);
          /* Todo: Write user specified memory bank here */
          trace_memory = "TS2MM";
        }
      } else {
        xdevice->startTrace(XCL_PERF_MON_MEMORY, traceOption);
        // for HW_EMU consider , 2 calls , with new XDP, all flow should be same
      }

      // Get/set clock freqs
      double deviceClockMHz = xdevice->getDeviceClock().get();
      if (deviceClockMHz > 0) {
        setKernelClockFreqMHz(device->get_unique_name(), deviceClockMHz );
        profileMgr->setDeviceClockFreqMHz( deviceClockMHz );
      }

      // Get the trace samples threshold
      info->mSamplesThreshold = profileMgr->getTraceSamplesThreshold();

      // Calculate interval for clock training
      info->mTrainingIntervalUsec = (uint32_t)(pow(2, 17) / deviceClockMHz);
      profileMgr->setLoggingTrace(XCL_PERF_MON_MEMORY, false);
    }

    if(Plugin->getFlowMode() == xdp::RTUtil::DEVICE)
      Plugin->setTraceMemory(trace_memory);
  }

  void OCLProfiler::endTrace()
  {
    auto platform = getclPlatformID();

    for (auto device : platform->get_device_range()) {
      if(!device->is_active()) {
        continue;
      }
      auto itr = DeviceData.find(device);
      if (itr==DeviceData.end()) {
        return;
      }
      auto xdevice = device->get_xrt_device();
      xdp::xoclp::platform::device::data* info = &(itr->second);
      if (info->ts2mm_en) {
        auto dInt  = &(info->mDeviceIntf);
        clearDeviceDDRBufferForTrace(dInt, xdevice);
        info->ts2mm_en = false;
      }
    }
  }

  // Get device counters
  void OCLProfiler::getDeviceCounters(bool firstReadAfterProgram, bool forceReadCounters)
  {
    if (!isProfileRunning() || !deviceCountersProfilingOn())
      return;

    XOCL_DEBUGF("getDeviceCounters: START (firstRead: %d, forceRead: %d)\n",
                 firstReadAfterProgram, forceReadCounters);

    logDeviceCounters(firstReadAfterProgram, forceReadCounters,
          false /* In HW flow, all monitor counters are logged anyway; only matters in HW EMU */,
          XCL_PERF_MON_MEMORY);

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
    if(deviceTraceProfilingOn()) {
      logTrace(XCL_PERF_MON_MEMORY /* in new flow, type should not matter in HW or even HW Emu */, forceReadTrace, true);

    // With new XDP flow, HW Emu should be similar to Device flow. So, multiple calls to trace/counters should not be needed.
    // But needed for older flow
      if ((Plugin->getFlowMode() == xdp::RTUtil::HW_EM) && Plugin->getSystemDPAEmulation() == false) {
        xoclp::platform::log_device_trace(platform, XCL_PERF_MON_ACCEL, forceReadTrace);
        xoclp::platform::log_device_trace(platform, XCL_PERF_MON_STR, forceReadTrace);
      }
    }

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

    ProfileMgr->setProfileStartTime(std::chrono::steady_clock::now());

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
    std::string profileFile("profile_summary");
    ProfileMgr->turnOnFile(xdp::RTUtil::FILE_SUMMARY);
    xdp::CSVProfileWriter* csvProfileWriter = new xdp::CSVProfileWriter(
      Plugin.get(), "Xilinx", profileFile);
    ProfileWriters.push_back(csvProfileWriter);
    ProfileMgr->attach(csvProfileWriter);
    // Add JSON writer as well
    xdp::JSONProfileWriter* jsonWriter = new xdp::JSONProfileWriter(
      Plugin.get(), "Xilinx", profileFile);
    ProfileWriters.push_back(jsonWriter);
    ProfileMgr->attach(jsonWriter);
    ProfileMgr->getRunSummary()->setProfileTree(jsonWriter->getProfileTree());

    // Enable Trace File if profile is on and trace is enabled
    std::string timelineFile("");
    if (xrt::config::get_timeline_trace()) {
      timelineFile = "timeline_trace";
      ProfileMgr->turnOnFile(xdp::RTUtil::FILE_TIMELINE_TRACE);
    }
    xdp::CSVTraceWriter* csvTraceWriter = new xdp::CSVTraceWriter(timelineFile, "Xilinx", Plugin.get());
    TraceWriters.push_back(csvTraceWriter);
    ProfileMgr->attach(csvTraceWriter);

#if 0
    // Not Used
    if (std::getenv("SDX_NEW_PROFILE")) {
      std::string profileFile2("sdx_profile_summary");
      std::string timelineFile2("sdx_timeline_trace");
      xdp::UnifiedCSVProfileWriter* csvProfileWriter2 = new xdp::UnifiedCSVProfileWriter(profileFile2, "Xilinx", Plugin.get());
      ProfileWriters.push_back(csvProfileWriter2);
      ProfileMgr->attach(csvProfileWriter2);
    }
#endif

    // Add functions to callback for profiling kernel/CU scheduling
    xocl::add_command_start_callback(xoclp::get_cu_start);
    xocl::add_command_done_callback(xoclp::get_cu_done);
  }

  // Wrap up profiling by writing files
  void OCLProfiler::endProfiling()
  {
    ProfileMgr->setProfileEndTime(std::chrono::steady_clock::now());

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
    unsigned int numStallSlots = 0;
    unsigned int numStreamSlots = 0;
    unsigned int numShellSlots = 0;
    if (applicationProfilingOn() && ProfileMgr->isDeviceProfileOn()) {
      if (Plugin->getFlowMode() == RTUtil::DEVICE || (Plugin->getFlowMode() == xdp::RTUtil::HW_EM && Plugin->getSystemDPAEmulation())) {
        for (auto device : Platform->get_device_range()) {
          auto itr = DeviceData.find(device);
          if (itr==DeviceData.end()) {
            itr = DeviceData.emplace(device,xdp::xoclp::platform::device::data()).first;
          }
          DeviceIntf* dInt = &(itr->second.mDeviceIntf);
          // Assumption : debug_ip_layout has been read
  
          numStallSlots  += dInt->getNumMonitors(XCL_PERF_MON_STALL);
          numStreamSlots += dInt->getNumMonitors(XCL_PERF_MON_STR);
          numShellSlots  += dInt->getNumMonitors(XCL_PERF_MON_SHELL);
        }
      } else {
        for (auto device : Platform->get_device_range()) {
          std::string deviceName = device->get_unique_name();
          numStallSlots  += xoclp::platform::get_profile_num_slots(getclPlatformID(),
                                                                   deviceName,
                                                                   XCL_PERF_MON_STALL);
          numStreamSlots += xoclp::platform::get_profile_num_slots(getclPlatformID(),
                                                                   deviceName,
                                                                   XCL_PERF_MON_STR);
          numShellSlots  += xoclp::platform::get_profile_num_slots(getclPlatformID(),
                                                                   deviceName,
                                                                   XCL_PERF_MON_SHELL);
        }
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
        if (Plugin->getFlowMode() == RTUtil::DEVICE && numShellSlots > 0) {
          w->enableShellTables();
        }
      }
    }
  }

  void OCLProfiler::logDeviceCounters(bool firstReadAfterProgram, bool forceReadCounters, bool logAllMonitors, xclPerfMonType type)
  {
    // check valid perfmon type
    if(!logAllMonitors && !( (deviceCountersProfilingOn() && (type == XCL_PERF_MON_MEMORY || type == XCL_PERF_MON_STR))
        || ((Plugin->getFlowMode() == xdp::RTUtil::HW_EM) && type == XCL_PERF_MON_ACCEL) )) {
      return;
    }

    auto platform = getclPlatformID();
    for (auto device : platform->get_device_range()) {
      if(!device->is_active()) {
        continue;
      }
      auto xdevice = device->get_xrt_device();

      auto itr = DeviceData.find(device);
      if (itr==DeviceData.end()) {
        itr = DeviceData.emplace(device,xdp::xoclp::platform::device::data()).first;
      }
      xdp::xoclp::platform::device::data* info = &(itr->second);
      DeviceIntf* dInt = nullptr;
      if ((Plugin->getFlowMode() == xdp::RTUtil::DEVICE) || (Plugin->getFlowMode() == xdp::RTUtil::HW_EM && Plugin->getSystemDPAEmulation())) {
        dInt = &(itr->second.mDeviceIntf);
        dInt->setDevice(new xdp::XrtDevice(xdevice));
      }

      std::chrono::steady_clock::time_point nowTime = std::chrono::steady_clock::now();
      if (forceReadCounters || 
              ((nowTime - info->mLastCountersSampleTime) > std::chrono::milliseconds(info->mSampleIntervalMsec)))
      {
        if(dInt) {
          dInt->readCounters(XCL_PERF_MON_MEMORY, info->mCounterResults);
        } else {
          xdevice->readCounters(XCL_PERF_MON_MEMORY, info->mCounterResults);
        }

        // Record the counter data 
        struct timespec now;
        int err = clock_gettime(CLOCK_MONOTONIC, &now);
        uint64_t timeNsec = (err < 0) ? 0 : (uint64_t) now.tv_sec * 1000000000UL + (uint64_t) now.tv_nsec;

        // Create unique name for device since currently all devices are called fpga0
        std::string device_name = device->get_unique_name();
        std::string binary_name = device->get_xclbin().project_name();
        uint32_t program_id = (device->get_program()) ? (device->get_program()->get_uid()) : 0;
        getProfileManager()->logDeviceCounters(device_name, binary_name, program_id,
                                     XCL_PERF_MON_MEMORY /*For HW flow all types handled together */,
                                     info->mCounterResults, timeNsec, firstReadAfterProgram);
 
        //update the last time sample
        info->mLastCountersSampleTime = nowTime;
      }
    // With new XDP flow, HW Emu should be similar to Device flow. So, multiple calls to trace/counters should not be needed.
    // But needed for older flow
      if(Plugin->getFlowMode() == xdp::RTUtil::HW_EM && Plugin->getSystemDPAEmulation() == false) {
          xoclp::platform::device::logCounters(device, XCL_PERF_MON_ACCEL, firstReadAfterProgram, forceReadCounters);
          xoclp::platform::device::logCounters(device, XCL_PERF_MON_STR, firstReadAfterProgram, forceReadCounters);
      }
    }   // for all devices
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
      if(Plugin->getFlowMode() == xdp::RTUtil::DEVICE || (Plugin->getFlowMode() == xdp::RTUtil::HW_EM && Plugin->getSystemDPAEmulation())) {
        ret = (int)logTrace(type, true /* forceRead*/, true /* logAllMonitors */);
      } else {
        ret = xoclp::platform::log_device_trace(getclPlatformID(),type, true);
      }
      if (ret == -1)
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_msec));
      iter++;
    }
    XDP_LOG("Trace logged for type %d after %d iterations\n", type, iter);
  }

  int OCLProfiler::logTrace(xclPerfMonType type, bool forceRead, bool logAllMonitors)
  {
    auto profileMgr = getProfileManager();
    if(profileMgr->getLoggingTrace(type)) {
        return -1;
    }

    // check valid perfmon type
    if(!logAllMonitors && !( (deviceTraceProfilingOn() && (type == XCL_PERF_MON_MEMORY || type == XCL_PERF_MON_STR))
        || ((Plugin->getFlowMode() == xdp::RTUtil::HW_EM) && type == XCL_PERF_MON_ACCEL) )) {
      return -1;
    }

    auto platform = getclPlatformID();
    profileMgr->setLoggingTrace(type, true);
    for (auto device : platform->get_device_range()) {
      if(!device->is_active()) {
        continue;
      }
      auto xdevice = device->get_xrt_device();

      auto itr = DeviceData.find(device);
      if (itr==DeviceData.end()) {
        itr = DeviceData.emplace(device,xdp::xoclp::platform::device::data()).first;
      }
      xdp::xoclp::platform::device::data* info = &(itr->second);
      DeviceIntf* dInt = nullptr;
      if ((Plugin->getFlowMode() == xdp::RTUtil::DEVICE) || (Plugin->getFlowMode() == xdp::RTUtil::HW_EM && Plugin->getSystemDPAEmulation())) {
        dInt = &(itr->second.mDeviceIntf);
        dInt->setDevice(new xdp::XrtDevice(xdevice));
      }

      // Do clock training if enough time has passed
      // NOTE: once we start flushing FIFOs, we stop all training (no longer needed)
      std::chrono::steady_clock::time_point nowTime = std::chrono::steady_clock::now();

      if (!info->mPerformingFlush &&
            (nowTime - info->mLastTraceTrainingTime[type]) > std::chrono::microseconds(info->mTrainingIntervalUsec)) {
        // Empty method // xdevice->clockTraining(type);
        info->mLastTraceTrainingTime[type] = nowTime;
      }

      // Read and log when trace FIFOs are filled beyond specified threshold
      uint32_t numSamples = 0;
      if (!forceRead) {
        numSamples = (dInt) ? dInt->getTraceCount(type) : xdevice->countTrace(type).get();
      }

      // Control how often we do clock training: if there are new samples, then don't train
      if (numSamples > info->mLastTraceNumSamples[type]) {
        info->mLastTraceTrainingTime[type] = nowTime;
      }
      info->mLastTraceNumSamples[type] = numSamples;

      if (forceRead || (numSamples > info->mSamplesThreshold)) {
        // Create unique name for device since system can have multiples of same device
        std::string device_name = device->get_unique_name();
        std::string binary_name = "binary";
        if (device->is_active())
          binary_name = device->get_xclbin().project_name();

        if (dInt) {    // HW Device flow
          if (dInt->hasFIFO()) {
              dInt->readTrace(type, info->mTraceVector);
              if (info->mTraceVector.mLength) {
                profileMgr->logDeviceTrace(device_name, binary_name, type, info->mTraceVector);
                // detect if FIFO is full
                auto fifoProperty = Plugin->getProfileSlotProperties(XCL_PERF_MON_FIFO, device_name, 0);
                auto fifoSize = RTUtil::getDevTraceBufferSize(fifoProperty);
                if (info->mTraceVector.mLength >= fifoSize)
                  Plugin->sendMessage(FIFO_WARN_MSG);
              }
              info->mTraceVector.mLength= 0;
          } else if (dInt->hasTs2mm()) {
            configureDDRTraceReader(dInt->getWordCountTs2mm());
            bool endLog = false;
            while (!endLog) {
              endLog = !(readTraceDataFromDDR(dInt, xdevice, info->mTraceVector));
              profileMgr->logDeviceTrace(device_name, binary_name, type, info->mTraceVector, endLog);
              info->mTraceVector = {};
            }
          }
        } else {
          while(1) {
            xdevice->readTrace(type, info->mTraceVector);
            if(!info->mTraceVector.mLength)
              break;

            // log the device trace
            profileMgr->logDeviceTrace(device_name, binary_name, type, info->mTraceVector);
            info->mTraceVector.mLength= 0;

// With new emulation support, is this required ?
            // Only check repeatedly for trace buffer flush if HW emulation
            if(Plugin->getFlowMode() != xdp::RTUtil::HW_EM)
              break;
          }  // for HW Emu continue the loop

        }
      }   // forceRead || (numSamples > info->mSamplesThreshold)
      if(forceRead)
        info->mPerformingFlush = true;

    } // for all devices
    profileMgr->setLoggingTrace(type, false);
    return 0;
  }


  bool OCLProfiler::allocateDeviceDDRBufferForTrace(DeviceIntf* dInt, xrt::device* xrtDevice)
  {
    /* If buffer is already allocated and still attempting to initialize again, 
     * then reset the TS2MM IP and free the old buffer
     */
    if(mDDRBufferForTrace) {
      clearDeviceDDRBufferForTrace(dInt, xrtDevice);
    }

    try {
      mDDRBufferSz = xdp::xoclp::platform::get_ts2mm_buf_size();
      mDDRBufferForTrace = xrtDevice->alloc(mDDRBufferSz, xrt::hal::device::Domain::XRT_DEVICE_RAM, dInt->getTS2MmMemIndex(), nullptr);
      xrtDevice->sync(mDDRBufferForTrace, mDDRBufferSz, 0, xrt::hal::device::direction::HOST2DEVICE, false);
    } catch (const std::exception& ex) {
      std::cerr << ex.what() << std::endl;
      return false;
    }
    // Data Mover will write input stream to this address
    uint64_t bufAddr = xrtDevice->getDeviceAddr(mDDRBufferForTrace);

    dInt->initTS2MM(mDDRBufferSz, bufAddr);
    return true;
  }


  // Reset DDR Trace : reset TS2MM IP and clear buffer on Device DDR
  void OCLProfiler::clearDeviceDDRBufferForTrace(DeviceIntf* dInt, xrt::device* xrtDevice)
  {
    if(!mDDRBufferForTrace)
      return;

    dInt->resetTS2MM();

    auto addr = xrtDevice->map(mDDRBufferForTrace);
    munmap(addr, mDDRBufferSz);
    xrtDevice->free(mDDRBufferForTrace);

    mDDRBufferForTrace = nullptr;
    mDDRBufferSz = 0;
  }

  void OCLProfiler::configureDDRTraceReader(uint64_t wordCount)
  {
    mTraceReadBufSz = wordCount * TRACE_PACKET_SIZE;
    mTraceReadBufSz = (mTraceReadBufSz > TS2MM_MAX_BUF_SIZE) ? TS2MM_MAX_BUF_SIZE : mTraceReadBufSz;

    mTraceReadBufOffset = 0;
    mTraceReadBufChunkSz = MAX_TRACE_NUMBER_SAMPLES * TRACE_PACKET_SIZE;
  }

  /** 
   * Takes the offset inside the mapped buffer
   * and syncs it with device and returns its virtual address.
   * We can read the entire buffer in one go if we want to
   * or choose to read in chunks
   */
  void* OCLProfiler::syncDeviceDDRToHostForTrace(xrt::device* xrtDevice, uint64_t offset, uint64_t bytes)
  {
    if(!mDDRBufferSz || !mDDRBufferForTrace)
      return nullptr;

    auto addr = xrtDevice->map(mDDRBufferForTrace);
    xrtDevice->sync(mDDRBufferForTrace, bytes, offset, xrt::hal::device::direction::DEVICE2HOST, false);

    return static_cast<char*>(addr) + offset;
  }

  void OCLProfiler::readTraceDataFromDDR(DeviceIntf* dIntf, xrt::device* xrtDevice, xclTraceResultsVector& traceVector, uint64_t offset, uint64_t bytes)
  {
    void* hostBuf = syncDeviceDDRToHostForTrace(xrtDevice, offset, bytes);
    if(hostBuf) {
      dIntf->parseTraceData(hostBuf, bytes, traceVector);
    }
  }

  /**
   * This reader needs to be initialized once and then
   * returns data as long as it's available
   * returns true if data equal to chunksize was read
   */
  bool OCLProfiler::readTraceDataFromDDR(DeviceIntf* dIntf, xrt::device* xrtDevice, xclTraceResultsVector& traceVector)
  {
    if(mTraceReadBufOffset >= mTraceReadBufSz)
      return false;

    uint64_t nBytes = mTraceReadBufChunkSz;
    if((mTraceReadBufOffset + mTraceReadBufChunkSz) > mTraceReadBufSz)
      nBytes = mTraceReadBufSz - mTraceReadBufOffset;

    void* hostBuf = syncDeviceDDRToHostForTrace(xrtDevice, mTraceReadBufOffset, nBytes);
    if(hostBuf) {
      dIntf->parseTraceData(hostBuf, nBytes, traceVector);
      mTraceReadBufOffset += nBytes;
    }
    return (nBytes == (mTraceReadBufChunkSz && hostBuf));
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

  void OCLProfiler::reset()
  {
    // resetDeviceProfilingFlag();
    DeviceData.clear();  
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
