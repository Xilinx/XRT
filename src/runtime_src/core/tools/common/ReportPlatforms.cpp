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
// System - Include Files
#include <map>

// Local - Include Files
#include "ReportPlatforms.h"
#include "XBUtilities.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
namespace qr = xrt_core::query;

static std::map<int, std::string> p2p_config_map = {
  { 0, "disabled" },
  { 1, "enabled" },
  { 2, "error" },
  { 3, "reboot" },
  { 4, "not supported" },
};

/**
 * New flow for exposing mac addresses
 * qr::mac_contiguous_num is the total number of mac addresses
 * avaliable contiguously starting from qr::mac_addr_first
 * 
 * Old flow: Query the four sysfs nodes we have and validate them
 * before adding them to the property tree
 */
static boost::property_tree::ptree
mac_addresses(const xrt_core::device * dev)
{
  boost::property_tree::ptree ptree;
  auto mac_contiguous_num = xrt_core::device_query<qr::mac_contiguous_num>(dev);
  auto mac_addr_first = xrt_core::device_query<qr::mac_addr_first>(dev);
  
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
    auto mac_addr = xrt_core::device_query<qr::mac_addr_list>(dev);
    for (const auto& a : mac_addr) {
      boost::property_tree::ptree addr;
      if (a.empty() || a.compare("FF:FF:FF:FF:FF:FF") != 0) {
        addr.add("address", a);
        ptree.push_back(std::make_pair("", addr));
      }
    }
   }
  return ptree;

}

void
ReportPlatforms::getPropertyTreeInternal( const xrt_core::device * dev, 
                                              boost::property_tree::ptree &pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(dev, pt);
}

void 
ReportPlatforms::getPropertyTree20202( const xrt_core::device * dev, 
                                           boost::property_tree::ptree &pt) const
{
  boost::property_tree::ptree ptree;
  boost::property_tree::ptree pt_platform;
  
  boost::property_tree::ptree static_region;
  static_region.add("vbnv", xrt_core::device_query<qr::rom_vbnv>(dev));
  static_region.add("jtag_idcode", qr::idcode::to_string(xrt_core::device_query<qr::idcode>(dev)));
  static_region.add("fpga_name", xrt_core::device_query<qr::rom_fpga_name>(dev));
  pt_platform.put_child("static_region", static_region);

  boost::property_tree::ptree bd_info;
  auto ddr_size_bytes = [](uint64_t size_gb, uint64_t count) {
    auto bytes = size_gb * 1024 * 1024 * 1024;
    return bytes * count;
  };
  bd_info.add("ddr_size_bytes", ddr_size_bytes(xrt_core::device_query<qr::rom_ddr_bank_size_gb>(dev), xrt_core::device_query<qr::rom_ddr_bank_count_max>(dev)));
  bd_info.add("ddr_count", xrt_core::device_query<qr::rom_ddr_bank_count_max>(dev));
  pt_platform.put_child("off_chip_board_info", bd_info);

  boost::property_tree::ptree status;
  status.add("mig_calibrated", xrt_core::device_query<qr::status_mig_calibrated>(dev));
  std::string msg;
  auto value = XBUtilities::check_p2p_config(dev, msg);
  status.add("p2p_status", p2p_config_map[value]);
  pt_platform.put_child("status", status);

  boost::property_tree::ptree controller;
  boost::property_tree::ptree sc;
  boost::property_tree::ptree cmc;
  sc.add("version", xrt_core::device_query<qr::xmc_sc_version>(dev));
  sc.add("expected_version", xrt_core::device_query<qr::expected_sc_version>(dev));
  cmc.add("version", xrt_core::device_query<qr::xmc_version>(dev));
  cmc.add("serial_number", xrt_core::device_query<qr::xmc_serial_num>(dev));
  cmc.add("oem_id", XBUtilities::parse_oem_id(xrt_core::device_query<qr::oem_id>(dev)));
  controller.put_child("satellite_controller", sc);
  controller.put_child("card_mgmt_controller", cmc);
  pt_platform.put_child("controller", controller);
  
  auto raw = xrt_core::device_query<qr::clock_freq_topology_raw>(dev);
  if(!raw.empty()) {
    boost::property_tree::ptree pt_clocks;
    auto clock_topology = reinterpret_cast<const clock_freq_topology*>(raw.data());
    for(int i = 0; i < clock_topology->m_count; i++) {
      boost::property_tree::ptree clock;
      clock.add("id", clock_topology->m_clock_freq[i].m_name);
      clock.add("description", XBUtilities::parse_clock_id(clock_topology->m_clock_freq[i].m_name));
      clock.add("freq_mhz", clock_topology->m_clock_freq[i].m_freq_Mhz);
      pt_clocks.push_back(std::make_pair("", clock));
    }
    pt_platform.put_child("clocks", pt_clocks);
  }
  
  auto macs = mac_addresses(dev);
  if(!macs.empty())
    pt_platform.put_child("macs", macs);
  
  ptree.push_back(std::make_pair("", pt_platform));
  // There can only be 1 root node
  pt.add_child("platforms", ptree);
}

void 
ReportPlatforms::writeReport( const xrt_core::device * dev,
                                  const std::vector<std::string> & /*_elementsFilter*/, 
                                  std::iostream & output) const
{
  boost::property_tree::ptree pt;
  boost::property_tree::ptree empty_ptree;
  getPropertyTreeInternal(dev, pt);

  output << "Platform\n";
  boost::property_tree::ptree& platforms = pt.get_child("platforms", empty_ptree);
  for(auto& kp : platforms) {
    boost::property_tree::ptree& pt_platform = kp.second;
    boost::property_tree::ptree& pt_static_region = pt_platform.get_child("static_region");
    output << boost::format("  %-20s : %s \n") % "XSA Name" % pt_static_region.get<std::string>("vbnv");
    output << boost::format("  %-20s : %s \n") % "FPGA Name" % pt_static_region.get<std::string>("fpga_name");
    output << boost::format("  %-20s : %s \n") % "JTAG ID Code" % pt_static_region.get<std::string>("jtag_idcode");
    
    boost::property_tree::ptree& pt_board_info = pt_platform.get_child("off_chip_board_info");
    output << boost::format("  %-20s : %s Bytes\n") % "DDR Size" % pt_board_info.get<std::string>("ddr_size_bytes");
    output << boost::format("  %-20s : %s \n") % "DDR Count" % pt_board_info.get<std::string>("ddr_count");
    
    boost::property_tree::ptree& pt_status = pt_platform.get_child("status");
    output << boost::format("  %-20s : %s \n") % "Mig Calibrated" % pt_status.get<std::string>("mig_calibrated");
    output << boost::format("  %-20s : %s \n") % "P2P Status" % pt_status.get<std::string>("p2p_status");

    boost::property_tree::ptree& clocks = pt_platform.get_child("clocks", empty_ptree);
    if(!clocks.empty())
      output << "Clocks\n";
    for(auto& kc : clocks) {
      boost::property_tree::ptree& pt_clock = kc.second;
      output << boost::format("    %-20s : %s MHz\n") % pt_clock.get<std::string>("description") % pt_clock.get<std::string>("freq_mhz");
    }

    boost::property_tree::ptree& macs = pt_platform.get_child("macs", empty_ptree);
    if(!macs.empty())
      output << "Mac Addresses\n";
    for(auto& km : macs) {
      boost::property_tree::ptree& pt_mac = km.second;
      output << boost::format("    %-20s : %s\n") % "" % pt_mac.get<std::string>("address");
    }
  }
  
  output << std::endl;
  
}
