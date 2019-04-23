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

#include "xclbin_parser.h"

// This is xclbin parser. Update this file if xclbin format has changed.

namespace {

template <typename SectionType>
static SectionType*
get_axlf_section(const axlf* top, axlf_section_kind kind)
{
  if (auto header = ::xclbin::get_axlf_section(top, kind)) {
    auto begin = reinterpret_cast<const char*>(top) + header->m_sectionOffset ;
    return reinterpret_cast<SectionType*>(begin);
  }
  return nullptr;
}

// Filter out IPs with invalid base address (streaming kernel)
static bool
is_valid_cu(const ip_data& ip)
{
  if (ip.m_type != IP_TYPE::IP_KERNEL)
    return false;

  // Filter IP KERNELS if necessary
  // ...

  return true;
}

// Base address of unused (streaming) CUs is given a max address to
// ensure that they are sorted to come after regular AXI-lite CUs
// The sort order is important as it determines the CU indices used
// throughout XRT.
static uint64_t
get_base_addr(const ip_data& ip)
{
  auto addr = ip.m_base_address;
  if (addr == static_cast<uint64_t>(-1))
    addr = std::numeric_limits<uint64_t>::max() & ~0xFF;
  return addr;
}

}

namespace xrt_core { namespace xclbin {

std::vector<uint64_t>
get_cus(const axlf* top, bool encoding)
{
  std::vector<uint64_t> cus;
  auto ip_layout = get_axlf_section<const ::ip_layout>(top,axlf_section_kind::IP_LAYOUT);
  if (!ip_layout)
   return cus;

  for (int32_t count=0; count <ip_layout->m_count; ++count) {
    const auto& ip_data = ip_layout->m_ip_data[count];
    if (is_valid_cu(ip_data)) {
      uint64_t addr = get_base_addr(ip_data);
      if (encoding)
          // encode handshaking control in lower unused address bits
          addr |= ((ip_data.properties & IP_CONTROL_MASK) >> IP_CONTROL_SHIFT);
      cus.push_back(addr);
    }
  }
  std::sort(cus.begin(),cus.end());
  return cus;
}

std::vector<std::pair<uint64_t, size_t> >
get_debug_ips(const axlf* top)
{
  std::vector<std::pair<uint64_t, size_t> > ips;
  auto debug_ip_layout = get_axlf_section<const ::debug_ip_layout>(top,
                         axlf_section_kind::DEBUG_IP_LAYOUT);
  if (!debug_ip_layout)
    return ips;

  for (int32_t count=0; count < debug_ip_layout->m_count; ++count) {
    const auto& debug_ip_data = debug_ip_layout->m_debug_ip_data[count];
    uint64_t addr = debug_ip_data.m_base_address;
    // There is no size for each debug IP in the xclbin. Use hardcoding size now.
    // The default size is 64KB.
    size_t size = 0x10000;
    if (debug_ip_data.m_type == AXI_MONITOR_FIFO_LITE
        || debug_ip_data.m_type == AXI_MONITOR_FIFO_FULL)
       // The size of these two type of IPs is 8KB
       size = 0x2000;

    ips.push_back(std::make_pair(addr, size));
  }

  std::sort(ips.begin(), ips.end());
  return ips;
}

uint64_t
get_cu_base_offset(const axlf* top)
{
  std::vector<uint64_t> cus;
  auto ip_layout = get_axlf_section<const ::ip_layout>(top,axlf_section_kind::IP_LAYOUT);
  if (!ip_layout)
    return 0;

  uint64_t base = std::numeric_limits<uint32_t>::max();
  for (int32_t count=0; count <ip_layout->m_count; ++count) {
    const auto& ip_data = ip_layout->m_ip_data[count];
    if (is_valid_cu(ip_data))
      base = std::min(base,get_base_addr(ip_data));
  }
  return base;
}

bool
get_cuisr(const axlf* top)
{
  auto ip_layout = get_axlf_section<const ::ip_layout>(top,axlf_section_kind::IP_LAYOUT);
  if (!ip_layout)
    return false;

  for (int32_t count=0; count <ip_layout->m_count; ++count) {
    const auto& ip_data = ip_layout->m_ip_data[count];
    if (is_valid_cu(ip_data) && !(ip_data.properties & 0x1))
      return false;
  }
  return true;
}

bool
get_dataflow(const axlf* top)
{
  auto ip_layout = get_axlf_section<const ::ip_layout>(top,axlf_section_kind::IP_LAYOUT);
  if (!ip_layout)
    return false;

  for (int32_t count=0; count <ip_layout->m_count; ++count) {
    const auto& ip_data = ip_layout->m_ip_data[count];
    if (is_valid_cu(ip_data) &&
        ((ip_data.properties & IP_CONTROL_MASK) >> IP_CONTROL_SHIFT) == AP_CTRL_CHAIN)
        return true;
  }
  return false;
}

std::vector<std::pair<uint64_t, size_t> >
get_cus_pair(const axlf* top)
{
  std::vector<uint64_t> cus;
  std::vector<std::pair<uint64_t, size_t> > ret;
  cus = get_cus(top, false);

  for (auto it = cus.begin(); it != cus.end(); ++it)
    // CU size is 64KB
    ret.push_back(std::make_pair(*it, 0x10000));

  return ret;
}

std::vector<std::pair<uint64_t, size_t> >
get_dbg_ips_pair(const axlf* top)
{
  return get_debug_ips(top);
}

} // namespace xclbin
} // namespace xrt_core
