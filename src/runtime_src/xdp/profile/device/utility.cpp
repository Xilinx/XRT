/**
 * Copyright (C) 2022 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_CORE_SOURCE

#include "xdp/profile/device/utility.h"

namespace xdp {

  uint64_t getAIMSlotId(uint64_t idx) {
    return ((idx - min_trace_id_aim)/num_trace_id_per_aim);
  }

  uint64_t getAMSlotId(uint64_t idx) {
    return ((idx - min_trace_id_am)/num_trace_id_per_am);
  }

  uint64_t getASMSlotId(uint64_t idx) {
    return ((idx - min_trace_id_asm)/num_trace_id_per_asm);
  }

} // end namespace xdp

