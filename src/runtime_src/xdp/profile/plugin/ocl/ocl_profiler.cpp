/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#define XDP_SOURCE

#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <chrono>

#include "ocl_profiler.h"
#include "xdp/profile/profile_config.h"
#include "xdp/profile/core/rt_profile.h"
#include "xdp/profile/device/xrt_device/xdp_xrt_device.h"
#include "xdp/profile/device/ocl_device_logger/profile_mngr_trace_logger.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/writer/json_profile.h"
#include "xdp/profile/writer/csv_profile.h"
#include "xrt/util/config_reader.h"
#include "xrt/util/message.h"
#include "xclperf.h"


#ifdef _WIN32
#pragma warning (disable : 4996 4702)
/* 4996 : Disable warning during Windows compilation for use of std::getenv */
/* 4702 : Disable warning for unreachable code. This is a temporary workaround for a crash on Windows */
#endif


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
    ProfileMgr = std::make_unique<RTProfile>(ProfileFlags, Plugin);
    startProfiling();
  }

  OCLProfiler::~OCLProfiler()
  {
    // Inform downstream guidance if objects were properly released
    Plugin->setObjectsReleased(mEndDeviceProfilingCalled);

    // End all profiling, including device
    if (!mEndDeviceProfilingCalled && applicationProfilingOn()) {
      xrt_xocl::message::send(xrt_xocl::message::severity_level::warning,
          "Profiling may contain incomplete information. Please ensure all OpenCL objects are released by your host code (e.g., clReleaseProgram()).");

      // Before deleting, do a final read of counters and force flush of trace buffers
      endDeviceProfiling();
    }
    Plugin->setApplicationEnd();
    endProfiling();
    reset();
    pDead = true;
  }

  oclDeviceData* OCLProfiler::initializeDeviceInterface(xocl::device* device)
  {
    DeviceIntf* dInt = nullptr;
    auto xdevice = device->get_xrt_device();

    auto itr = DeviceData.find(device);
    if (itr!=DeviceData.end())
      return itr->second;

    itr = DeviceData.emplace(device,(new oclDeviceData())).first;
    auto info = itr->second;
    if ((Plugin->getFlowMode() == xdp::RTUtil::DEVICE) ||
        (Plugin->getFlowMode() == xdp::RTUtil::HW_EM && Plugin->getSystemDPAEmulation())) {
      dInt = &(info->mDeviceIntf);
      dInt->setDevice(new xdp::XrtDevice(xdevice));
      dInt->readDebugIPlayout();
      dInt->setMaxBwRead();
      dInt->setMaxBwWrite();

      // Record number of monitors and how many have trace enabled
      auto deviceName = device->get_unique_name();
      xclPerfMonType monType [] = {XCL_PERF_MON_ACCEL, XCL_PERF_MON_MEMORY, XCL_PERF_MON_STR};
      std::string    monName [] = {"XCL_PERF_MON_ACCEL", "XCL_PERF_MON_MEMORY", "XCL_PERF_MON_STR"};
      for (uint32_t m=0; m < 3; ++m) {
        uint32_t numMonitors = dInt->getNumMonitors(monType[m]);
        uint32_t numTrace = 0;
        for (uint32_t n=0; n < numMonitors; ++n) {
          if (dInt->getMonitorProperties(monType[m], n) & XCL_PERF_MON_TRACE_MASK)
            numTrace++;
        }

        std::string key = deviceName + "|" + monName[m] + "|" + std::to_string(numTrace);
        Plugin->addNumMonitorMap(key, numMonitors);
      }
    }
    return info;
  }

  // Start device profiling
  void OCLProfiler::startDeviceProfiling(size_t numComputeUnits)
  {
    auto platform = getclPlatformID();

    // xdp always needs some device data
    // regardless of whether device profiling
    // was turned
    for (auto device: platform->get_device_range()) {
      if (device->is_active())
        initializeDeviceInterface(device);
    }

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
  uint64_t
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
      auto info = initializeDeviceInterface(device);
      auto xdevice = device->get_xrt_device();
      DeviceIntf* dInt = &info->mDeviceIntf;

      // Set clock etc.
      double deviceClockMHz = xdevice->getDeviceClock().get();
      if(deviceClockMHz > 0) {
          getProfileManager()->setDeviceClockFreqMHz(deviceClockMHz);
      }
      info->mPerformingFlush = false;

      // Reset and Start counters
      if(dInt) {
        dInt->startCounters();
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
           * into DeviceIntf as it uses xrt_xocl::device
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

    /*
     * Currently continuous offload only works on :
     * 1. one active device
     * 2. hardware flow
     */
    if (mTraceThreadEn) {
       unsigned int numActiveDevices = 0;
       for (auto device : platform->get_device_range()) {
         if(device->is_active())
           ++numActiveDevices;
       }
       if (numActiveDevices > 1) {
         xrt_xocl::message::send(xrt_xocl::message::severity_level::warning, CONTINUOUS_OFFLOAD_WARN_MSG_DEVICE);
         mTraceThreadEn = false;
       }
       if (Plugin->getFlowMode() != xdp::RTUtil::DEVICE) {
         xrt_xocl::message::send(xrt_xocl::message::severity_level::warning, CONTINUOUS_OFFLOAD_WARN_MSG_FLOW);
         mTraceThreadEn = false;
       }
    }

    for (auto device : platform->get_device_range()) {
      if(!device->is_active()) {
        continue;
      }
      auto info = initializeDeviceInterface(device);
      auto xdevice = device->get_xrt_device();
      DeviceIntf* dInt = &info->mDeviceIntf;

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

      if (dInt) {
        // Do for both PL and AIE trace (if available)
        //for (int i=0; i < 2; i++) { 
          //bool isAIETrace = (i == 1);	// FOR now
          //bool isAIETrace = false;

          // Configure monitor IP and FIFO if present
          dInt->startTrace(traceOption);
          std::string  binaryName = device->get_xclbin().project_name();
          uint64_t traceBufSz = 0;
          if (dInt->hasTs2mm()) {
            traceBufSz = getDeviceDDRBufferSize(dInt, device);
            trace_memory = "TS2MM";
          }
          
          // Continuous trace isn't safe to use with stall setting
          if (dInt->hasFIFO() && mTraceThreadEn && stallTrace!= xdp::RTUtil::STALL_TRACE_OFF) {
            xrt_xocl::message::send(xrt_xocl::message::severity_level::warning, CONTINUOUS_OFFLOAD_WARN_MSG_STALLS);
          }

          DeviceTraceLogger* deviceTraceLogger = new TraceLoggerUsingProfileMngr(getProfileManager(), device->get_unique_name(), binaryName);
          auto offloader = std::make_unique<DeviceTraceOffload>(dInt, deviceTraceLogger,
                                                         mTraceReadIntMs, traceBufSz, false);
        bool init_done = offloader->read_trace_init(mTraceThreadEn);
        if (init_done) {
          offloader->train_clock();
          /* Trace FIFO is usually very small (8k,16k etc)
           *  Hence enable Continuous clock training by default
           *  ONLY for Trace Offload to DDR Memory
           */
          if (mTraceThreadEn)
            offloader->start_offload(OffloadThreadType::TRACE);
          else if (dInt->hasTs2mm())
            offloader->start_offload(OffloadThreadType::CLOCK_TRAIN);

          /* If unable to use circular buffer then throw warning
           */
          if (dInt->hasTs2mm() && mTraceThreadEn) {
            auto tdma = dInt->getTs2mm();
            if (tdma->supportsCircBuf()) {
              uint64_t min_offload_rate = 0;
              uint64_t requested_offload_rate = 0;
              bool using_circ_buf = offloader->using_circular_buffer(min_offload_rate, requested_offload_rate);
              if (!using_circ_buf) {
                std::string msg = std::string(TS2MM_WARN_MSG_CIRC_BUF)
                                + " Minimum required offload rate (bytes per second) : " + std::to_string(min_offload_rate)
                                + " Requested offload rate : " + std::to_string(requested_offload_rate);
                xrt_xocl::message::send(xrt_xocl::message::severity_level::warning, msg);
              }
            }
          }

          DeviceTraceLoggers.push_back(deviceTraceLogger);
          DeviceTraceOffloadList.push_back(std::move(offloader));
        } else {
          delete deviceTraceLogger;
          if (dInt->hasTs2mm()) {
            xrt_xocl::message::send(xrt_xocl::message::severity_level::warning, TS2MM_WARN_MSG_ALLOC_FAIL);
          }
        }
      } else {
        xdevice->startTrace(XCL_PERF_MON_MEMORY, traceOption);
        // for HW_EMU consider , 2 calls , with new XDP, all flow should be same
      }

      // Get/set clock freqs
      double deviceClockMHz = xdevice->getDeviceClock().get();
      if (deviceClockMHz > 0) {
        setKernelClockFreqMHz(device->get_unique_name(), static_cast<unsigned int>(deviceClockMHz) );
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
    auto& g_map = Plugin->getDeviceTraceBufferFullMap();
    for (auto& trace_offloader : DeviceTraceOffloadList) {
      TraceLoggerUsingProfileMngr* deviceTraceLogger =
          dynamic_cast<TraceLoggerUsingProfileMngr*>(trace_offloader->getDeviceTraceLogger());

      if (trace_offloader->trace_buffer_full()) {
        // Only show FIFO full messages for device runs
        if (Plugin->getFlowMode() == xdp::RTUtil::DEVICE) {
          if (trace_offloader->has_fifo())
            Plugin->sendMessage(FIFO_WARN_MSG);
          else
            Plugin->sendMessage(TS2MM_WARN_MSG_BUF_FULL);
        }

        if (deviceTraceLogger) {
          g_map[deviceTraceLogger->getDeviceName()] = 1;
        }
      }
      else if (deviceTraceLogger) {
        g_map[deviceTraceLogger->getDeviceName()] = 0;
      }
      trace_offloader.reset();
    }
    DeviceTraceOffloadList.clear();
    for (auto itr : DeviceTraceLoggers) {
      delete itr;
    }
    DeviceTraceLoggers.clear();
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
    if (xrt_xocl::config::get_profile() == false)
      return;

    ProfileMgr->setProfileStartTime(std::chrono::steady_clock::now());

    // Turn on device profiling (as requested)
    std::string data_transfer_trace = xrt_xocl::config::get_data_transfer_trace();
    std::string stall_trace = xrt_xocl::config::get_stall_trace();

    // Turn on application profiling
    turnOnProfile(xdp::RTUtil::PROFILE_APPLICATION);

    turnOnProfile(xdp::RTUtil::PROFILE_DEVICE_COUNTERS);

    char* emuMode = std::getenv("XCL_EMULATION_MODE");
    if((!emuMode /* Device Flow */
        || ((0 == strcmp(emuMode, "hw_emu")) && xrt_xocl::config::get_system_dpa_emulation()) /* HW Emu with System DPA, same as Device Flow */
        || (data_transfer_trace.find("off") == std::string::npos)) && xrt_xocl::config::get_timeline_trace()  ) {
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
    if (xrt_xocl::config::get_timeline_trace()) {
      timelineFile = "timeline_trace";
      ProfileMgr->turnOnFile(xdp::RTUtil::FILE_TIMELINE_TRACE);
      mTraceThreadEn = xrt_xocl::config::get_continuous_trace();
      if (mTraceThreadEn) {
        mTraceReadIntMs = xrt_xocl::config::get_continuous_trace_interval_ms();
      } else {
        // Faster clock training causes problems with long designs
        // 500ms is good enough for continous clock training
        mTraceReadIntMs = 500;
      }
    }
    xdp::CSVTraceWriter* csvTraceWriter = new xdp::CSVTraceWriter(timelineFile, "Xilinx", Plugin.get());
    TraceWriters.push_back(csvTraceWriter);
    ProfileMgr->attach(csvTraceWriter);

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
            itr = DeviceData.emplace(device,(new oclDeviceData())).first;
          }
          DeviceIntf* dInt = &(itr->second->mDeviceIntf);
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
        itr = DeviceData.emplace(device,(new oclDeviceData())).first;
      }
      oclDeviceData* info = itr->second;
      DeviceIntf* dInt = nullptr;
      if ((Plugin->getFlowMode() == xdp::RTUtil::DEVICE) || (Plugin->getFlowMode() == xdp::RTUtil::HW_EM && Plugin->getSystemDPAEmulation())) {
        dInt = &(itr->second->mDeviceIntf);
        dInt->setDevice(new xdp::XrtDevice(xdevice));
      }

      std::chrono::steady_clock::time_point nowTime = std::chrono::steady_clock::now();
      if (forceReadCounters ||
              ((nowTime - info->mLastCountersSampleTime) > std::chrono::milliseconds(info->mSampleIntervalMsec)))
      {
        if(dInt) {
          dInt->readCounters(info->mCounterResults);
        } else {
          xdevice->readCounters(XCL_PERF_MON_MEMORY, info->mCounterResults);
        }

        // Record the counter data
        auto timeSinceEpoch = (std::chrono::steady_clock::now()).time_since_epoch();
        auto value = std::chrono::duration_cast<std::chrono::nanoseconds>(timeSinceEpoch);
        uint64_t timeNsec = value.count();

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
    // Dedicated thread takes care of all the logging
    if (mTraceThreadEn)
      return -1;

    auto profileMgr = getProfileManager();
    if(profileMgr->getLoggingTrace(type)) {
        return -1;
    }

    // check valid perfmon type
    if(!logAllMonitors && !( (deviceTraceProfilingOn() && (type == XCL_PERF_MON_MEMORY || type == XCL_PERF_MON_STR))
        || ((Plugin->getFlowMode() == xdp::RTUtil::HW_EM) && type == XCL_PERF_MON_ACCEL) )) {
      return -1;
    }

    profileMgr->setLoggingTrace(type, true);

    for (auto& trace_offloader: DeviceTraceOffloadList) {
      trace_offloader->read_trace();
      trace_offloader->read_trace_end();
    }

    return 0;
  }

  uint64_t OCLProfiler::getDeviceDDRBufferSize(DeviceIntf* dInt, xocl::device* device)
  {
    uint64_t sz = 0;
    sz = GetTS2MMBufSize();
    auto memorySz = xdp::xoclp::platform::device::getMemSizeBytes(device, dInt->getTS2MmMemIndex());
    if (memorySz > 0 && sz > memorySz) {
      sz = memorySz;
      std::string msg = "Trace Buffer size is too big for Memory Resource. Using " + std::to_string(memorySz)
                        + " Bytes instead.";
      xrt_xocl::message::send(xrt_xocl::message::severity_level::warning, msg);
    }
    return sz;
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
    for(auto itr : DeviceData) {
      delete itr.second;
    }
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
    OCLProfiler::Instance()->getDeviceCounters(firstReadAfterProgram, forceReadCounters);
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
