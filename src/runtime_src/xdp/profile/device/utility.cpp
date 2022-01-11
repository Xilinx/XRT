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

#define XDP_SOURCE

#include "xdp/profile/device/utility.h"

namespace xdp {

  uint64_t getAIMSlotId(uint64_t idx) {
    return ((idx - MIN_TRACE_ID_AIM)/NUM_TRACE_ID_PER_AIM);
  }

  uint64_t getAMSlotId(uint64_t idx) {
    return ((idx - MIN_TRACE_ID_AM)/NUM_TRACE_ID_PER_AM);
  }

  uint64_t getASMSlotId(uint64_t idx) {
    return ((idx - MIN_TRACE_ID_ASM)/NUM_TRACE_ID_PER_ASM);
  }

} // end namespace xdp

