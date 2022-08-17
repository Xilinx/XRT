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

#ifndef _XDP_DEVICE_TRACEDEFS_H_
#define _XDP_DEVICE_TRACEDEFS_H_

#define TRACE_PACKET_SIZE        8
// Device timestamp is 45 bits so it can never be this value
#define INVALID_DEVICE_TIMESTAMP 0xffffffffffffffff

// Property bit masks
#define XCL_PERF_MON_TRACE_MASK 0x1
#define TS2MM_AIE_TRACE_MASK    0x1

// Offsets
#define TS2MM_COUNT_LOW         0x10
#define TS2MM_COUNT_HIGH        0x14
#define TS2MM_RST               0x1c
#define TS2MM_WRITE_OFFSET_LOW  0x2c
#define TS2MM_WRITE_OFFSET_HIGH 0x30
#define TS2MM_WRITTEN_LOW       0x38
#define TS2MM_WRITTEN_HIGH      0x3c
#define TS2MM_CIRCULAR_BUF      0x50
#define TS2MM_AP_CTRL           0x0

// Command for HLS ap_start
#define TS2MM_AP_START          0x1

// little less than 4GB
#define TS2MM_MAX_BUF_SIZE      0xffffefff
// 1 MB
#define TS2MM_DEF_BUF_SIZE      0x100000
// 8KB
#define TS2MM_MIN_BUF_SIZE      0x2000
// Throw warning when we process > 50MB trace
#define TS2MM_WARN_BIG_BUF_SIZE 0x3200000
// Read data only if it's more than 512B unless forced
#define TS2MM_MIN_READ_SIZE      0x200
#define DEFAULT_TRACE_OFFLOAD_INTERVAL_MS 10
// Throw warning when too much trace in processing pipeline
// Use some arbitrary large number here
#define TS2MM_QUEUE_SZ_WARN_THRESHOLD 5000

// In some cases, we cannot use coarse mode
#define COARSE_MODE_UNSUPPORTED "Coarse mode cannot be enabled. Defaulting to fine mode. Please check compilation for details."

#define FIFO_WARN_MSG "Trace FIFO is full because of too many events. Device trace could be incomplete. Suggested fixes:\n\
1. Use larger FIFO size or DDR/HBM bank as 'trace_memory' in linking options.\n\
2. Use 'coarse' option for device_trace and/or turn off stall_trace in runtime settings."

#define CONTINUOUS_OFFLOAD_WARN_MSG_FIFO   "Continuous offload is currently not supported in FIFO trace offload. Disabling this option."

#define TS2MM_WARN_MSG_BUFSIZE_BIG    "Trace Buffer size is too big. The maximum size of 4095M will be used."
#define TS2MM_WARN_MSG_BUFSIZE_SMALL  "Trace Buffer size is too small. The minimum size of 8K will be used."
#define TS2MM_WARN_MSG_BUFSIZE_DEF    "Trace Buffer size could not be parsed. The default size of 1M will be used."
#define TS2MM_WARN_MSG_ALLOC_FAIL     "Trace Buffer could not be allocated on device. Device trace will be missing."
#define TS2MM_WARN_MSG_BUF_FULL       "Trace Buffer is full. Device trace could be incomplete. \
Please increase trace_buffer_size or use 'coarse' option for device_trace or turn on continuous_trace."
#define TS2MM_WARN_MSG_CIRC_BUF       "Device trace will be limited to trace buffer size due to insufficient trace offload rate. Please increase trace \
buffer size and/or reduce trace_buffer_offload_interval."
#define TS2MM_WARN_MSG_CIRC_BUF_OVERWRITE   "Circular buffer overwrite was detected in device trace. Timeline trace could be incomplete."
#define TS2MM_WARN_MSG_BIG_BUF         "Processing large amount of device trace. It could take a while before application ends."
#define TS2MM_WARN_MSG_QUEUE_SZ        "Too much trace in processing queue. This could have negative impact on host memory utilization. \
Please increase trace_buffer_size and trace_buffer_offload_interval together or use 'coarse' option for device_trace."

// Throw warning if less than 8M is used or rate is less than 8 GB/S
#define AIE_MIN_SIZE_CIRCULAR_BUF 0x800000
#define AIE_CIR_BUF_MIN_RATE_PLIO 0x200000000

#define AIE_TRACE_UNAVAILABLE "Neither PLIO nor GMIO trace infrastucture is found in the given design. So, AIE event trace will not be available."
#define AIE_TRACE_BUF_ALLOC_FAIL              "Allocation of buffer for AIE trace failed. AIE trace will not be available."
#define AIE_TS2MM_WARN_MSG_BUF_FULL           "AIE Trace Buffer is full. Device trace could be incomplete."
#define AIE_TS2MM_WARN_MSG_CIRC_BUF_OVERWRITE "Circular buffer overwrite was detected in device trace. AIE trace could be incomplete."
#define AIE_TRACE_BUF_REUSE_WARN              "AIE trace reuse setting may lead to buffer overrun. Please increase \
aie_trace_buffer_size and/or reduce aie_trace_buffer_offload_interval_us. Recommended (min) trace buffer size per stream : \
functions : 8M functions_partial_stalls : 16M functions_all_stalls 32M. For large AIE designs, use granular \
trace settings."
#define AIE_TRACE_WARN_REUSE_PERIODIC  "AIE Trace Buffer reuse only supported with periodic offload."
#define AIE_TRACE_WARN_REUSE_GMIO      "AIE Trace buffer reuse is not supported on GMIO trace."
#define AIE_TRACE_PERIODIC_OFFLOAD_UNSUPPORTED "Continuous offload of AIE Trace is not supported for GMIO mode. So, AIE Trace for GMIO mode will be offloaded only at the end of application."
#define AIE_TRACE_CIRC_BUF_EN          "Circular buffers enabled for AIE trace."

// Trace file Dump Settings and Warnings
#define MIN_TRACE_DUMP_INTERVAL_S 1
#define TRACE_DUMP_INTERVAL_WARN_MSG "Setting trace file dump interval to minimum supported value of 1 second."
#define AIE_TRACE_DUMP_INTERVAL_WARN_MSG "Setting AIE trace file dump interval to minimum supported value of 1 second."
#define TRACE_DUMP_FILE_COUNT_WARN 10
#define TRACE_DUMP_FILE_COUNT_WARN_MSG "Continuous Trace might create a large number of trace files. Please use trace_file_dump_interval \
to control how often trace data is written."

namespace xdp {

// Ease of use constants
constexpr unsigned int BITS_PER_WORD = 32;
constexpr unsigned int BYTES_PER_WORD = 4;
constexpr unsigned int BYTES_64BIT = 8;
constexpr unsigned int BYTES_128BIT = 16;

constexpr uint32_t NUM_TRACE_EVENTS = 8;
constexpr uint32_t NUM_OUTPUT_TRACE_EVENTS = 9;
constexpr uint32_t NUM_BROADCAST_EVENTS = 16;
constexpr uint32_t EVENT_CORE_ACTIVE = 28;
constexpr uint32_t EVENT_CORE_DISABLED = 29;
constexpr uint32_t BROADCAST_MASK_DEFAULT = 65535;
constexpr uint32_t NUM_TRACE_PCS = 4;
constexpr uint32_t NUM_MEM_TRACE_PCS = 2;

constexpr uint32_t NUM_COMBO_EVENT_CONTROL = 3;
constexpr uint32_t NUM_COMBO_EVENT_INPUT = 4;

}

#endif
