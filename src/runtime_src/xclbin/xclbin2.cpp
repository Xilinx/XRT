/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#include "binary.h"

#include "driver/include/xclbin.h"

#include <algorithm>
#include <iostream>

namespace xclbin {

/**
 * class xclbin2 (axlf)
 *
 * Encapsulated within the implementation of binary without
 * exposing implemenation details.
 */
struct xclbin2 : public binary::impl
{
  const std::vector<char> m_xclbin;
  const char* m_raw = nullptr;
  const axlf* m_axlf = nullptr;
  const axlf_header* m_header = nullptr;

  explicit
  xclbin2(std::vector<char>&& xb)
    : m_xclbin(std::move(xb)), m_raw(&m_xclbin[0])
    , m_axlf(reinterpret_cast<const axlf*>(m_raw))
    , m_header(&m_axlf->m_header)
  {
    if (m_xclbin.size() < sizeof(axlf))
      throw error("bad axlf file");

    if (m_xclbin.size() < m_header->m_length)
      throw error ("axlf length mismatch");
  }

  size_t
  size() const
  {
    return m_header->m_length;
  }

  std::string
  version() const
  {
    return m_axlf->m_magic;
  }

  data_range
  binary_data() const
  {
    auto begin = m_raw;
    return std::make_pair(begin,begin+m_header->m_length);
  }

  data_range
  meta_data() const
  {
    if (auto header = xclbin::get_axlf_section(m_axlf,axlf_section_kind::EMBEDDED_METADATA)) {
      auto begin = m_raw+header->m_sectionOffset;
      return std::make_pair(begin,begin+header->m_sectionSize);
    }
    throw error("axlf contains no meta data section");
  }

  data_range
  debug_data() const
  {
    if (auto header = xclbin::get_axlf_section(m_axlf, axlf_section_kind::DEBUG_DATA))
    {
      auto begin = m_raw + header->m_sectionOffset ;
      return std::make_pair(begin, begin + header->m_sectionSize) ;
    }
    //throw error("axlf contains no debug data section") ;
    return std::make_pair(nullptr,nullptr);
  }

  data_range
  connectivity_data() const
  {
    if (auto header = xclbin::get_axlf_section(m_axlf, axlf_section_kind::CONNECTIVITY))
    {
      auto begin = m_raw + header->m_sectionOffset ;
      return std::make_pair(begin, begin + header->m_sectionSize) ;
    }
    //throw error("axlf contains no connectivity data section") ;
    return std::make_pair(nullptr,nullptr);
  }

  data_range
  mem_topology_data() const
  {
    if (auto header = xclbin::get_axlf_section(m_axlf, axlf_section_kind::MEM_TOPOLOGY))
    {
      auto begin = m_raw + header->m_sectionOffset ;
      return std::make_pair(begin, begin + header->m_sectionSize) ;
    }
    //throw error("axlf contains no mem topology data section") ;
    return std::make_pair(nullptr,nullptr);
  }

  data_range
  ip_layout_data() const
  {
    if (auto header = xclbin::get_axlf_section(m_axlf, axlf_section_kind::IP_LAYOUT))
    {
      auto begin = m_raw + header->m_sectionOffset ;
      return std::make_pair(begin, begin + header->m_sectionSize) ;
    }
    //throw error("axlf contains no mem topology data section") ;
    return std::make_pair(nullptr,nullptr);
  }

  data_range
  clk_freq_data() const
  {
    if (auto header = xclbin::get_axlf_section(m_axlf, axlf_section_kind::CLOCK_FREQ_TOPOLOGY))
    {
      auto begin = m_raw + header->m_sectionOffset ;
      return std::make_pair(begin, begin + header->m_sectionSize) ;
    }
    //throw error("axlf contains no clk_freq_data section") ;
    return std::make_pair(nullptr,nullptr);
  }

};

// exposed to binary.cpp
std::unique_ptr<binary::impl>
create_xclbin2(std::vector<char>&& xb)
{
  // Before moving do sanity checks for proper xclbin2
  if (xb.size() < sizeof(axlf))
    throw error("bad axlf file");

  auto xb2 = reinterpret_cast<const axlf*>(&xb[0]);
  auto hdr = &xb2->m_header;
  if (xb.size() < hdr->m_length)
    throw error ("axlf length mismatch");

  // Ok we are probably good, any throws now breaks
  // strong exception safety guarantee as xb is being
  // moved
  return std::make_unique<xclbin2>(std::move(xb));
}


}
