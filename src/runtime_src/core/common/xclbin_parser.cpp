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
#define XRT_CORE_COMMON_SOURCE
#include "xclbin_parser.h"
#include "config_reader.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/optional.hpp>
#include <cstring>
#include <cstdlib>
// This is xclbin parser. Update this file if xclbin format has changed.

#ifdef _WIN32
#pragma warning ( disable : 4996 )
#endif

namespace {

namespace pt = boost::property_tree;

static size_t
convert(const std::string& str)
{
  return str.empty() ? 0 : std::stoul(str,0,0);
}

static bool
is_sw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool swem = xem ? std::strcmp(xem,"sw_emu")==0 : false;
  return swem;
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
static size_t 
get_base_addr(const ip_data& ip)
{
  auto addr = ip.m_base_address;
  if (addr == static_cast<size_t>(-1))
    addr = std::numeric_limits<size_t>::max() & ~0xFF;
  return addr;
}

static int
kernel_max_ctx(const ip_data& ip)
{
  auto ctx = xrt_core::config::get_kernel_channel_info();
  if (ctx.empty())
    return 0;

  std::string knm = reinterpret_cast<const char*>(ip.m_name);
  knm = knm.substr(0,knm.find(":"));

  auto pos1 = ctx.find("{"+knm+":");
  if (pos1 == std::string::npos)
    return 0;

  auto pos2 = ctx.find("}",pos1);
  if (pos2 == std::string::npos || pos2 < pos1+knm.size()+2)
    return 0;

  auto ctxid_str = ctx.substr(pos1+knm.size()+2,pos2);
  auto ctxid = std::stoi(ctxid_str);

  if (ctxid < 0 || ctxid > 31)
    throw std::runtime_error("context id must be between 0 and 31");

  return ctxid;
}

}

namespace xrt_core { namespace xclbin {

std::string
memidx_to_name(const axlf* top,  int32_t midx)
{
  auto mem_topology = axlf_section_type<const ::mem_topology*>::get(top,axlf_section_kind::MEM_TOPOLOGY);
  if (!mem_topology)
    return std::to_string(midx);
  if (midx >= mem_topology->m_count)
    return std::to_string(midx);

  auto& md = mem_topology->m_mem_data[midx];
  return std::string(reinterpret_cast<const char*>(md.m_tag));
}

int32_t
get_first_used_mem(const axlf* top)
{
  auto mem_topology = axlf_section_type<const ::mem_topology*>::get(top,axlf_section_kind::MEM_TOPOLOGY);
  if (!mem_topology)
    return -1;

  for (int32_t i=0; i<mem_topology->m_count; ++i) {
    if (mem_topology->m_mem_data[i].m_used)
      return i;
  }

  return -1;
}

std::vector<uint64_t>
get_cus(const ip_layout* ip_layout, bool encode)
{
  std::vector<uint64_t> cus;
  
  for (int32_t count=0; count <ip_layout->m_count; ++count) {
    const auto& ip_data = ip_layout->m_ip_data[count];
    if (is_valid_cu(ip_data)) {
      uint64_t addr = get_base_addr(ip_data);
      if (encode) {
        // encode handshaking control in lower unused address bits [2-0]
        addr |= ((ip_data.properties & IP_CONTROL_MASK) >> IP_CONTROL_SHIFT);

        // encode max context in lower [7-3] bits of addr, assumes IP control
        // takes three bits only.  This is a hack for now.
        addr |= (kernel_max_ctx(ip_data) << 3);
      }
      cus.push_back(addr);
    }
  }
  std::sort(cus.begin(),cus.end());
  return cus;
}

std::vector<uint64_t>
get_cus(const axlf* top, bool encode)
{
  auto ip_layout = axlf_section_type<const ::ip_layout*>::get(top,axlf_section_kind::IP_LAYOUT);
  return ip_layout ? get_cus(ip_layout,encode) : std::vector<uint64_t>(0);
}

std::vector<std::pair<uint64_t, size_t>>
get_debug_ips(const axlf* top)
{
  std::vector<std::pair<uint64_t, size_t>> ips;
  auto debug_ip_layout = axlf_section_type<const ::debug_ip_layout*>::
    get(top,axlf_section_kind::DEBUG_IP_LAYOUT);
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

uint32_t
get_cu_control(const axlf* top, uint64_t cuaddr)
{
  if (is_sw_emulation())
    return AP_CTRL_HS;

  auto ip_layout = axlf_section_type<const ::ip_layout*>::get(top,axlf_section_kind::IP_LAYOUT);
  if (!ip_layout)
    throw std::runtime_error("No such CU at address: " + std::to_string(cuaddr));
  for (int32_t count=0; count <ip_layout->m_count; ++count) {
    const auto& ip_data = ip_layout->m_ip_data[count];
    size_t ip_base_addr  = (ip_data.m_base_address == static_cast<size_t>(-1)) ? 
      std::numeric_limits<size_t>::max() : ip_data.m_base_address;
    if (ip_base_addr == cuaddr)
      return ((ip_data.properties & IP_CONTROL_MASK) >> IP_CONTROL_SHIFT);
  }
  throw std::runtime_error("No such CU at address: " + std::to_string(cuaddr));
}

uint64_t
get_cu_base_offset(const axlf* top)
{
  std::vector<uint64_t> cus;
  auto ip_layout = axlf_section_type<const ::ip_layout*>::get(top,axlf_section_kind::IP_LAYOUT);
  if (!ip_layout)
    return 0;

  size_t base = std::numeric_limits<uint32_t>::max();
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
  auto ip_layout = axlf_section_type<const ::ip_layout*>::get(top,axlf_section_kind::IP_LAYOUT);
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
  auto ip_layout = axlf_section_type<const ::ip_layout*>::get(top,axlf_section_kind::IP_LAYOUT);
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

std::vector<std::pair<uint64_t, size_t>>
get_cus_pair(const axlf* top)
{
  std::vector<uint64_t> cus;
  std::vector<std::pair<uint64_t, size_t>> ret;
  cus = get_cus(top, false);

  for (auto it = cus.begin(); it != cus.end(); ++it)
    // CU size is 64KB
    ret.push_back(std::make_pair(*it, 0x10000));

  return ret;
}

std::vector<std::pair<uint64_t, size_t>>
get_dbg_ips_pair(const axlf* top)
{
  return get_debug_ips(top);
}

std::vector<softkernel_object>
get_softkernels(const axlf* top)
{
  std::vector<softkernel_object> sks;
  const axlf_section_header *pSection;

  for (pSection = ::xclbin::get_axlf_section(top, SOFT_KERNEL);
    pSection != nullptr;
    pSection = ::xclbin::get_axlf_section_next(top, pSection, SOFT_KERNEL)) {
      auto begin = reinterpret_cast<const char*>(top) + pSection->m_sectionOffset;
      auto soft = reinterpret_cast<const soft_kernel*>(begin);

      softkernel_object sko;
      sko.ninst = soft->m_num_instances;
      sko.symbol_name = const_cast<char*>(begin + soft->mpo_symbol_name);
      sko.size = soft->m_image_size;
      sko.sk_buf = const_cast<char*>(begin + soft->m_image_offset);

      sks.push_back(sko);
  }

  return sks;
}

size_t
get_kernel_freq(const axlf* top)
{
  size_t kernel_clk_freq = 100; //default clock frequency is 100
  const axlf_section_header *xml_hdr = ::xclbin::get_axlf_section(top, EMBEDDED_METADATA);
  if (xml_hdr) {
    auto begin = reinterpret_cast<const char*>(top) + xml_hdr->m_sectionOffset;
    const char *xml_data = reinterpret_cast<const char*>(begin);
    uint64_t xml_size = xml_hdr->m_sectionSize;

    pt::ptree xml_project;
    std::stringstream xml_stream;
    xml_stream.write(xml_data,xml_size);
    pt::read_xml(xml_stream,xml_project);

    auto clock_child = xml_project.get_child_optional("project.platform.device.core.kernelClocks"); 

    if (clock_child) { // check whether kernelClocks field exists or not
      for (auto& xml_clock : xml_project.get_child("project.platform.device.core.kernelClocks")) {
        if (xml_clock.first != "clock")
          continue;
        auto port = xml_clock.second.get<std::string>("<xmlattr>.port","");
        auto freq = convert(xml_clock.second.get<std::string>("<xmlattr>.frequency","100"));
        if(port == "KERNEL_CLK")
          kernel_clk_freq = freq;
      }
    }
  }
  return kernel_clk_freq;
}

} // namespace xclbin
} // namespace xrt_core
