/*
 * Copyright (C) 2018, Xilinx Inc - All rights reserved
 * Xilinx SDAccel Media Accelerator API
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

#include "xma_profile.h"
#include "xdp/rt_singleton.h"

// TODO: reduce these includes; do we really need them all?
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <set>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <vector>
#include <queue>
#include <thread>
#include <atomic>
#include <boost/format.hpp>

// TODO: Once the new XDP is ready, let's hook these in. For now, turn them off.
#if 0
// *****************************************************************************
//                        Top-Level Profile Functions
// *****************************************************************************

void
profile_initialize(xclDeviceHandle s_handle, int use_profile, int use_trace,
                   const char* data_transfer_trace, const char* stall_trace)
{
  printf("profile_initialize: s_handle=%p, use_profile=%d, use_trace=%d, data_transfer_trace=%s, stall_trace=%s\n",
         s_handle, use_profile, use_trace, data_transfer_trace, stall_trace);

  // Initialize runtime singleton
  auto rts = xdp::RTSingleton::Instance();

  XmaPlugin* xmaPlugin = new XmaPlugin();
  rts->attachPlugin(xmaPlugin);

  int ProfileFlags = xdp::RTProfile::PROFILE_APPLICATION;

  // Evaluate arguments
  xdp::mUseProfile = use_profile;
  xdp::mUseTrace = use_trace;
  xdp::mTraceOption = 0;

  if (xdp::mUseTrace) {
    xdp::mDataTransferTrace = data_transfer_trace;
    xdp::mStallTrace = stall_trace;
    xdp::mTraceOption = (xdp::mDataTransferTrace == "coarse") ? 0x1 : 0x0;
    if (xdp::mStallTrace == "dataflow")    xdp::mTraceOption |= (0x1 << 2);

    else if (xdp::mStallTrace == "pipe")   xdp::mTraceOption |= (0x1 << 3);
    else if (xdp::mStallTrace == "memory") xdp::mTraceOption |= (0x1 << 4);
    else if (xdp::mStallTrace == "all")    xdp::mTraceOption |= (0x7 << 2);
    else printf("The stall_trace setting of %s is not recognized. Please use memory|dataflow|pipe|all|off.", xdp::mStallTrace.c_str());
  }

  // Get design info (clock freqs, device/binary names)
  xclDeviceInfo2 deviceInfo;
  xclGetDeviceInfo2(s_handle, &deviceInfo);
  xdp::mKernelClockFreq = deviceInfo.mOCLFrequency[0];
  xdp::mDeviceName = deviceInfo.mName;
  // TODO: do we know this?
  xdp::mBinaryName = "binary";
  xdp::mProfileMgr->setKernelClockFreqMHz(xdp::mDeviceName, xdp::mKernelClockFreq);

  //
  // Profile Summary
  //
  if (xdp::mUseProfile) {
	xdp::mProfileMgr->turnOnProfile(xdp::RTProfile::PROFILE_DEVICE_COUNTERS);
    xdp::mProfileMgr->turnOnFile(xdp::RTProfile::FILE_SUMMARY);

    //std::string timelineFile = "xma_timeline_trace.csv";
    //xdp::mWriter = new xdp::CSVWriter(profileFile, timelineFile, "Xilinx");
    xdp::mWriter = new xdp::CSVWriter("sdaccel_profile_summary", "sdaccel_timeline_trace", "Xilinx");
    xdp::mProfileMgr->attach(xdp::mWriter);
  }

  //
  // Timeline Trace
  //
  if (xdp::mUseTrace) {
	xdp::mProfileMgr->turnOnProfile(xdp::RTProfile::PROFILE_DEVICE_TRACE);
    xdp::mProfileMgr->turnOnFile(xdp::RTProfile::FILE_TIMELINE_TRACE);

    // Open timeline trace file
    //xdp::mTraceStream.open("xma_timeline_trace.csv");

    // Make an initialization call for time
    xdp::mProfileMgr->time_ns();

    // Write header
    //xdp::writeTraceHeader(xdp::mTraceStream);
  }
}

void
profile_start(xclDeviceHandle s_handle)
{
  printf("profile_start: s_handle=%p\n", s_handle);

  //
  // Profile Summary
  //
  if (xdp::mUseProfile) {
    // Start counters
    xclPerfMonStartCounters(s_handle, XCL_PERF_MON_MEMORY);
  }

  //
  // Timeline Trace
  //
  if (xdp::mUseTrace) {
    // Start trace (also reads debug_ip_layout)
    xclPerfMonStartTrace(s_handle, XCL_PERF_MON_MEMORY, xdp::mTraceOption);
    xclPerfMonStartTrace(s_handle, XCL_PERF_MON_ACCEL, xdp::mTraceOption);
  }

  // Get accelerator names
  uint32_t numAccels = xclGetProfilingNumberSlots(s_handle, XCL_PERF_MON_ACCEL);
  xdp::mProfileMgr->setProfileNumberSlots(XCL_PERF_MON_ACCEL, numAccels);

  for (uint32_t i=0; i < numAccels; ++i) {
    char name[128];
    xclGetProfilingSlotName(s_handle, XCL_PERF_MON_ACCEL, i, name, 128);
    std::string nameStr = name;
    xdp::mProfileMgr->setProfileSlotName(XCL_PERF_MON_ACCEL, xdp::mDeviceName, i, nameStr);

	// TODO: we don't know the kernel name so just use the CU name
    xdp::mProfileMgr->setProfileKernelName(xdp::mDeviceName, nameStr, nameStr);
  }

  // Get accelerator port names
  uint32_t numAccelPorts = xclGetProfilingNumberSlots(s_handle, XCL_PERF_MON_MEMORY);
  xdp::mProfileMgr->setProfileNumberSlots(XCL_PERF_MON_MEMORY, numAccelPorts);

  for (uint32_t i=0; i < numAccelPorts; ++i) {
    char name[128];
    xclGetProfilingSlotName(s_handle, XCL_PERF_MON_MEMORY, i, name, 128);
    std::string nameStr = name;
    xdp::mProfileMgr->setProfileSlotName(XCL_PERF_MON_MEMORY, xdp::mDeviceName, i, nameStr);
  }

  uint32_t numHosts = xclGetProfilingNumberSlots(s_handle, XCL_PERF_MON_HOST);
  xdp::mProfileMgr->setProfileNumberSlots(XCL_PERF_MON_HOST, numHosts);

  if (xdp::mUseProfile) {
    // Read counters
    xclCounterResults counterResults;
    xclPerfMonReadCounters(s_handle, XCL_PERF_MON_MEMORY, counterResults);
    uint64_t timeNsec = xdp::mProfileMgr->time_ns();
    bool firstReadAfterProgram = true;
    xdp::mProfileMgr->logDeviceCounters(xdp::mDeviceName, xdp::mBinaryName, XCL_PERF_MON_MEMORY,
        counterResults, timeNsec, firstReadAfterProgram);
  }
}

void
profile_stop(xclDeviceHandle s_handle)
{
  printf("profile_stop: s_handle=%p\n", s_handle);

  //
  // Profile summary
  //
  if (xdp::mUseProfile) {
    // Read counters
    xclCounterResults counterResults;
    xclPerfMonReadCounters(s_handle, XCL_PERF_MON_MEMORY, counterResults);

    // Store results
    uint64_t timeNsec = xdp::mProfileMgr->time_ns();
    bool firstReadAfterProgram = false;
	xdp::mProfileMgr->logDeviceCounters(xdp::mDeviceName, xdp::mBinaryName, XCL_PERF_MON_MEMORY,
        counterResults, timeNsec, firstReadAfterProgram);

    // Stop counters
    xclPerfMonStopCounters(s_handle, XCL_PERF_MON_MEMORY);
  }

  //
  // Timeline Trace
  //
  if (xdp::mUseTrace) {
    // Data transfers
    xclTraceResultsVector traceVector = {0};
    xclPerfMonReadTrace(s_handle, XCL_PERF_MON_MEMORY, traceVector);
    xdp::mProfileMgr->logTrace(XCL_PERF_MON_MEMORY, xdp::mDeviceName, xdp::mBinaryName, traceVector);

    // Accelerators
    xclPerfMonReadTrace(s_handle, XCL_PERF_MON_ACCEL, traceVector);
    xdp::mProfileMgr->logTrace(XCL_PERF_MON_ACCEL, xdp::mDeviceName, xdp::mBinaryName, traceVector);

    // Stop trace
    xclPerfMonStopTrace(s_handle, XCL_PERF_MON_MEMORY);
    xclPerfMonStopTrace(s_handle, XCL_PERF_MON_ACCEL);
  }
}

void
profile_finalize(xclDeviceHandle s_handle)
{
  printf("profile_finalize: s_handle=%p\n", s_handle);

  //
  // Profile summary
  //
  if (xdp::mUseProfile) {
    // Write profile summary
    xdp::mProfileMgr->writeProfileSummary();

    // Close writer and delete
    xdp::mProfileMgr->detach(xdp::mWriter);
    delete xdp::mWriter;
  }

#if 0
  //
  // Timeline Trace
  //
  if (xdp::mUseTrace) {
    if (!xdp::mTraceStream.is_open()) {
      printf("WARNING: Please run xma_plg_start_trace before starting application.");
      return;
    }

    xclDeviceInfo2 deviceInfo;
    xclGetDeviceInfo2(s_handle, &deviceInfo);

    // Write footer & close
    xdp::writeTraceFooter(deviceInfo, xdp::mTraceStream);
    xdp::mTraceStream.close();
  }
#endif
}

int
xclSyncBOWithProfile(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir,
                     size_t size, size_t offset)
{
  static std::atomic<int> id(0);
  int rc;


  int localid = ++id;

  xclBOProperties p;
  uint64_t boAddr = !xclGetBOProperties(handle, boHandle, &p) ? p.paddr : -1;

  xdp::mProfileMgr->logDataTransfer(
    static_cast<uint64_t>(boHandle)
     ,((dir == XCL_BO_SYNC_BO_TO_DEVICE) ? "WRITE_BUFFER" : "READ_BUFFER")
     ,"START"
     ,std::to_string(localid)
     ,""
     ,size
     ,boAddr
     ,"Unknown"
     ,std::this_thread::get_id());

  rc = xclSyncBO(handle, boHandle, dir, size, offset);

  xdp::mProfileMgr->logDataTransfer(
    static_cast<uint64_t>(boHandle)
     ,((dir == XCL_BO_SYNC_BO_TO_DEVICE) ? "WRITE_BUFFER" : "READ_BUFFER")
     ,"END"
     ,std::to_string(localid)
     ,""
     ,size
     ,boAddr
     ,"Unknown"
     ,std::this_thread::get_id());

  return rc;
}
#endif
