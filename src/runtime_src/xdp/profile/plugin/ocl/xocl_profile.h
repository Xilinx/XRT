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

#ifndef _XDP_XOCL_PROFILE_H_
#define _XDP_XOCL_PROFILE_H_

/**
 * This file contains xocl core object helper code for profiling
 */

#include "driver/include/xclperf.h"
#include "driver/include/xcl_app_debug.h"
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
#include "xocl/xclbin/xclbin.h"

#include <map>
#include <memory>
#include <cmath>
#include <string>

namespace xdp { namespace xoclp {
//
// CU profiling callbacks
//
uint32_t
get_num_cu_masks(uint32_t header);
uint32_t
get_cu_index_mask(uint32_t cumask);
unsigned int
get_cu_index(const xrt::command* cmd);

void
get_cu_start(const xrt::command* cmd, const xocl::execution_context* ctx);
void
get_cu_done(const xrt::command* cmd, const xocl::execution_context* ctx);

//
// Platform
//
namespace platform {

using key = const xocl::platform*;

void
init(key k);

bool
is_unified(key k);

cl_int
set_profile_num_slots(key k, xclPerfMonType type, unsigned numSlots);

unsigned
get_profile_num_slots(key k, std::string& deviceName, xclPerfMonType type);

cl_int
get_profile_slot_name(key k, std::string& deviceName, xclPerfMonType type,
		              unsigned slotnum, std::string& slotName);

unsigned
get_profile_slot_properties(key k, std::string& deviceName, xclPerfMonType type,
		              unsigned slotnum);

cl_int
get_profile_kernel_name(key k, const std::string& deviceName, const std::string& cuName, std::string& kernelName);

cl_int
write_host_event(key k, xclPerfMonEventType type, xclPerfMonEventID id);

size_t 
get_device_timestamp(key k, std::string& deviceName);

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
};

void
init(key k);

cl_int
setProfileNumSlots(key k, xclPerfMonType type, unsigned numSlots);

unsigned
getProfileNumSlots(key k, xclPerfMonType type);

cl_int
getProfileSlotName(key k, xclPerfMonType type, unsigned slotnum, std::string& slotName);

unsigned
getProfileSlotProperties(key k, xclPerfMonType type, unsigned slotnum);

cl_int 
writeHostEvent(key k, xclPerfMonEventType type, xclPerfMonEventID id);

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

cl_int 
startCounters(key k, xclPerfMonType type);

cl_int 
stopCounters(key k, xclPerfMonType type);

cl_int 
logTrace(key k, xclPerfMonType type, bool forceRead);

cl_int 
logCounters(key k, xclPerfMonType type, bool firstReadAfterProgram, bool forceRead);

cl_int
debugReadIPStatus(key k, xclDebugReadType type, void*  aDebugResults);

} // device
} // platform

}} // profile,xdp

#endif


