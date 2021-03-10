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

#ifndef _XDP_XOCL_PROFILE_H_
#define _XDP_XOCL_PROFILE_H_

/**
 * This file contains xocl core object helper code for profiling
 */

#include "xdp/profile/device/device_intf.h"
#include "xclperf.h"
#include "xcl_app_debug.h"
#include "xocl/core/object.h"
#include "xocl/core/platform.h"
#include "xocl/core/device.h"
#include "xocl/core/event.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/kernel.h"
#include "xocl/core/context.h"
#include "xocl/core/program.h"
#include "xocl/core/range.h"
#include "xocl/core/execution_context.h"
#include "core/include/experimental/xrt_kernel.h"
#include <map>
#include <memory>
#include <cmath>
#include <string>

namespace xdp { namespace xoclp {

void
get_cu_start(const xocl::execution_context* ctx, const xrt::run& run);
void
get_cu_done(const xocl::execution_context* ctx, const xrt::run& run);

//
// Platform
//
namespace platform {

using key = const xocl::platform*;

void
init(key k);

unsigned int
get_profile_num_slots(key k, const std::string& deviceName, xclPerfMonType type);

cl_int
get_profile_slot_name(key k, const std::string& deviceName, xclPerfMonType type,
		              unsigned int slotnum, std::string& slotName);

cl_int
get_trace_slot_name(key k, const std::string& deviceName, xclPerfMonType type,
		              unsigned int slotnum, std::string& slotName);

unsigned int
get_profile_slot_properties(key k, const std::string& deviceName, xclPerfMonType type,
		              unsigned int slotnum);

unsigned int
get_trace_slot_properties(key k, const std::string& deviceName, xclPerfMonType type,
		              unsigned int slotnum);

cl_int
get_profile_kernel_name(key k, const std::string& deviceName, const std::string& cuName, std::string& kernelName);

size_t 
get_device_timestamp(key k, const std::string& deviceName);

double 
get_device_max_read(key k);

double 
get_device_max_write(key k);

cl_int 
start_device_trace(key k, xclPerfMonType type, size_t numComputeUnits);

cl_int 
stop_device_trace(key k, xclPerfMonType type);

cl_int 
log_device_trace(key k, xclPerfMonType type, bool forceRead);

cl_int 
start_device_counters(key k, xclPerfMonType type);

cl_int 
stop_device_counters(key k, xclPerfMonType type);

cl_int 
log_device_counters(key k, xclPerfMonType type, bool firstReadAfterProgram,
                    bool forceRead);

cl_int
debugReadIPStatus(key k, xclDebugReadType type, void* aDebugResults);

unsigned int
get_ddr_bank_count(key k, const std::string& deviceName);

bool 
isValidPerfMonTypeTrace(key k, xclPerfMonType type);

bool 
isValidPerfMonTypeCounters(key k, xclPerfMonType type);

bool
is_ap_ctrl_chain(key k, const std::string& deviceName, const std::string& cu);

// All devices have same buf size
uint64_t
get_ts2mm_buf_size(bool isAIETrace);

//
// Device
//
namespace device {

using key = const xocl::device*;

struct data
{
  bool mPerformingFlush = false;
  xclTraceResultsVector mTraceVector;
  xclCounterResults mCounterResults;
  uint32_t mSamplesThreshold = 0;
  uint32_t mSampleIntervalMsec = 0;
  uint32_t mTrainingIntervalUsec = 0;
  uint32_t mLastTraceNumSamples[XCL_PERF_MON_TOTAL_PROFILE] = {0};
  std::chrono::steady_clock::time_point mLastCountersSampleTime;
  std::chrono::steady_clock::time_point mLastTraceTrainingTime[XCL_PERF_MON_TOTAL_PROFILE];
  DeviceIntf mDeviceIntf;
  bool ts2mm_en = false;
};

unsigned int
getProfileNumSlots(key k, xclPerfMonType type);

cl_int
getProfileSlotName(key k, xclPerfMonType type, unsigned int slotnum, std::string& slotName);

cl_int
getTraceSlotName(key k, xclPerfMonType type, unsigned int slotnum, std::string& slotName);

unsigned int
getProfileSlotProperties(key k, xclPerfMonType type, unsigned int slotnum);

unsigned int
getTraceSlotProperties(key k, xclPerfMonType type, unsigned int slotnum);

cl_int
startTrace(key k, xclPerfMonType type, size_t numComputeUnits);

cl_int 
stopTrace(key k, xclPerfMonType type);

size_t 
getTimestamp(key k);

double 
getMaxRead(key k);

double 
getMaxWrite(key k);

void 
configureDataflow(key k, xclPerfMonType type);

cl_int 
startCounters(key k, xclPerfMonType type);

cl_int 
stopCounters(key k, xclPerfMonType type);

cl_int 
logTrace(key k, xclPerfMonType type, bool forceRead);

cl_int 
logCounters(key k, xclPerfMonType type, bool firstReadAfterProgram, bool forceRead);

//cl_int
//debugReadIPStatus(key k, xclDebugReadType type, void*  aDebugResults);

bool
isAPCtrlChain(key k, const std::string& cu);

uint64_t getMemSizeBytes(key k, int idx);

uint64_t getPlramSizeBytes(key k);

void getMemUsageStats(key k, std::map<std::string, uint64_t>& stats);

} // device
} // platform

}} // profile,xdp

#endif


