// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportClocks.h"
#include "core/common/info_platform.h"
#include "tools/common/Table2D.h"

// 3rd Party Library - Include Files
#include <vector>

using bpt = boost::property_tree::ptree;

void
ReportClocks::getPropertyTreeInternal(const xrt_core::device* dev,
                                         bpt& pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data,
  // Then update this method to do so.
  getPropertyTree20202(dev, pt);
}

void
ReportClocks::getPropertyTree20202(const xrt_core::device* dev,
                                      bpt& pt) const
{
  // There can only be 1 root node
  pt = xrt_core::platform::get_clock_info(dev);
}

void
ReportClocks::writeReport(const xrt_core::device* /*_pDevice*/,
                             const bpt& pt,
                             const std::vector<std::string>& /*_elementsFilter*/,
                             std::ostream& _output) const
{
  _output << "Clocks\n";

  const bpt empty_ptree;
  const bpt& pt_clock_array = pt.get_child("clocks", empty_ptree);
  if (pt_clock_array.empty()) {
    _output << "  No Clocks information available\n\n";
    return;
  }

  //print clocks
  std::stringstream ss;
  for (const auto& kc : pt_clock_array) {
    const bpt& pt_clock = kc.second;
    ss << boost::format("  %-23s: %3s MHz\n") % pt_clock.get<std::string>("id") 
                                              % pt_clock.get<std::string>("freq_mhz");
  }
  std::cout << ss.str();
}
