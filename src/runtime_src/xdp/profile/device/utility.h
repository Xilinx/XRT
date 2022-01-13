/**
 * Copyright (C) 2022 Xilinx, Inc
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

/************************ Trace IDs ************************************/

#define MIN_TRACE_ID_AIM       0
#define MAX_TRACE_ID_AIM       61
/* To differentiate between reads and writes, AIMs can produce up
 *  to 2 different trace IDs in their trace packets.
 */
#define NUM_TRACE_ID_PER_AIM   2

#define MIN_TRACE_ID_AM        64
#define MAX_TRACE_ID_AM        544
#define MAX_TRACE_ID_AM_HWEM   94
/* Because of the different stalls, AMs can produce up to 16 different
 * trace IDs in their trace packets.
 */
#define NUM_TRACE_ID_PER_AM    16

#define MIN_TRACE_ID_ASM       576
#define MAX_TRACE_ID_ASM       607
// ASMs only generate one type of trace ID in their trace packets.
#define NUM_TRACE_ID_PER_ASM   1

#include <stdint.h>
#include "xdp/config.h"

namespace xdp {

  XDP_EXPORT
  uint64_t getAIMSlotId(uint64_t idx);

  XDP_EXPORT
  uint64_t getAMSlotId(uint64_t idx);

  XDP_EXPORT
  uint64_t getASMSlotId(uint64_t idx);

} // end namespace xdp

#endif
