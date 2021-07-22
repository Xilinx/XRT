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

// ------ I N C L U D E   F I L E S -------------------------------------------
#define XRT_CORE_COMMON_SOURCE

// Local - Include Files
#include "info_platform.h"
#include "query_requests.h"
#include "utils.h"

namespace {

static std::map<int, std::string> p2p_config_map = {
  { 0, "disabled" },
  { 1, "enabled" },
  { 2, "error" },
  { 3, "reboot" },
  { 4, "not supported" },
};

/**
 * New flow for exposing mac addresses
 * xrt_core::query::mac_contiguous_num is the total number of mac addresses
 * avaliable contiguously starting from xrt_core::query::mac_addr_first
 * 
 * Old flow: Query the four sysfs nodes we have and validate them
 * before adding them to the property tree
 */
static boost::property_tree::ptree
mac_addresses(const xrt_core::device * dev)
{
  boost::property_tree::ptree ptree;
  uint64_t mac_contiguous_num = 0;
  std::string mac_addr_first;
  try {
    mac_contiguous_num = xrt_core::device_query<xrt_core::query::mac_contiguous_num>(dev);
    mac_addr_first = xrt_core::device_query<xrt_core::query::mac_addr_first>(dev);
  }
  catch (const xrt_core::query::no_such_key&) {
    // Ignoring if not available: Edge Case 
  }

  //new flow
  if (mac_contiguous_num && !mac_addr_first.empty()) {
    std::string mac_prefix = mac_addr_first.substr(0, mac_addr_first.find_last_of(":"));
    std::string mac_base = mac_addr_first.substr(mac_addr_first.find_last_of(":") + 1);
    std::stringstream ss;
    uint32_t mac_base_val = 0;
    ss << std::hex << mac_base;
    ss >> mac_base_val;

    for (uint32_t i = 0; i < (uint32_t)mac_contiguous_num; i++) {
      boost::property_tree::ptree addr;
      auto base = boost::format("%02X") % (mac_base_val + i);
      addr.add("address", mac_prefix + ":" + base.str());
      ptree.push_back(std::make_pair("", addr));
    } 
  }
  else { //old flow
    std::vector<std::string> mac_addr;
    try {	  
      mac_addr = xrt_core::device_query<xrt_core::query::mac_addr_list>(dev);
    }
    catch (const xrt_core::query::no_such_key&) {
      // Ignoring if not available: Edge Case 
    }
    for (const auto& a : mac_addr) {
      boost::property_tree::ptree addr;
      if (!a.empty() && a.compare("FF:FF:FF:FF:FF:FF") != 0) {
        addr.add("address", a);
        ptree.push_back(std::make_pair("", addr));
      }
    }
  }

  return ptree;
}

} //unnamed namespace

namespace xrt_core {
namespace platform {

boost::property_tree::ptree
platform_info(const xrt_core::device * device) {
  boost::property_tree::ptree ptree;
  boost::property_tree::ptree pt_platform;
  
  boost::property_tree::ptree static_region;
  static_region.add("vbnv", xrt_core::device_query<xrt_core::query::rom_vbnv>(device));

  std::vector<std::string> interface_uuids;
  // the vectors are being populated by empty strings which need to be removed
  try {
    interface_uuids = xrt_core::device_query<xrt_core::query::interface_uuids>(device);
    interface_uuids.erase(
      std::remove_if(interface_uuids.begin(), interface_uuids.end(),	
                  [](const std::string& s) { return s.empty(); }), interface_uuids.end());
  } catch (const xrt_core::query::no_such_key&) {}
  static_region.add("interface_uuid", xrt_core::utils::string_to_UUID(interface_uuids[0]));

  try {
    static_region.add("jtag_idcode", xrt_core::query::idcode::to_string(xrt_core::device_query<xrt_core::query::idcode>(device)));
  }
  catch (const xrt_core::query::no_such_key&) {
    // Ignoring if not available: Edge Case
    static_region.add("jtag_idcode", "N/A");
  }

  try {
    static_region.add("fpga_name", xrt_core::device_query<xrt_core::query::rom_fpga_name>(device));
  }
  catch (const xrt_core::query::no_such_key&) {
    // Ignoring if not available: Edge Case 
    static_region.add("fpga_name", "N/A");
  }
  
  pt_platform.put_child("static_region", static_region);

  boost::property_tree::ptree bd_info;
  auto ddr_size_bytes = [](uint64_t size_gb, uint64_t count) {
    auto bytes = size_gb * 1024 * 1024 * 1024;
    return bytes * count;
  };
  bd_info.add("ddr_size_bytes", ddr_size_bytes(xrt_core::device_query<xrt_core::query::rom_ddr_bank_size_gb>(device), xrt_core::device_query<xrt_core::query::rom_ddr_bank_count_max>(device)));
  bd_info.add("ddr_count", xrt_core::device_query<xrt_core::query::rom_ddr_bank_count_max>(device));
  pt_platform.put_child("off_chip_board_info", bd_info);

  boost::property_tree::ptree status;
  try {
    status.add("mig_calibrated", xrt_core::device_query<xrt_core::query::status_mig_calibrated>(device));
  }
  catch (const xrt_core::query::no_such_key&) {
    // Ignoring if not available: Edge Case 
    status.add("mig_calibrated", "N/A");
  }

  std::vector<std::string> config;
  std::string msg;
  int value = static_cast<int>(xrt_core::utils::p2p_config::not_supported);
  try {
    config = xrt_core::device_query<xrt_core::query::p2p_config>(device);
    value = xrt_core::utils::parse_p2p_config(config, msg);
  }
  catch (const std::runtime_error&) {
    value = static_cast<int>(xrt_core::utils::p2p_config::not_supported);
  }
  catch (const xrt_core::query::no_such_key&) {
    value = static_cast<int>(xrt_core::utils::p2p_config::not_supported);
  }
  
  status.add("p2p_status", p2p_config_map[value]);
  pt_platform.put_child("status", status);

  boost::property_tree::ptree controller;
  boost::property_tree::ptree sc;
  boost::property_tree::ptree cmc;
  try {
    sc.add("version", xrt_core::device_query<xrt_core::query::xmc_sc_version>(device));
    sc.add("expected_version", xrt_core::device_query<xrt_core::query::expected_sc_version>(device));
    cmc.add("version", xrt_core::device_query<xrt_core::query::xmc_version>(device));
    cmc.add("serial_number", xrt_core::device_query<xrt_core::query::xmc_serial_num>(device));
    cmc.add("oem_id", xrt_core::utils::parse_oem_id(xrt_core::device_query<xrt_core::query::oem_id>(device)));
  }
  catch (const xrt_core::query::no_such_key&) {
    // Ignoring if not available: Edge Case 
  }
  controller.put_child("satellite_controller", sc);
  controller.put_child("card_mgmt_controller", cmc);
  pt_platform.put_child("controller", controller);

  std::vector<char> raw; 
  try { 
    raw = xrt_core::device_query<xrt_core::query::clock_freq_topology_raw>(device);
  }
  catch (const xrt_core::query::no_such_key&) {
    // Ignoring if not available: Edge Case 
  }
  if(!raw.empty()) {
    boost::property_tree::ptree pt_clocks;
    auto clock_topology = reinterpret_cast<const clock_freq_topology*>(raw.data());
    for(int i = 0; i < clock_topology->m_count; i++) {
      boost::property_tree::ptree clock;
      clock.add("id", clock_topology->m_clock_freq[i].m_name);
      clock.add("description", xrt_core::utils::parse_clock_id(clock_topology->m_clock_freq[i].m_name));
      clock.add("freq_mhz", clock_topology->m_clock_freq[i].m_freq_Mhz);
      pt_clocks.push_back(std::make_pair("", clock));
    }
    pt_platform.put_child("clocks", pt_clocks);
  }

  auto macs = mac_addresses(device);
  if(!macs.empty())
    pt_platform.put_child("macs", macs);
    
  ptree.push_back(std::make_pair("", pt_platform));
  return ptree;
}

boost::property_tree::ptree
pcie_info(const xrt_core::device * device) {
  boost::property_tree::ptree ptree;
  try {
    ptree.add("vendor", xrt_core::query::pcie_vendor::to_string(xrt_core::device_query<xrt_core::query::pcie_vendor>(device)));
    ptree.add("device", xrt_core::query::pcie_device::to_string(xrt_core::device_query<xrt_core::query::pcie_device>(device)));
    ptree.add("sub_device", xrt_core::query::pcie_subsystem_id::to_string(xrt_core::device_query<xrt_core::query::pcie_subsystem_id>(device)));
    ptree.add("sub_vendor", xrt_core::query::pcie_subsystem_vendor::to_string(xrt_core::device_query<xrt_core::query::pcie_subsystem_vendor>(device)));
    ptree.add("link_speed_gbit_sec", xrt_core::device_query<xrt_core::query::pcie_link_speed_max>(device));
    ptree.add("express_lane_width_count", xrt_core::device_query<xrt_core::query::pcie_express_lane_width>(device));

    //this sysfs node might not be present for nodma, but it is safe to ignore.
    try {
      ptree.add("dma_thread_count", xrt_core::device_query<xrt_core::query::dma_threads_raw>(device).size());
    } catch(...) {}
    ptree.add("cpu_affinity", xrt_core::device_query<xrt_core::query::cpu_affinity>(device));
    ptree.add("max_shared_host_mem_aperture_bytes", xrt_core::device_query<xrt_core::query::max_shared_host_mem_aperture_bytes>(device));
    ptree.add("shared_host_mem_size_bytes", xrt_core::device_query<xrt_core::query::shared_host_mem>(device));
  } catch(...) {}
  
  return ptree;
}

}} // platform, xrt