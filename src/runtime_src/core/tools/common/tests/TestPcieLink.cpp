// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestPcieLink.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestPcieLink::TestPcieLink()
  : TestRunner("pcie-link", 
                "Check if PCIE link is active"){}

boost::property_tree::ptree
TestPcieLink::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  const uint64_t speed     = xrt_core::device_query<xrt_core::query::pcie_link_speed>(dev);
  const uint64_t max_speed = xrt_core::device_query<xrt_core::query::pcie_link_speed_max>(dev);
  const uint64_t width     = xrt_core::device_query<xrt_core::query::pcie_express_lane_width>(dev);
  const uint64_t max_width = xrt_core::device_query<xrt_core::query::pcie_express_lane_width_max>(dev);
  if (speed != max_speed || width != max_width) {
    XBValidateUtils::logger(ptree, "Warning", "Link is active");
    XBValidateUtils::logger(ptree, "Warning", boost::str(boost::format("Please make sure that the device is plugged into Gen %dx%d, instead of Gen %dx%d. %s.")
                                          % max_speed % max_width % speed % width % "Lower performance maybe experienced"));
  }
  ptree.put("status", XBValidateUtils::test_token_passed);
  return ptree;
}
