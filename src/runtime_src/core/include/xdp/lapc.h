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

// This file captures all the constants used to access the
// Lightweight AXI Protocol Checker cores that may exist in the design.
// These values are shared between xbutil and the XDP library.

#ifndef LAPC_DOT_H
#define LAPC_DOT_H

namespace xdp::IP::LAPC {

// The total number of the 32-bit registers available on the IP
constexpr int NUM_COUNTERS = 9;
// The status registers are grouped together in groups of four
constexpr int NUM_STATUS = 4;

namespace AXI_LITE {
// These are the actual physical offsets of the 32-bit registers in
// the LAPC IP accessible over the AXI-Lite connection.  If using
// xclRead or xclWrite, these offsets are used.
constexpr unsigned int STATUS              = 0x0;
constexpr unsigned int CUMULATIVE_STATUS_0 = 0x100;
constexpr unsigned int CUMULATIVE_STATUS_1 = 0x104;
constexpr unsigned int CUMULATIVE_STATUS_2 = 0x108;
constexpr unsigned int CUMULATIVE_STATUS_3 = 0x10c;
constexpr unsigned int SNAPSHOT_STATUS_0   = 0x200;
constexpr unsigned int SNAPSHOT_STATUS_1   = 0x204;
constexpr unsigned int SNAPSHOT_STATUS_2   = 0x208;
constexpr unsigned int SNAPSHOT_STATUS_3   = 0x20c;
} // end namespace AXI_LITE

namespace sysfs {
// When accessing the IP via sysfs, an array of 32-bit words is
// put together and returned.  These numbers are the array offsets to
// access the specific registers returned.
constexpr int STATUS              = 0;
constexpr int CUMULATIVE_STATUS_0 = 1;
constexpr int CUMULATIVE_STATUS_1 = 2;
constexpr int CUMULATIVE_STATUS_2 = 3;
constexpr int CUMULATIVE_STATUS_3 = 4;
constexpr int SNAPSHOT_STATUS_0   = 5;
constexpr int SNAPSHOT_STATUS_1   = 6;
constexpr int SNAPSHOT_STATUS_2   = 7;
constexpr int SNAPSHOT_STATUS_3   = 8;
} // end namespace sysfs

} // end namespace xdp::IP::LAPC

#endif
