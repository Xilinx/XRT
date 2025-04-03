/**
 * Copyright (C) 2022 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef XDP_DEVICE_UTILITY_DOT_H
#define XDP_DEVICE_UTILITY_DOT_H

// Functions that can be used in the database, the plugins, and the writers

#include <stdint.h>
#include <string>
#include <memory>
#include "xdp/config.h"
#include "xrt/xrt_device.h"

namespace xdp::util {

  XDP_CORE_EXPORT
  uint64_t getAIMSlotId(uint64_t idx);

  XDP_CORE_EXPORT
  uint64_t getAMSlotId(uint64_t idx);

  XDP_CORE_EXPORT
  uint64_t getASMSlotId(uint64_t idx);

  XDP_CORE_EXPORT
  std::string getDebugIpLayoutPath(void* deviceHandle);

  XDP_CORE_EXPORT
  std::string getDeviceName(void* deviceHandle, bool hw_context_flow = false);

  XDP_CORE_EXPORT
  std::shared_ptr<xrt_core::device>
  convertToCoreDevice(void* h, bool hw_context_flow);


  // At compile time, each monitor inserted in the PL region is given a set 
  // of trace IDs, regardless of if trace is enabled or not.  This ID is
  // embedded in the PL events and used by the XDP library to identify the
  // type and source of each hw event.

  // In order to differentiate between reads and writes, each AIM is assigned
  // two trace IDs.  At compile time, we can only insert up to 31 AIMs in the
  // PL region.
  constexpr int num_trace_id_per_aim = 2;
  constexpr int min_trace_id_aim     = 0;
  constexpr int max_trace_id_aim     = 61;

  // Because of the different stalls each compute unit can create, each AM
  // is assigned sixteen trace IDs.  At compile time, we can only insert
  // up to 31 AMs in the PL region.
  constexpr int num_trace_id_per_am = 16;
  constexpr int min_trace_id_am     = 64;
  constexpr int max_trace_id_am     = 544;

  // Each ASM is assigned a single trace ID.  At compile time, we can only
  // insert up to 31 ASMs in the PL region.
  constexpr int num_trace_id_per_asm = 1;
  constexpr int min_trace_id_asm     = 576;
  constexpr int max_trace_id_asm     = 607;

  constexpr unsigned int sysfs_max_path_length = 512;

} // end namespace xdp::util

#endif
