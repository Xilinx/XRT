/**
 * Copyright (C) 2022 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
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
#include "xdp/config.h"

namespace xdp {

  XDP_EXPORT
  uint64_t getAIMSlotId(uint64_t idx);

  XDP_EXPORT
  uint64_t getAMSlotId(uint64_t idx);

  XDP_EXPORT
  uint64_t getASMSlotId(uint64_t idx);

  // At compile time, each monitor inserted in the PL region is given a set 
  // of trace IDs, regardless of if trace is enabled or not.  This ID is
  // embedded in the PL events and used by the XDP library to identify the
  // type and source of each hw event.

  // In order to differentiate between reads and writes, each AIM is assigned
  // two trace IDs.  At compile time, we can only insert up to 31 AIMs in the
  // PL region.
  constexpr int NUM_TRACE_ID_PER_AIM = 2;
  constexpr int MIN_TRACE_ID_AIM     = 0;
  constexpr int MAX_TRACE_ID_AIM     = 61;

  // Because of the different stalls each compute unit can create, each AM
  // is assigned sixteen trace IDs.  At compile time, we can only insert
  // up to 31 AMs in the PL region.
  constexpr int NUM_TRACE_ID_PER_AM = 16;
  constexpr int MIN_TRACE_ID_AM     = 64;
  constexpr int MAX_TRACE_ID_AM     = 544;

  // Each ASM is assigned a single trace ID.  At compile time, we can only
  // insert up to 31 ASMs in the PL region.
  constexpr int NUM_TRACE_ID_PER_ASM = 1;
  constexpr int MIN_TRACE_ID_ASM     = 576;
  constexpr int MAX_TRACE_ID_ASM     = 607;

} // end namespace xdp

#endif
