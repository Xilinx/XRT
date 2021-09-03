/*
 * Copyright (C) 2019-2021 Xilinx, Inc
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
#include "error.h"

#include <algorithm>
#include <map>
#include <regex>
#include <cstring>
#include <cstdlib>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/optional.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

// This is xclbin parser. Update this file if xclbin format has changed.
#ifdef _WIN32
#pragma warning ( disable : 4996 )
#endif

namespace {

namespace pt = boost::property_tree;

// NOLINTNEXTLINE
constexpr size_t operator"" _kb(unsigned long long v)  { return 1024u * v; }

static size_t
convert(const std::string& str)
{
  return str.empty() ? 0 : std::stoul(str,nullptr,0);
}

static bool
to_bool(const std::string& str)
{
  return str == "true" ? true : false;
}

static xrt_core::xclbin::kernel_properties::mailbox_type
convert_to_mailbox_type(const std::string& str)
{
  static std::map<std::string, xrt_core::xclbin::kernel_properties::mailbox_type> table = {
    { "none", xrt_core::xclbin::kernel_properties::mailbox_type::none },
    { "in", xrt_core::xclbin::kernel_properties::mailbox_type::in },
    { "out", xrt_core::xclbin::kernel_properties::mailbox_type::out },
    { "inout", xrt_core::xclbin::kernel_properties::mailbox_type::inout },
    { "both", xrt_core::xclbin::kernel_properties::mailbox_type::inout },
    { "true", xrt_core::xclbin::kernel_properties::mailbox_type::inout },
    { "false", xrt_core::xclbin::kernel_properties::mailbox_type::none },
  };
  auto itr = table.find(str);
  if (itr == table.end())
    throw xrt_core::error("Invalid mailbox property '" + str + "'");
  return (*itr).second;
}


// Kernel mailbox
// Needed until meta-data support (Vitis-1147)
// Format is "[/kernel_name/]*"
// mailbox="/kernel1_name/kernel2_name/"
static xrt_core::xclbin::kernel_properties::mailbox_type
get_mailbox_from_ini(const std::string& kname)
{
  static auto mailbox_kernels = xrt_core::config::get_mailbox_kernels();
  return (mailbox_kernels.find("/" + kname + "/") != std::string::npos)
    ? xrt_core::xclbin::kernel_properties::mailbox_type::inout
    : xrt_core::xclbin::kernel_properties::mailbox_type::none;
}

// Kernel auto restart counter offset
// Needed until meta-data support (Vitis-1147)
static xrt_core::xclbin::kernel_properties::restart_type
get_restart_from_ini(const std::string& kname)
{
  static auto restart_kernels = xrt_core::config::get_auto_restart_kernels();
  return (restart_kernels.find("/" + kname + "/") != std::string::npos)
    ? 1
    : 0;
}

// Kernel software reset
static bool
get_sw_reset_from_ini(const std::string& kname)
{
  static auto reset_kernels = xrt_core::config::get_sw_reset_kernels();
  return (reset_kernels.find("/" + kname + "/") != std::string::npos);
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

bool
compare_intr_id(struct ip_data &l, struct ip_data &r)
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
    addr = std::numeric_limits<size_t>::max() & ~0xFF; // NOLINT
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

  if (ctxid < 0 || ctxid > 31) // NOLINT
    throw std::runtime_error("context id must be between 0 and 31");

  return ctxid;
}

// Determine the address range from kernel xml entry
static size_t
get_address_range(const pt::ptree& xml_kernel)
{
  constexpr auto default_address_range = 64_kb;
  size_t address_range = default_address_range;
  for (auto& xml_port : xml_kernel) {
    if (xml_port.first != "port")
      continue;

    // one AXI slave port per kernel
    if (xml_port.second.get<std::string>("<xmlattr>.mode") == "slave") {
      address_range = convert(xml_port.second.get<std::string>("<xmlattr>.range"));
      break;
    }
  }
  return address_range;
}

} // namespace

namespace xrt_core { namespace xclbin {

const axlf_section_header*
get_axlf_section(const axlf* top, axlf_section_kind kind)
{
  // replace group kinds with none group kinds if grouping
  // is disabled per xrt.ini
  static bool use_groups = xrt_core::config::get_use_xclbin_group_sections();
  if (kind == ASK_GROUP_TOPOLOGY && !use_groups)
    kind = MEM_TOPOLOGY;
  else if (kind == ASK_GROUP_CONNECTIVITY && !use_groups)
    kind = CONNECTIVITY;

  if (auto hdr = ::xclbin::get_axlf_section(top, kind))
    return hdr;

  // hdr is nullptr, check if kind is one of the group sections,
  // which then does not appear in the xclbin and should default to
  // the none group one.
  if (kind == ASK_GROUP_TOPOLOGY)
    return ::xclbin::get_axlf_section(top, MEM_TOPOLOGY);
  else if (kind == ASK_GROUP_CONNECTIVITY)
    return ::xclbin::get_axlf_section(top, CONNECTIVITY);

  return nullptr;
}

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
address_to_memidx(const mem_topology* mem_topology, uint64_t address)
{
  if (is_sw_emulation())
    return 0;  // default bank in software emulation
  
  // Reserve look for preferred group id
  for (int idx = mem_topology->m_count-1; idx >= 0; --idx) {
    auto& mem = mem_topology->m_mem_data[idx];
    if (!mem.m_used)
      continue;
    if (mem.m_type == MEM_STREAMING)
      continue;
    if (mem.m_type == MEM_STREAMING_CONNECTION)
      continue;
    if (address < mem.m_base_address)
      continue;
    if (address > (mem.m_base_address + mem.m_size * 1024))  // NOLINT
      continue;
    return idx;
  }
  return std::numeric_limits<int32_t>::max();
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

    // determine address range to ensure args are within
    size_t address_range = get_address_range(xml_kernel.second);

    // iterate arguments and find offset and size to compute max
    for (auto& xml_arg : xml_kernel.second) {
      if (xml_arg.first != "arg")
        continue;

      auto ofs = convert(xml_arg.second.get<std::string>("<xmlattr>.offset"));
      auto sz = convert(xml_arg.second.get<std::string>("<xmlattr>.size"));

      // Validate offset and size against address range
      if (ofs + sz > address_range) {
        auto knm = xml_kernel.second.get<std::string>("<xmlattr>.name");
        auto argnm = xml_arg.second.get<std::string>("<xmlattr>.name");
        auto fmt = boost::format
          ("Invalid kernel offset in xclbin for kernel (%s) argument (%s).\n"
           "The offset (0x%x) and size (0x%x) exceeds kernel address range (0x%x)")
          % knm % argnm % ofs % sz % address_range;
        throw xrt_core::error(fmt.str());
      }
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
      return "^(" + str + "):((.*))$";            // "(kernel):((.*))"

    std::string kernel = m[1];
    std::string insts = m[2];                     // "cu1,cu2,cu3"
    std::string regex = "^(" + kernel + "):(";    // "(kernel):("
    std::vector<std::string> cus;                 // split at ','
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
    size_t size = 0x10000; // NOLINT
    if (debug_ip_data.m_type == AXI_MONITOR_FIFO_LITE
        || debug_ip_data.m_type == AXI_MONITOR_FIFO_FULL)
       // The size of these two type of IPs is 8KB
       size = 0x2000;  // NOLINT

    ips.emplace_back(std::make_pair(addr, size));
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

  constexpr size_t cu_size = 0x10000; // CU size is 64KB
  for (auto cu : cus)
    ret.emplace_back(std::make_pair(cu, cu_size));

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
  const axlf_section_header *pSection = nullptr;

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
      sko.sk_buf = const_cast<char*>(begin + soft->m_image_offset);  // NOLINT
      sks.emplace_back(std::move(sko));
  }

  return sks;
}

size_t
get_kernel_freq(const axlf* top)
{
  constexpr size_t default_kernel_clk_freq = 100;
  size_t kernel_clk_freq = default_kernel_clk_freq;
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
      auto freq = xml_clock.second.get<std::string>("<xmlattr>.frequency","100");
      //clock is always represented in units in XML
      auto units = "MHz";
      size_t found = freq.find(units);

      //remove the units from the string
      if (found != std::string::npos)
        freq = freq.substr(0,found);

      if(!freq.empty() && port == "KERNEL_CLK")
        kernel_clk_freq = convert(freq);
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
         ,xml_arg.second.get<std::string>("<xmlattr>.port", "no-port")
         ,index
         ,convert(xml_arg.second.get<std::string>("<xmlattr>.offset"))
         ,convert(xml_arg.second.get<std::string>("<xmlattr>.size"))
         ,convert(xml_arg.second.get<std::string>("<xmlattr>.hostSize"))
         ,0  // fa_desc_offset post computed if necessary
         ,kernel_argument::argtype(xml_arg.second.get<size_t>("<xmlattr>.addressQualifier"))
         ,kernel_argument::direction(kernel_argument::direction::input)
      });
    }

    // stable sort to preserve order of multi-component arguments
    // for example global_size, local_size, etc.
    std::stable_sort(args.begin(), args.end(), [](auto& a1, auto& a2) { return a1.index < a2.index; });
    break;
  }
  return args;
}

std::vector<kernel_argument>
get_kernel_arguments(const axlf* top, const std::string& kname)
{
  auto xml = get_xml_section(top);
  return get_kernel_arguments(xml.first, xml.second, kname);
}

kernel_properties
get_kernel_properties(const char* xml_data, size_t xml_size, const std::string& kname)
{
  pt::ptree xml_project;
  std::stringstream xml_stream;
  xml_stream.write(xml_data,xml_size);
  pt::read_xml(xml_stream,xml_project);

  for (auto& xml_kernel : xml_project.get_child("project.platform.device.core")) {
    if (xml_kernel.first != "kernel")
      continue;
    if (xml_kernel.second.get<std::string>("<xmlattr>.name") != kname)
      continue;

    // kernel address range
    size_t address_range = get_address_range(xml_kernel.second);

    // Determine features
    auto mailbox = convert_to_mailbox_type(xml_kernel.second.get<std::string>("<xmlattr>.mailbox","none"));
    if (mailbox == kernel_properties::mailbox_type::none)
      mailbox = get_mailbox_from_ini(kname);
    auto restart = convert(xml_kernel.second.get<std::string>("<xmlattr>.countedAutoRestart","0"));
    if (restart == 0)
      restart = get_restart_from_ini(kname);
    auto sw_reset = to_bool(xml_kernel.second.get<std::string>("<xmlattr>.swReset","false"));
    if (!sw_reset)
      sw_reset = get_sw_reset_from_ini(kname);

    return kernel_properties{kname, restart, mailbox, address_range, sw_reset};
  }

  return kernel_properties{};
}
    
kernel_properties
get_kernel_properties(const axlf* top, const std::string& kname)
{
  auto xml = get_xml_section(top);
  return get_kernel_properties(xml.first, xml.second, kname);
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

std::vector<kernel_object>
get_kernels(const char* xml_data, size_t xml_size)
{
  std::vector<kernel_object> kernels;

  auto knames = get_kernel_names(xml_data, xml_size);
  for (auto& kname : get_kernel_names(xml_data, xml_size)) {
    auto kprop = get_kernel_properties(xml_data, xml_size, kname);
    kernels.emplace_back(kernel_object{
        kname
       ,get_kernel_arguments(xml_data, xml_size, kname)
       ,kprop.address_range
       ,kprop.sw_reset
    });
  }

  return kernels;
}

std::vector<kernel_object>
get_kernels(const axlf* top)
{
  auto xml = get_xml_section(top);
  return get_kernels(xml.first, xml.second);
}

// PDI only XCLBIN has PDI section only;
// Or has AIE_METADATA and PDI sections only
bool
is_pdi_only(const axlf* top)
{
  auto pdi = axlf_section_type<const char*>::get(top, axlf_section_kind::PDI);
  auto aie_meta = axlf_section_type<const char*>::get(top, axlf_section_kind::AIE_METADATA);
  auto aie_res = axlf_section_type<const char*>::get(top, axlf_section_kind::AIE_RESOURCES);

  return ((top->m_header.m_numSections == 1 && pdi != nullptr)
          || (top->m_header.m_numSections == 2 && pdi != nullptr && aie_meta != nullptr)
          || (top->m_header.m_numSections == 3 && pdi != nullptr && aie_meta != nullptr && aie_res != nullptr));
}

std::string
get_vbnv(const axlf* top)
{
  constexpr size_t vbnv_length = 64;
  auto vbnv = reinterpret_cast<const char*>(top->m_header.m_platformVBNV);
  return {vbnv, strnlen(vbnv, vbnv_length)};
}

}} // xclbin, xrt_core
