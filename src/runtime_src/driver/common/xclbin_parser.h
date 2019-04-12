/**
 * Copyright (C) 2019 Xilinx, Inc
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

#ifndef xclbin_parser_h_
#define xclbin_parser_h_

#include "driver/include/xclbin.h"
#include <vector>

namespace xrt_core { namespace xclbin {

std::vector<uint64_t>
get_cus(const axlf* top, bool encoding);

std::vector<std::pair<uint64_t, size_t> >
get_debug_ips(const axlf* top);

uint64_t
get_cu_base_offset(const axlf* top);

bool
get_cuisr(const axlf* top);

bool
get_dataflow(const axlf* top);

/**
 * get_cus_pair() - Get list CUs physical address & size pair
 */
std::vector<std::pair<uint64_t, size_t> >
get_cus_pair(const axlf* top);

/**
 * get_dbg_ips_pair() - Get list of Debug IPs physical address & size pair
 */
std::vector<std::pair<uint64_t, size_t> >
get_dbg_ips_pair(const axlf* top);

} // xclbin
} // xrt_core

#endif
