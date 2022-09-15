/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc - All rights reserved
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

// This file captures all the constants used to access the AXI Stream Monitor
// cores that may exist in the design.  These values are shared between
// the xbutil code base and the XDP library

#ifndef ASM_DOT_H
#define ASM_DOT_H

namespace xdp::IP::ASM {

// The total number of 64-bit counters accessible on the IP
constexpr int NUM_COUNTERS = 5;

namespace AXI_LITE {
// These are the actual physical offsets of the 64-bit registers in
// the ASM IP accessible over the AXI-Lite connection.  If using
// xclRead or xclWrite, these offsets are used.
constexpr unsigned int CONTROL       = 0x00;
constexpr unsigned int SAMPLE        = 0x20;
constexpr unsigned int NUM_TRANX     = 0x80;
constexpr unsigned int DATA_BYTES    = 0x88;
constexpr unsigned int BUSY_CYCLES   = 0x90;
constexpr unsigned int STALL_CYCLES  = 0x98;
constexpr unsigned int STARVE_CYCLES = 0xA0;
} // end namespace AXI_LITE

namespace sysfs {
// When accessing the IP via sysfs, an array of 64-bit words is
// put together and returned.  These numbers are the array offsets
// to access the specific registers returned.
constexpr int NUM_TRANX     = 0;
constexpr int DATA_BYTES    = 1;
constexpr int BUSY_CYCLES   = 2;
constexpr int STALL_CYCLES  = 3;
constexpr int STARVE_CYCLES = 4;
} // end namespace sysfs

namespace mask {
constexpr unsigned int COUNTER_RESET = 0x00000001;
constexpr unsigned int TRACE_ENABLE  = 0x00000002;
constexpr unsigned int TRACE_CTRL    = 0x2;
} // end namespace mask

} // end namespace xdp::IP::ASM

#endif
