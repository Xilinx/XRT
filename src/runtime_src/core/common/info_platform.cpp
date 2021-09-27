/**
 * Copyright (C) 2021 Xilinx, Inc
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
#include "info_platform.h"
#include "query_requests.h"
#include "utils.h"

#include <boost/algorithm/string.hpp>

// Too much typing
using ptree_type = boost::property_tree::ptree;
namespace xq = xrt_core::query;

namespace {

void
add_static_region_info(const xrt_core::device* device, ptree_type& pt)
{
  ptree_type static_region;

  static_region.add("vbnv", xrt_core::device_query<xq::rom_vbnv>(device));

  std::vector<std::string> logic_uuids;
  try {
    logic_uuids = xrt_core::device_query<xq::logic_uuids>(device);
    logic_uuids.erase
      (std::remove_if(logic_uuids.begin(), logic_uuids.end(),
                      [](const std::string& s) {
                        return s.empty();
                      }), logic_uuids.end());
  }
  catch (const xq::exception&) {
  }
  
  if (!logic_uuids.empty())
    static_region.add("logic_uuid", xq::interface_uuids::to_uuid_upper_string(logic_uuids[0]));
  else 
    static_region.add("logic_uuid", (boost::format("0x%x") % xrt_core::device_query<xq::rom_time_since_epoch>(device)));

  try {
    static_region.add("jtag_idcode", xq::idcode::to_string(xrt_core::device_query<xq::idcode>(device)));
  }
  catch (const xq::no_such_key&) {
    static_region.add("jtag_idcode", "N/A");  // edge
  }

  try {
    static_region.add("fpga_name", xrt_core::device_query<xq::rom_fpga_name>(device));
  }
  catch (const xq::no_such_key&) {
    static_region.add("fpga_name", "N/A");   // edige
  }

  pt.put_child("static_region", static_region);
}

void
add_board_info(const xrt_core::device* device, ptree_type& pt)
{
  auto ddr_size_bytes = [](uint64_t size_gb, uint64_t count) {
                          constexpr uint64_t gb = 1024 * 1024 * 1024;
                          auto bytes = size_gb * gb;
                          return bytes * count;
  };

  ptree_type bd_info;
  try {
    bd_info.add("ddr_size_bytes",ddr_size_bytes
                (xrt_core::device_query<xq::rom_ddr_bank_size_gb>(device),
                 xrt_core::device_query<xq::rom_ddr_bank_count_max>(device)));
    bd_info.add("ddr_count", xrt_core::device_query<xq::rom_ddr_bank_count_max>(device));
  }
  catch (xq::exception& ex) {
    bd_info.add("error_msg", ex.what());
  }

  pt.put_child("off_chip_board_info", bd_info);
}

void
add_mig_info(const xrt_core::device* device, ptree_type& pt)
{
  try {
    pt.add("mig_calibrated", xrt_core::device_query<xq::status_mig_calibrated>(device));
  }
  catch (const xq::no_such_key&) {
    // Ignoring if not available: Edge Case
    pt.add("mig_calibrated", "N/A");
  }
}

void
add_p2p_info(const xrt_core::device* device, ptree_type& pt)
{
  auto value = xq::p2p_config::value_type::not_supported;
  try {
    auto config = xrt_core::device_query<xq::p2p_config>(device);
    std::tie(value, std::ignore) = xq::p2p_config::parse(config);
  }
  catch (const xq::exception&) {
  }

  pt.add("p2p_status", xq::p2p_config::to_string(value));
}

void
add_status_info(const xrt_core::device* device, ptree_type& pt)
{
  ptree_type pt_status;

  add_mig_info(device, pt_status);
  add_p2p_info(device, pt_status);

  pt.put_child("status", pt_status);
}

void
add_controller_info(const xrt_core::device* device, ptree_type& pt)
{
  ptree_type controller;

  try {
    ptree_type sc;
    sc.add("version", xrt_core::device_query<xq::xmc_sc_version>(device));
    sc.add("expected_version", xrt_core::device_query<xq::expected_sc_version>(device));
    ptree_type cmc;
    std::stringstream version;
    
    try {
       version << "0x" << std::hex << std::stoi(xrt_core::device_query<xq::xmc_version>(device));
    }
    catch (...) {}
    cmc.add("version", version.str());
    cmc.add("serial_number", xrt_core::device_query<xq::xmc_serial_num>(device));
    cmc.add("oem_id", xq::oem_id::parse(xrt_core::device_query<xq::oem_id>(device)));
    controller.put_child("satellite_controller", sc);
    controller.put_child("card_mgmt_controller", cmc);
    pt.put_child("controller", controller);
  }
  catch (const xq::exception&) {
    // Ignoring if not available: Edge Case
  }
}

void
add_clock_info(const xrt_core::device* device, ptree_type& pt)
{
  ptree_type pt_clock_array;

  try {
    auto raw = xrt_core::device_query<xq::clock_freq_topology_raw>(device);
    if(raw.empty())
      return;

    ptree_type pt_clocks;
    auto clock_topology = reinterpret_cast<const clock_freq_topology*>(raw.data());
    for(int i = 0; i < clock_topology->m_count; i++) {
      ptree_type pt_clock;
      pt_clock.add("id", clock_topology->m_clock_freq[i].m_name);
      pt_clock.add("description", xq::clock_freq_topology_raw::parse(clock_topology->m_clock_freq[i].m_name));
      pt_clock.add("freq_mhz", clock_topology->m_clock_freq[i].m_freq_Mhz);
      pt_clock_array.push_back(std::make_pair("", pt_clock));
    }
    pt.put_child("clocks", pt_clock_array);
  }
  catch (const xq::no_such_key&) {
    // ignoring if not available: Edge Case
  }
}

void
add_mac_info(const xrt_core::device* device, ptree_type& pt)
{
  ptree_type pt_mac;

  try {
    auto mac_contiguous_num = xrt_core::device_query<xq::mac_contiguous_num>(device);
    auto mac_addr_first = xrt_core::device_query<xq::mac_addr_first>(device);

    // new flow
    if (mac_contiguous_num && !mac_addr_first.empty()) {
      std::string mac_prefix = mac_addr_first.substr(0, mac_addr_first.find_last_of(":"));
      std::string mac_base = mac_addr_first.substr(mac_addr_first.find_last_of(":") + 1);
      constexpr int base = 16;
      auto mac_base_val = std::stoul(mac_base, nullptr, base);

      for (decltype(mac_contiguous_num) i = 0; i < mac_contiguous_num; ++i) {
        ptree_type addr;
        auto basex = boost::format("%02X") % (mac_base_val + i);
        addr.add("address", mac_prefix + ":" + basex.str());
        pt_mac.push_back(std::make_pair("", addr));
      }
    }
    else { //old flow
      auto  mac_addr = xrt_core::device_query<xq::mac_addr_list>(device);
      for (const auto& a : mac_addr) {
        ptree_type addr;
        if (!a.empty() && a.compare("FF:FF:FF:FF:FF:FF") != 0) {
          addr.add("address", a);
          pt_mac.push_back(std::make_pair("", addr));
        }
      }
    }
    pt.put_child("macs", pt_mac);

  }
  catch (const xq::no_such_key&) {
    // Ignoring if not available: Edge Case
  }
}

void
add_platform_info(const xrt_core::device* device, ptree_type& pt_platform_array)
{
  ptree_type pt_platform;
  ptree_type pt_platforms;

  add_static_region_info(device, pt_platform);
  add_board_info(device, pt_platform);
  add_status_info(device, pt_platform);
  add_controller_info(device, pt_platform);
  add_clock_info(device, pt_platform);
  add_mac_info(device, pt_platform);

  pt_platforms.push_back(std::make_pair("", pt_platform));
  pt_platform_array.add_child("platforms", pt_platforms);
}

} //unnamed namespace

namespace xrt_core { namespace platform {

ptree_type
platform_info(const xrt_core::device* device)
{
  ptree_type pt_platform_array; // array of platforms?
  add_platform_info(device, pt_platform_array);
  return pt_platform_array;
}

ptree_type
pcie_info(const xrt_core::device * device)
{
  ptree_type ptree;

  try {
    ptree.add("vendor", xq::pcie_vendor::to_string(xrt_core::device_query<xq::pcie_vendor>(device)));
    ptree.add("device", xq::pcie_device::to_string(xrt_core::device_query<xq::pcie_device>(device)));
    ptree.add("sub_device", xq::pcie_subsystem_id::to_string(xrt_core::device_query<xq::pcie_subsystem_id>(device)));
    ptree.add("sub_vendor", xq::pcie_subsystem_vendor::to_string(xrt_core::device_query<xq::pcie_subsystem_vendor>(device)));
    ptree.add("link_speed_gbit_sec", xrt_core::device_query<xq::pcie_link_speed_max>(device));
    ptree.add("express_lane_width_count", xrt_core::device_query<xq::pcie_express_lane_width>(device));

    // dma_thread_count might not be present for nodma, but it is safe to ignore.
    try {
      ptree.add("dma_thread_count", xrt_core::device_query<xq::dma_threads_raw>(device).size());
    }
    catch(const xq::no_such_key&) {
    }

    ptree.add("cpu_affinity", xrt_core::device_query<xq::cpu_affinity>(device));
    ptree.add("max_shared_host_mem_aperture_bytes", xrt_core::utils::unit_convert(xrt_core::device_query<xq::max_shared_host_mem_aperture_bytes>(device)));
    ptree.add("shared_host_mem_size_bytes", xrt_core::utils::unit_convert(xrt_core::device_query<xq::shared_host_mem>(device)));
    ptree.add("enabled_host_mem_size_bytes", xrt_core::utils::unit_convert(xrt_core::device_query<xq::enabled_host_mem>(device)));
  }
  catch(const xq::exception&) {
  }

  return ptree;
}

}} // platform, xrt
