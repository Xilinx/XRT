/*
 * Copyright (C) 2021-2022 Xilinx, Inc
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

#ifndef XRT_CORE_DEBUG_IP_H
#define XRT_CORE_DEBUG_IP_H

#include "core/common/config.h"

#include<vector>
#include<cstdint>

struct debug_ip_data;

namespace xrt_core {

class device;

namespace debug_ip {

  XRT_CORE_COMMON_EXPORT
  std::vector<uint64_t> 
  get_aim_counter_result(const xrt_core::device* , debug_ip_data*);

  XRT_CORE_COMMON_EXPORT
  std::vector<uint64_t> 
  get_am_counter_result(const xrt_core::device* , debug_ip_data*);

  XRT_CORE_COMMON_EXPORT
  std::vector<uint64_t> 
  get_asm_counter_result(const xrt_core::device* , debug_ip_data*);

  XRT_CORE_COMMON_EXPORT
  std::vector<uint32_t> 
  get_lapc_status(const xrt_core::device* , debug_ip_data*);

  XRT_CORE_COMMON_EXPORT
  std::vector<uint32_t> 
  get_spc_status(const xrt_core::device* , debug_ip_data*);

  XRT_CORE_COMMON_EXPORT
  uint64_t 
  get_accel_deadlock_status(const xrt_core::device* , debug_ip_data*);

} } // namespace debug_ip, xrt_core

#endif
