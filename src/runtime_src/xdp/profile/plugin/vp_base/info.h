/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

  const uint64_t aie_profile     = 0x0001 ;
  const uint64_t aie_trace       = 0x0002 ;
  const uint64_t device_offload  = 0x0004 ;
  const uint64_t hal             = 0x0008 ;
  const uint64_t lop             = 0x0010 ;
  const uint64_t native          = 0x0020 ;
  const uint64_t noc             = 0x0040 ;
  const uint64_t opencl_counters = 0x0080 ;
  const uint64_t opencl_trace    = 0x0100 ;
  const uint64_t power           = 0x0200 ;
  const uint64_t system_compiler = 0x0400 ;
  const uint64_t user            = 0x0800 ;
  const uint64_t vart            = 0x1000 ;

} // end namespace info
} // end namespace xdp ;

#endif
