/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#ifndef INFO_DOT_H
#define INFO_DOT_H

#include <cstdint>

namespace xdp {
namespace info {

  const uint64_t aie_profile     = 0x00001 ;
  const uint64_t aie_trace       = 0x00002 ;
  const uint64_t device_offload  = 0x00004 ;
  const uint64_t hal             = 0x00008 ;
  const uint64_t lop             = 0x00010 ;
  const uint64_t native          = 0x00020 ;
  const uint64_t noc             = 0x00040 ;
  const uint64_t opencl_counters = 0x00080 ;
  const uint64_t opencl_trace    = 0x00100 ;
  const uint64_t power           = 0x00200 ;
  const uint64_t system_compiler = 0x00400 ;
  const uint64_t user            = 0x00800 ;
  const uint64_t vart            = 0x01000 ;
  const uint64_t aie_status      = 0x02000 ;
  const uint64_t ml_timeline     = 0x04000 ;
  const uint64_t aie_halt        = 0x08000 ;
  const uint64_t aie_pc          = 0x10000 ;
  const uint64_t aie_debug       = 0x20000 ;

} // end namespace info
} // end namespace xdp ;

#endif
