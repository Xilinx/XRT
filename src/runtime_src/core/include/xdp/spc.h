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
// AXI Stream Protocol Checker cores that may exist in the design.
// These values are shared between xbutil and the XDP library.

#ifndef SPC_DOT_H
#define SPC_DOT_H

namespace xdp::IP::SPC {

// The total number of 32-bit counters accessible on the IP
constexpr int NUM_COUNTERS = 3;

namespace AXI_LITE {
// These are the actual physical offsets of the 32-bit registers in
// the SPC IP accessible over the AXI-Lite connection.  If using
// xclRead or xclWrite, these offsets are used.
constexpr unsigned int PC_ASSERTED = 0x0;
constexpr unsigned int CURRENT_PC  = 0x100;
constexpr unsigned int SNAPSHOT_PC = 0x200;
} // end namespace AXI_LITE

namespace sysfs {
// When accessing the IP via sysfs, an array of 32-bit words is
// put together and returned.  These numbers are the array offsets to
// access the specific registers returned.
constexpr int PC_ASSERTED = 0;
constexpr int CURRENT_PC  = 1;
constexpr int SNAPSHOT_PC = 2;
} // end namespace sysfs

} // end namespace xdp::IP::SPC

#endif
