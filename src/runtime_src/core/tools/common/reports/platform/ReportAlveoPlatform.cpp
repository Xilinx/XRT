// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// System - Include Files
#include <map>

// Local - Include Files
#include "ReportAlveoPlatform.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/json_parser.hpp>

void
ReportAlveoPlatform::getPropertyTreeInternal(const xrt_core::device* dev,
                                             boost::property_tree::ptree& pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data,
  // Then update this method to do so.
  getPropertyTree20202(dev, pt);
}

void
ReportAlveoPlatform::getPropertyTree20202(const xrt_core::device* dev,
                                          boost::property_tree::ptree& pt) const
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
ReportAlveoPlatform::writeReport(const xrt_core::device* /*_pDevice*/,
                                 const boost::property_tree::ptree& _pt,
                                 const std::vector<std::string>& /*_elementsFilter*/,
                                 std::ostream& _output) const
{
  boost::property_tree::ptree empty_ptree;

  _output << "Platform\n";
  const boost::property_tree::ptree& platforms = _pt.get_child("platforms", empty_ptree);
  for (const auto& kp : platforms) {
    const boost::property_tree::ptree& pt_platform = kp.second;
    const boost::property_tree::ptree& pt_static_region = pt_platform.get_child("static_region", empty_ptree);
    _output << boost::format("  %-23s: %s \n") % "XSA Name" % pt_static_region.get<std::string>("vbnv");
    _output << boost::format("  %-23s: %s \n") % "Logic UUID" % pt_static_region.get<std::string>("logic_uuid");
    _output << boost::format("  %-23s: %s \n") % "FPGA Name" % pt_static_region.get<std::string>("fpga_name");
    _output << boost::format("  %-23s: %s \n") % "JTAG ID Code" % pt_static_region.get<std::string>("jtag_idcode");
    
    const boost::property_tree::ptree& pt_board_info = pt_platform.get_child("off_chip_board_info");
    _output << boost::format("  %-23s: %s Bytes\n") % "DDR Size" % pt_board_info.get<std::string>("ddr_size_bytes");
    _output << boost::format("  %-23s: %s \n") % "DDR Count" % pt_board_info.get<std::string>("ddr_count");
    try {
      _output << boost::format("  %-23s: %s \n") % "Revision" % pt_board_info.get<std::string>("revision");
      _output << boost::format("  %-23s: %s \n") % "MFG Date" % pt_board_info.get<std::string>("mfg_date");
    } catch (...) {
    }

    const boost::property_tree::ptree& pt_status = pt_platform.get_child("status");
    _output << boost::format("  %-23s: %s \n") % "Mig Calibrated" % pt_status.get<std::string>("mig_calibrated");
    _output << boost::format("  %-23s: %s \n") % "P2P Status" % pt_status.get<std::string>("p2p_status");

    const boost::property_tree::ptree ptEmpty;
    const boost::property_tree::ptree& pt_config = pt_platform.get_child("config.p2p", ptEmpty); 
    if (!pt_config.empty())
      _output << boost::format("  %-23s: %s\n") % "P2P IO space required" % pt_config.get<std::string>("exp_bar"); // Units appended when ptree is created

    const boost::property_tree::ptree& clocks = pt_platform.get_child("clocks.clocks", empty_ptree);
    if (!clocks.empty()) {
      _output << std::endl << "Clocks" << std::endl;
      for (const auto& kc : clocks) {
        const boost::property_tree::ptree& pt_clock = kc.second;
        std::string clock_name_type = pt_clock.get<std::string>("id") + " (" + pt_clock.get<std::string>("description") + ")";
        _output << boost::format("  %-23s: %3s MHz\n") % clock_name_type % pt_clock.get<std::string>("freq_mhz");
      }
    }

    const boost::property_tree::ptree& macs = pt_platform.get_child("macs", empty_ptree);
    if (!macs.empty()) {
      _output << std::endl;
      unsigned int macCount = 0;

      for (const auto& km : macs) {
        const boost::property_tree::ptree& pt_mac = km.second;
        if ( macCount++ == 0)
          _output << boost::format("%-25s: %s\n") % "Mac Addresses" % pt_mac.get<std::string>("address");
        else
          _output << boost::format("  %-23s: %s\n") % "" % pt_mac.get<std::string>("address");
      }
    }
  }

  _output << std::endl;
}
