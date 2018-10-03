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

#include "xperf.h"
#include "xdp_profile.h"
#include "xdp_profile_writers.h"
#include "xdp_profile_results.h"

// *****************************************************************************
//                               Helper Functions
// *****************************************************************************

//
// XDP: Helpers, classes, & members
//

// TODO: replace these with functions in XDP library
namespace XDP {

  // Members
  bool mUseProfile;
  bool mUseTrace;
  std::string mDeviceName;
  std::string mBinaryName;
  unsigned short mKernelClockFreq;
  uint32_t mTraceOption;
  std::string mDataTransferTrace;
  std::string mStallTrace;
  //std::ofstream mProfileStream;
  std::ofstream mTraceStream;

  XDP::CSVWriter*  mWriter = nullptr;
  XDP::XDPProfile* mProfileMgr = nullptr;

 // XDP namespace
}

// *****************************************************************************
//                        Top-Level Profile Functions
// *****************************************************************************

void
profile_initialize(xclDeviceHandle s_handle, int use_profile, int use_trace,
                   const char* data_transfer_trace, const char* stall_trace)
{
  printf("profile_initialize: s_handle=%p, use_profile=%d, use_trace=%d, data_transfer_trace=%s, stall_trace=%s\n",
         s_handle, use_profile, use_trace, data_transfer_trace, stall_trace);

  int ProfileFlags = XDP::XDPProfile::PROFILE_APPLICATION;
  XDP::mProfileMgr = new XDP::XDPProfile(ProfileFlags);

  // Evaluate arguments
  XDP::mUseProfile = use_profile;
  XDP::mUseTrace = use_trace;
  XDP::mTraceOption = 0;

  if (XDP::mUseTrace) {
    XDP::mDataTransferTrace = data_transfer_trace;
    XDP::mStallTrace = stall_trace;
    XDP::mTraceOption = (XDP::mDataTransferTrace == "coarse") ? 0x1 : 0x0;
    if (XDP::mStallTrace == "dataflow")    XDP::mTraceOption |= (0x1 << 2);

    else if (XDP::mStallTrace == "pipe")   XDP::mTraceOption |= (0x1 << 3);
    else if (XDP::mStallTrace == "memory") XDP::mTraceOption |= (0x1 << 4);
    else if (XDP::mStallTrace == "all")    XDP::mTraceOption |= (0x7 << 2);
    else printf("The stall_trace setting of %s is not recognized. Please use memory|dataflow|pipe|all|off.", XDP::mStallTrace);
  }

  // Get design info (clock freqs, device/binary names)
  xclDeviceInfo2 deviceInfo;
  xclGetDeviceInfo2(s_handle, &deviceInfo);
  XDP::mKernelClockFreq = deviceInfo.mOCLFrequency[0];
  XDP::mDeviceName = deviceInfo.mName;
  // TODO: do we know this?
  XDP::mBinaryName = "binary";
  XDP::mProfileMgr->setKernelClockFreqMHz(XDP::mDeviceName, XDP::mKernelClockFreq);

  //
  // Profile Summary
  //
  if (XDP::mUseProfile) {
	XDP::mProfileMgr->turnOnProfile(XDP::XDPProfile::PROFILE_DEVICE_COUNTERS);
    XDP::mProfileMgr->turnOnFile(XDP::XDPProfile::FILE_SUMMARY);

    //std::string timelineFile = "xma_timeline_trace.csv";
    //XDP::mWriter = new XDP::CSVWriter(profileFile, timelineFile, "Xilinx");
    XDP::mWriter = new XDP::CSVWriter("sdaccel_profile_summary", "sdaccel_timeline_trace", "Xilinx");
    XDP::mProfileMgr->attach(XDP::mWriter);
  }

  //
  // Timeline Trace
  //
  if (XDP::mUseTrace) {
	XDP::mProfileMgr->turnOnProfile(XDP::XDPProfile::PROFILE_DEVICE_TRACE);
    XDP::mProfileMgr->turnOnFile(XDP::XDPProfile::FILE_TIMELINE_TRACE);

    // Open timeline trace file
    //XDP::mTraceStream.open("xma_timeline_trace.csv");

    // Make an initialization call for time
    XDP::mProfileMgr->time_ns();

    // Write header
    //XDP::writeTraceHeader(XDP::mTraceStream);
  }
}

void
profile_start(xclDeviceHandle s_handle)
{
  printf("profile_start: s_handle=%p\n", s_handle);

  //
  // Profile Summary
  //
  if (XDP::mUseProfile) {
    // Start counters
    xclPerfMonStartCounters(s_handle, XCL_PERF_MON_MEMORY);
  }

  //
  // Timeline Trace
  //
  if (XDP::mUseTrace) {
    // Start trace (also reads debug_ip_layout)
    xclPerfMonStartTrace(s_handle, XCL_PERF_MON_MEMORY, XDP::mTraceOption);
    xclPerfMonStartTrace(s_handle, XCL_PERF_MON_ACCEL, XDP::mTraceOption);
  }

  // Get accelerator names
  uint32_t numAccels = xclGetProfilingNumberSlots(s_handle, XCL_PERF_MON_ACCEL);
  XDP::mProfileMgr->setProfileNumberSlots(XCL_PERF_MON_ACCEL, numAccels);

  for (uint32_t i=0; i < numAccels; ++i) {
    char name[128];
    xclGetProfilingSlotName(s_handle, XCL_PERF_MON_ACCEL, i, name, 128);
    std::string nameStr = name;
    XDP::mProfileMgr->setProfileSlotName(XCL_PERF_MON_ACCEL, XDP::mDeviceName, i, nameStr);

	// TODO: we don't know the kernel name so just use the CU name
    XDP::mProfileMgr->setProfileKernelName(XDP::mDeviceName, nameStr, nameStr);
  }

  // Get accelerator port names
  uint32_t numAccelPorts = xclGetProfilingNumberSlots(s_handle, XCL_PERF_MON_MEMORY);
  XDP::mProfileMgr->setProfileNumberSlots(XCL_PERF_MON_MEMORY, numAccelPorts);

  for (uint32_t i=0; i < numAccelPorts; ++i) {
    char name[128];
    xclGetProfilingSlotName(s_handle, XCL_PERF_MON_MEMORY, i, name, 128);
    std::string nameStr = name;
    XDP::mProfileMgr->setProfileSlotName(XCL_PERF_MON_MEMORY, XDP::mDeviceName, i, nameStr);
  }

  uint32_t numHosts = xclGetProfilingNumberSlots(s_handle, XCL_PERF_MON_HOST);
  XDP::mProfileMgr->setProfileNumberSlots(XCL_PERF_MON_HOST, numHosts);

  if (XDP::mUseProfile) {
    // Read counters
    xclCounterResults counterResults;
    xclPerfMonReadCounters(s_handle, XCL_PERF_MON_MEMORY, counterResults);
    uint64_t timeNsec = XDP::mProfileMgr->time_ns();
    bool firstReadAfterProgram = true;
    XDP::mProfileMgr->logDeviceCounters(XDP::mDeviceName, XDP::mBinaryName, XCL_PERF_MON_MEMORY,
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
  if (XDP::mUseProfile) {
    // Read counters
    xclCounterResults counterResults;
    xclPerfMonReadCounters(s_handle, XCL_PERF_MON_MEMORY, counterResults);

    // Store results
    uint64_t timeNsec = XDP::mProfileMgr->time_ns();
    bool firstReadAfterProgram = false;
	XDP::mProfileMgr->logDeviceCounters(XDP::mDeviceName, XDP::mBinaryName, XCL_PERF_MON_MEMORY,
        counterResults, timeNsec, firstReadAfterProgram);

    // Stop counters
    xclPerfMonStopCounters(s_handle, XCL_PERF_MON_MEMORY);
  }

  //
  // Timeline Trace
  //
  if (XDP::mUseTrace) {
    // Data transfers
    xclTraceResultsVector traceVector = {0};
    xclPerfMonReadTrace(s_handle, XCL_PERF_MON_MEMORY, traceVector);
    XDP::mProfileMgr->logTrace(XCL_PERF_MON_MEMORY, XDP::mDeviceName, XDP::mBinaryName, traceVector);

    // Accelerators
    xclPerfMonReadTrace(s_handle, XCL_PERF_MON_ACCEL, traceVector);
    XDP::mProfileMgr->logTrace(XCL_PERF_MON_ACCEL, XDP::mDeviceName, XDP::mBinaryName, traceVector);

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
  if (XDP::mUseProfile) {
    // Write profile summary
    XDP::mProfileMgr->writeProfileSummary();

    // Close writer and delete
    XDP::mProfileMgr->detach(XDP::mWriter);
    delete XDP::mWriter;
  }

#if 0
  //
  // Timeline Trace
  //
  if (XDP::mUseTrace) {
    if (!XDP::mTraceStream.is_open()) {
      printf("WARNING: Please run xma_plg_start_trace before starting application.");
      return;
    }

    xclDeviceInfo2 deviceInfo;
    xclGetDeviceInfo2(s_handle, &deviceInfo);

    // Write footer & close
    XDP::writeTraceFooter(deviceInfo, XDP::mTraceStream);
    XDP::mTraceStream.close();
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

  XDP::mProfileMgr->logDataTransfer(
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

  XDP::mProfileMgr->logDataTransfer(
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
