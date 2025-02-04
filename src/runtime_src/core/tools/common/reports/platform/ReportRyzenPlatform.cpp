// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// System - Include Files
#include <map>

// Local - Include Files
#include "ReportRyzenPlatform.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/json_parser.hpp>

void
ReportRyzenPlatform::getPropertyTreeInternal(const xrt_core::device* dev,
                                             boost::property_tree::ptree& pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data,
  // Then update this method to do so.
  getPropertyTree20202(dev, pt);
}

void
ReportRyzenPlatform::getPropertyTree20202(const xrt_core::device* dev,
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
ReportRyzenPlatform::writeReport(const xrt_core::device* /*_pDevice*/,
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
    _output << boost::format("  %-23s: %s \n") % "Name" % pt_static_region.get<std::string>("name");

    const boost::property_tree::ptree& pt_status = pt_platform.get_child("status");
    _output << boost::format("  %-23s: %s \n") % "Power Mode" % pt_status.get<std::string>("power_mode");
    _output << boost::format("  %-23s: %s \n") % "Total Columns" % pt_static_region.get<std::string>("total_columns");


    auto watts = pt_platform.get<std::string>("electrical.power_consumption_watts", "N/A");
    if (watts != "N/A")
      _output << std::endl << boost::format("%-23s  : %s Watts\n") % "Power" % watts;
    else
      _output << std::endl << boost::format("%-23s  : %s\n") % "Power" % watts;
  }

  _output << std::endl;
}
