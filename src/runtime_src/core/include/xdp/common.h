/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc.  All rights reserved
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

#ifndef XDP_COMMON_DOT_H
#define XDP_COMMON_DOT_H

// This file contains the common debug/profiling declarations used by
// distinct parts of the code base to interface with the profiling
// IP on the hardware inserted into the reconfigurable region.

namespace xdp {
// On the tools side, there are hard limits to the total number of
// monitors that can be inserted into the reconfigurable region.  If
// the tools change, these limits will have to be updated.

// On some platforms, there are AIMs in the static region configured for
// counters only (no trace).  On those platforms we can have 31 AIMs in the
// PL region + 3 in the static region for a total of 34.

constexpr int MAX_NUM_AMS   = 31; // Max Number of Accelerator Monitor
constexpr int MAX_NUM_AIMS  = 34; // Max Number of AXI Interface Monitors
constexpr int MAX_NUM_ASMS  = 31; // Max Number of AXI Stream Monitors
constexpr int MAX_NUM_LAPCS = 31; // Max Number of AXI Protocol Checkers
constexpr int MAX_NUM_SPCS  = 31; // Max Number of AXI Stream Protocol Checker

// This enumerates all of the different types of performance monitors
// possibly inserted into the hardware.  It is separate from ILA or debug
// hub types.  This enumeration is used in the different shims, the XDP
// library, and the HAL API interface for reading profiling IP.

enum MonitorType : unsigned int {
  memory = 0,
  host = 1,
  shell = 2,
  accel = 3,
  stall = 4,
  str = 5,
  fifo = 6,
  noc = 7,
  total = 8
};

} // end namespace xdp

#endif
