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

// 3rd Party Library - Include Files
#include <boost/property_tree/json_parser.hpp>
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
  xrt::device device(dev->get_device_id());
  boost::property_tree::ptree pt_platform;
  std::stringstream ss;
  ss << device.get_info<xrt::info::device::platform>();
  boost::property_tree::read_json(ss, pt_platform);
 
  // There can only be 1 root node
  pt = pt_platform;
}

void 
ReportPlatforms::writeReport( const xrt_core::device* /*_pDevice*/,
                              const boost::property_tree::ptree& _pt, 
                              const std::vector<std::string>& /*_elementsFilter*/,
                              std::ostream & _output) const
{
  boost::property_tree::ptree empty_ptree;

  _output << "Platform\n";
  const boost::property_tree::ptree& platforms = _pt.get_child("platforms", empty_ptree);
  for(auto& kp : platforms) {
    const boost::property_tree::ptree& pt_platform = kp.second;
    const boost::property_tree::ptree& pt_static_region = pt_platform.get_child("static_region", empty_ptree);
    _output << boost::format("  %-23s: %s \n") % "XSA Name" % pt_static_region.get<std::string>("vbnv");
    _output << boost::format("  %-23s: %s \n") % "Platform UUID" % pt_static_region.get<std::string>("logic_uuid");
    _output << boost::format("  %-23s: %s \n") % "FPGA Name" % pt_static_region.get<std::string>("fpga_name");
    _output << boost::format("  %-23s: %s \n") % "JTAG ID Code" % pt_static_region.get<std::string>("jtag_idcode");
    
    const boost::property_tree::ptree& pt_board_info = pt_platform.get_child("off_chip_board_info");
    _output << boost::format("  %-23s: %s Bytes\n") % "DDR Size" % pt_board_info.get<std::string>("ddr_size_bytes");
    _output << boost::format("  %-23s: %s \n") % "DDR Count" % pt_board_info.get<std::string>("ddr_count");
    
    const boost::property_tree::ptree& pt_status = pt_platform.get_child("status");
    _output << boost::format("  %-23s: %s \n") % "Mig Calibrated" % pt_status.get<std::string>("mig_calibrated");
    _output << boost::format("  %-23s: %s \n") % "P2P Status" % pt_status.get<std::string>("p2p_status");

    const boost::property_tree::ptree& clocks = pt_platform.get_child("clocks", empty_ptree);
    if(!clocks.empty()) {
      _output << std::endl << "Clocks" << std::endl;
      for(auto& kc : clocks) {
        const boost::property_tree::ptree& pt_clock = kc.second;
        std::string clock_name_type = pt_clock.get<std::string>("id") + " (" + pt_clock.get<std::string>("description") + ")"; 
        _output << boost::format("  %-23s: %3s MHz\n") % clock_name_type % pt_clock.get<std::string>("freq_mhz");
      }
    }

    const boost::property_tree::ptree& macs = pt_platform.get_child("macs", empty_ptree);
    if(!macs.empty()) {
      _output << std::endl;
      unsigned int macCount = 0;

      for(auto& km : macs) {
        const boost::property_tree::ptree& pt_mac = km.second;
        if( macCount++ == 0) 
          _output << boost::format("%-25s: %s\n") % "Mac Addresses" % pt_mac.get<std::string>("address");
        else
          _output << boost::format("  %-23s: %s\n") % "" % pt_mac.get<std::string>("address");
      }
    }
  }
  
  _output << std::endl;
}
