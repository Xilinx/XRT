/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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

#include <algorithm>
#include <regex>
#include <cstring>
#include <cstdlib>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/optional.hpp>
#include <boost/algorithm/string.hpp>

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

static std::pair<const char*, size_t>
get_xml_section(const axlf* top)
{
  const axlf_section_header* xml_hdr = ::xclbin::get_axlf_section(top, EMBEDDED_METADATA);

  if (!xml_hdr)
    throw std::runtime_error("No xml meta data in xclbin");

  auto begin = reinterpret_cast<const char*>(top) + xml_hdr->m_sectionOffset;
  auto xml_data = reinterpret_cast<const char*>(begin);
  auto xml_size = xml_hdr->m_sectionSize;
  return std::make_pair(xml_data, xml_size);
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

static bool
is_legacy_cu_intr(const ip_layout *ips)
{
  int32_t num_cus = ips->m_count;
  int cu_cnt = 0;
  int intr_cnt = 0;

  for (int i = 0; i < num_cus; i++) {
    const auto& ip = ips->m_ip_data[i];
    if (!is_valid_cu(ip))
      continue;

    cu_cnt++;
    if ((ip.properties & IP_INTERRUPT_ID_MASK) == 0)
      intr_cnt++;
  }

  return (cu_cnt == intr_cnt);
}

bool compare_intr_id(struct ip_data &l, struct ip_data &r)
{
    /* We need to put free running CU at the end */
    if (l.m_base_address == static_cast<size_t>(-1))
        return false;
    if (r.m_base_address == static_cast<size_t>(-1))
        return true;

    uint32_t l_id = l.properties & IP_INTERRUPT_ID_MASK;
    uint32_t r_id = r.properties & IP_INTERRUPT_ID_MASK;

    return l_id < r_id;
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


} // namespace

namespace xrt_core { namespace xclbin {

std::string
memidx_to_name(const mem_topology* mem_topology,  int32_t midx)
{
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

// Compute max register map size of CUs in xclbin
size_t
get_max_cu_size(const char* xml_data, size_t xml_size)
{
  pt::ptree xml_project;
  std::stringstream xml_stream;
  xml_stream.write(xml_data,xml_size);
  pt::read_xml(xml_stream,xml_project);

  size_t maxsz = 0;

  for (auto& xml_kernel : xml_project.get_child("project.platform.device.core")) {
    if (xml_kernel.first != "kernel")
      continue;

    for (auto& xml_arg : xml_kernel.second) {
      if (xml_arg.first != "arg")
        continue;

      auto ofs = convert(xml_arg.second.get<std::string>("<xmlattr>.offset"));
      auto sz = convert(xml_arg.second.get<std::string>("<xmlattr>.size"));
      maxsz = std::max(maxsz, ofs + sz);
    }
  }
  return maxsz;
}

std::vector<uint64_t>
get_cus(const ip_layout* ip_layout, bool encode)
{
  std::vector<uint64_t> cus;
  std::vector<struct ip_data> ips;

  for (int32_t count=0; count <ip_layout->m_count; ++count) {
    const auto& ip_data = ip_layout->m_ip_data[count];
    if (is_valid_cu(ip_data)) {
      ips.push_back(ip_data);
    }
  }

  if (!is_legacy_cu_intr(ip_layout)) {
      std::sort(ips.begin(), ips.end(), compare_intr_id);
  }

  for (auto &ip_data : ips) {
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

  if (is_legacy_cu_intr(ip_layout)) {
      std::sort(cus.begin(),cus.end());
  }
  return cus;
}

std::vector<const ip_data*>
get_cus(const ip_layout* ip_layout, const std::string& kname)
{
  // "kernel:{cu1,cu2,cu3}" -> "(kernel):((cu1)|(cu2)|(cu3))"
  // "kernel" -> "(kernel):((.*))"
  auto create_regex = [](const auto& str) {
    std::regex r("^(.*):\\{(.*)\\}$");
    std::smatch m;
    if (!regex_search(str,m,r))
      return "^(" + str + ")((.*))$";            // "(kernel):((.*))"

    std::string kernel = m[1];
    std::string insts = m[2];                  // "cu1,cu2,cu3"
    std::string regex = "^(" + kernel + "):(";  // "(kernel):("
    std::vector<std::string> cus;              // split at ','
    boost::split(cus,insts,boost::is_any_of(","));

    // compose final regex
    int count = 0;
    for (auto& cu : cus)
      regex.append("|", count++ ? 1 : 0).append("(").append(cu).append(")");
    regex += ")$";  // "^(kernel):((cu1)|(cu2)|(cu3))$"
    return regex;
  };

  std::regex r(create_regex(kname));
  std::vector<const ip_data*> ips;
  for (int32_t count = 0; count < ip_layout->m_count; ++count) {
    const auto& ip = ip_layout->m_ip_data[count];
    if (!is_valid_cu(ip))
      continue;
    std::string ipname = reinterpret_cast<const char*>(ip.m_name);
    if (regex_match(ipname, r))
      ips.push_back(&ip);
  }

  return ips;
}

// Extract CU base addresses for xml meta data
// Used in sw_emu because IP_LAYOUT section is not available in sw emu.
std::vector<uint64_t>
get_cus(const char* xml_data, size_t xml_size, bool)
{
  std::vector<uint64_t> cus;

  pt::ptree xml_project;
  std::stringstream xml_stream;
  xml_stream.write(xml_data, xml_size);
  pt::read_xml(xml_stream, xml_project);

  for (auto& xml_kernel : xml_project.get_child("project.platform.device.core")) {
    if (xml_kernel.first != "kernel")
      continue;
    for (auto& xml_inst : xml_kernel.second) {
      if (xml_inst.first != "instance")
        continue;
      for (auto& xml_remap : xml_inst.second) {
        if (xml_remap.first != "addrRemap")
          continue;
        auto base = convert(xml_remap.second.get<std::string>("<xmlattr>.base"));
        cus.push_back(base);
      }
    }
  }

  std::sort(cus.begin(), cus.end());
  return cus;
}

std::vector<uint64_t>
get_cus(const axlf* top, bool encode)
{
  if (is_sw_emulation()) {
    auto xml = get_xml_section(top);
    return get_cus(xml.first, xml.second);
  }

  auto ip_layout = axlf_section_type<const ::ip_layout*>::get(top,axlf_section_kind::IP_LAYOUT);
  return ip_layout ? get_cus(ip_layout,encode) : std::vector<uint64_t>(0);
}

std::vector<const ip_data*>
get_cus(const axlf* top, const std::string& kname)
{
  auto ip_layout = axlf_section_type<const ::ip_layout*>::get(top,axlf_section_kind::IP_LAYOUT);
  return ip_layout ? get_cus(ip_layout, kname) : std::vector<const ip_data*>(0);
}

std::string
get_ip_name(const ip_layout* ip_layout, uint64_t addr)
{
  auto end = ip_layout->m_ip_data + ip_layout->m_count;
  auto it = std::find_if(ip_layout->m_ip_data, end,
                         [addr] (const auto& ip_data) {
                           return ip_data.m_base_address == addr;
                         });

  if (it != end)
    return reinterpret_cast<const char*>((*it).m_name);

  throw std::runtime_error("No IP with base address " + std::to_string(addr));
}

std::string
get_ip_name(const axlf* top, uint64_t addr)
{
  if (auto ip_layout = axlf_section_type<const ::ip_layout*>::get(top,axlf_section_kind::IP_LAYOUT))
    return get_ip_name(ip_layout, addr);

  throw std::runtime_error("No IP layout in xclbin");
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
get_cu_control(const ip_layout* ip_layout, uint64_t cuaddr)
{
  if (!ip_layout && is_sw_emulation())
    return AP_CTRL_HS;

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
get_cu_base_offset(const ip_layout* ip_layout)
{
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

uint64_t
get_cu_base_offset(const axlf* top)
{
  auto ip_layout = axlf_section_type<const ::ip_layout*>::get(top,axlf_section_kind::IP_LAYOUT);
  return get_cu_base_offset(ip_layout);
}

bool
get_cuisr(const ip_layout* ip_layout)
{
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
get_cuisr(const axlf* top)
{
  auto ip_layout = axlf_section_type<const ::ip_layout*>::get(top,axlf_section_kind::IP_LAYOUT);
  return get_cuisr(ip_layout);
}

bool
get_dataflow(const ip_layout* ip_layout)
{
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

bool
get_dataflow(const axlf* top)
{
  auto ip_layout = axlf_section_type<const ::ip_layout*>::get(top,axlf_section_kind::IP_LAYOUT);
  return get_dataflow(ip_layout);
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
      sko.symbol_name = std::string(begin + soft->mpo_symbol_name);
      sko.mpo_name = std::string(begin + soft->mpo_name);
      sko.mpo_version = std::string(begin + soft->mpo_version);
      sko.size = soft->m_image_size;
      sko.sk_buf = const_cast<char*>(begin + soft->m_image_offset);
      sks.emplace_back(std::move(sko));
  }

  return sks;
}

size_t
get_kernel_freq(const axlf* top)
{
  size_t kernel_clk_freq = 100; //default clock frequency is 100
  auto xml = get_xml_section(top);

  pt::ptree xml_project;
  std::stringstream xml_stream;
  xml_stream.write(xml.first,xml.second);
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

  return kernel_clk_freq;
}

std::vector<kernel_argument>
get_kernel_arguments(const char* xml_data, size_t xml_size, const std::string& kname)
{
  std::vector<kernel_argument> args;

  pt::ptree xml_project;
  std::stringstream xml_stream;
  xml_stream.write(xml_data,xml_size);
  pt::read_xml(xml_stream,xml_project);

  for (auto& xml_kernel : xml_project.get_child("project.platform.device.core")) {
    if (xml_kernel.first != "kernel")
      continue;
    if (xml_kernel.second.get<std::string>("<xmlattr>.name") != kname)
      continue;

    for (auto& xml_arg : xml_kernel.second) {
      if (xml_arg.first != "arg")
        continue;

      std::string id = xml_arg.second.get<std::string>("<xmlattr>.id");
      size_t index = id.empty() ? kernel_argument::no_index : convert(id);

      args.emplace_back(kernel_argument{
          xml_arg.second.get<std::string>("<xmlattr>.name")
         ,xml_arg.second.get<std::string>("<xmlattr>.type", "no-type")
         ,index
         ,convert(xml_arg.second.get<std::string>("<xmlattr>.offset"))
         ,convert(xml_arg.second.get<std::string>("<xmlattr>.size"))
         ,kernel_argument::argtype(xml_arg.second.get<size_t>("<xmlattr>.addressQualifier"))
      });
    }

    std::sort(args.begin(), args.end(), [](auto& a1, auto& a2) { return a1.index < a2.index; });
    break;
  }
  return args;
}

std::vector<std::string>
get_kernel_names(const char *xml_data, size_t xml_size)
{
  std::vector<std::string> names;

  pt::ptree xml_project;
  std::stringstream xml_stream;
  xml_stream.write(xml_data,xml_size);
  pt::read_xml(xml_stream,xml_project);

  for (auto& xml_kernel : xml_project.get_child("project.platform.device.core")) {
    if (xml_kernel.first != "kernel")
      continue;

    names.push_back(xml_kernel.second.get<std::string>("<xmlattr>.name"));
  }

  return names;
}

std::vector<kernel_argument>
get_kernel_arguments(const axlf* top, const std::string& kname)
{
  auto xml = get_xml_section(top);
  return get_kernel_arguments(xml.first, xml.second, kname);
}

std::vector<kernel_object>
get_kernels(const axlf* top)
{
  auto xml = get_xml_section(top);
  std::vector<kernel_object> kernels;

  auto knames = get_kernel_names(xml.first, xml.second);
  for (auto& kname : knames) {
    kernels.emplace_back(kernel_object{
       kname
      ,get_kernel_arguments(xml.first, xml.second, kname)
    });
  }

  return kernels;
}

// PDI only XCLBIN has PDI section only;
// Or has AIE_METADATA and PDI sections only
bool
is_pdi_only(const axlf* top)
{
  auto pdi = axlf_section_type<const char*>::get(top, axlf_section_kind::PDI);
  auto aie_meta = axlf_section_type<const char*>::get(top, axlf_section_kind::AIE_METADATA);

  return ((top->m_header.m_numSections == 1 && pdi != nullptr) || (top->m_header.m_numSections == 2 && pdi != nullptr && aie_meta != nullptr));
}

}} // xclbin, xrt_core
