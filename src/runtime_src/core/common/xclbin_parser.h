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

#include "xclbin.h"
#include <string>
#include <vector>

namespace xrt_core { namespace xclbin {

/**
 * Get specific binary section of the axlf structure
 *
 * auto data = axlf_section_type::get<const ip_layout*>(top,axlf_section_kind::IP_LAYOUT);
 */
template <typename SectionType>
struct axlf_section_type;

template <typename SectionType>
struct axlf_section_type<SectionType*>
{
  static SectionType*
  get(const axlf* top, axlf_section_kind kind)
  {
    if (auto header = ::xclbin::get_axlf_section(top, kind)) {
      auto begin = reinterpret_cast<const char*>(top) + header->m_sectionOffset ;
      return reinterpret_cast<SectionType*>(begin);
    }
    return nullptr;
  }
};

/**
 * memidx_to_name() - Convert mem topology memory index to name
 */
std::string
memidx_to_name(const axlf* top, int32_t midx);

/**
 * get_cus() - Get sorted list of CU base addresses in xclbin.
 *
 * @encode: If true encode control protocol in lower address bit
 */
std::vector<uint64_t>
get_cus(const axlf* top, bool encode=false);

std::vector<std::pair<uint64_t, size_t>>
get_debug_ips(const axlf* top);

/**
 * get_cu_control() - Get the IP_CONTROL type of CU at specified address
 */
uint32_t
get_cu_control(const axlf* top, uint64_t cuaddr);

/**
 * get_cu_base_offset() - Get minimum base offset of all IP_KERNEL objects
 */
uint64_t
get_cu_base_offset(const axlf* top);

/**
 * get_cuisr() - Check if all kernels support interrupt
 */
bool
get_cuisr(const axlf* top);

/**
 * get_dataflow() - Check if any kernel in xclbin is a dataflow kernel
 */
bool
get_dataflow(const axlf* top);

/**
 * get_cus_pair() - Get list CUs physical address & size pair
 */
std::vector<std::pair<uint64_t, size_t>>
get_cus_pair(const axlf* top);

/**
 * get_dbg_ips_pair() - Get list of Debug IPs physical address & size pair
 */
std::vector<std::pair<uint64_t, size_t>>
get_dbg_ips_pair(const axlf* top);

} // xclbin
} // xrt_core

#endif
