/**
 * Copyright (C) 2020-2021 Xilinx, Inc
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
#include "xclbin_swemu.h"

#include "debug.h"
#include "xclbin_parser.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <map>
#include <string>
#include <stdexcept>

#ifdef _WIN32
#pragma warning ( disable : 4996 4267)
#endif

namespace {

namespace pt = boost::property_tree;

static size_t
convert(const std::string& str)
{
  return str.empty() ? 0 : std::stoul(str,0,0);
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

static std::vector<std::pair<uint64_t, std::string>>
get_cu_addr_name(const char* xml_data, size_t xml_size)
{
  std::vector<std::pair<uint64_t, std::string>> cus;

  pt::ptree xml_project;
  std::stringstream xml_stream;
  xml_stream.write(xml_data, xml_size);
  pt::read_xml(xml_stream, xml_project);

  for (auto& xml_kernel : xml_project.get_child("project.platform.device.core")) {
    if (xml_kernel.first != "kernel")
      continue;
    auto kname = xml_kernel.second.get<std::string>("<xmlattr>.name");
    for (auto& xml_inst : xml_kernel.second) {
      if (xml_inst.first != "instance")
        continue;
      auto iname = xml_inst.second.get<std::string>("<xmlattr>.name");
      for (auto& xml_remap : xml_inst.second) {
        if (xml_remap.first != "addrRemap")
          continue;
        auto base = convert(xml_remap.second.get<std::string>("<xmlattr>.base"));
        auto name = kname + ":" + iname;
        cus.emplace_back(std::make_pair(base, std::move(name)));
      }
    }
  }

  return cus;
}

static std::string
kernel_name(const std::string& cuname)
{
  return cuname.substr(0,cuname.find(":"));
}

static std::string
kernel_name(const uint8_t* cuname)
{
  return kernel_name(reinterpret_cast<const char*>(cuname));
}

static std::vector<char>
get_ip_layout(const axlf* top)
{
  auto xml = get_xml_section(top);
  auto cus = get_cu_addr_name(xml.first, xml.second);

  auto sz = cus.size() * sizeof(ip_data) + sizeof(ip_layout) - sizeof(ip_data);
  std::vector<char> vec(sz);
  auto ip_layout = reinterpret_cast<::ip_layout*>(vec.data());
  ip_layout->m_count = cus.size();

  for (int32_t count=0; count <ip_layout->m_count; ++count) {
    const auto& cu = cus[count];
    auto& ip_data = ip_layout->m_ip_data[count];
    ip_data.m_type = IP_KERNEL;
    ip_data.properties = 0;
    ip_data.m_base_address = cu.first;
    auto bytes = std::min<size_t>(cu.second.size(),63);
    std::memcpy(ip_data.m_name, cu.second.c_str(), bytes);
    ip_data.m_name[bytes] = 0;
  }

  return vec;
}

static std::vector<char>
get_connectivity(const axlf* top, const ip_layout* ip_layout)
{
  // for each kernel, lookup arguments
  std::map<std::string, std::vector<xrt_core::xclbin::kernel_argument>> k2args;
  for (auto& k : xrt_core::xclbin::get_kernels(top)) {
    auto& args = k.args;
    // remove non-indexed args
    args.erase(std::remove_if(args.begin(), args.end(), [] (auto& arg) {
          return arg.index == xrt_core::xclbin::kernel_argument::no_index;
        }),args.end());
    k2args.emplace(std::make_pair(std::move(k.name), std::move(args)));
  }

  // compute size of connectivity section
  size_t sz = sizeof(connectivity) - sizeof(connection);
  int32_t connectivity_count = 0;
  for (int32_t ipidx = 0; ipidx < ip_layout->m_count; ++ipidx) {
    const auto& ip_data = ip_layout->m_ip_data[ipidx];
    auto kname = kernel_name(ip_data.m_name);
    auto connections = k2args[kname].size();
    sz += sizeof(connection) * connections;
    connectivity_count += connections;
  }

  // create and populate
  std::vector<char> vec(sz);
  auto connectivity = reinterpret_cast<::connectivity*>(vec.data());
  connectivity->m_count = connectivity_count;

  int32_t connectivity_idx = 0;
  for (int32_t ipidx=0; ipidx < ip_layout->m_count; ++ipidx) {
    const auto& ip_data = ip_layout->m_ip_data[ipidx];
    for (const auto& arg : k2args[kernel_name(ip_data.m_name)]) {
      XRT_ASSERT(connectivity_idx < connectivity_count, "connectivity index mismatch");
      auto& con = connectivity->m_connection[connectivity_idx++];
      con.arg_index = arg.index;
      con.m_ip_layout_index = ipidx;
      con.mem_data_index = 0;
    }
  }

  return vec;
}

static std::vector<char>
get_mem_topology(const axlf*)
{
  std::vector<char> vec (sizeof(mem_topology));
  auto mem_topology = reinterpret_cast<::mem_topology*>(vec.data());
  mem_topology->m_count = 1;
  auto& mem_data = mem_topology->m_mem_data[0];
  mem_data.m_type = MEM_DDR4;
  mem_data.m_used = 1;
  mem_data.m_size = 0x1000000;
  mem_data.m_base_address = 0x0;
  auto bytes = std::min<size_t>(strlen("bank0"), 16);
  std::memcpy(mem_data.m_tag, "bank0", bytes);
  mem_data.m_tag[bytes] = 0;
  return vec;
}

} // namespace

namespace xrt_core { namespace xclbin { namespace swemu {

std::vector<char>
get_axlf_section(const device* device, const axlf* top, axlf_section_kind kind)
{

  switch (kind) {
  case MEM_TOPOLOGY:
  case ASK_GROUP_TOPOLOGY:
    return get_mem_topology(top);
  case CONNECTIVITY:
  case ASK_GROUP_CONNECTIVITY: {
    auto ipl = device->get_axlf_section_or_error<const ::ip_layout*>(IP_LAYOUT);
    return get_connectivity(top, ipl);
  }
  case IP_LAYOUT:
    return get_ip_layout(top);
  default:
    return {};
  }
}

std::vector<char>
get_axlf_section(const axlf* top, const ::ip_layout* ip_layout, axlf_section_kind kind)
{

  switch (kind) {
  case MEM_TOPOLOGY:
  case ASK_GROUP_TOPOLOGY:
    return get_mem_topology(top);
  case CONNECTIVITY:
  case ASK_GROUP_CONNECTIVITY:
    return get_connectivity(top, ip_layout);
  case IP_LAYOUT:
    return get_ip_layout(top);
  default:
    return {};
  }
}

}}} // swemu, xclbin, xrt_core
