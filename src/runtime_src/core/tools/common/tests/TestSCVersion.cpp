// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestSCVersion.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestSCVersion::TestSCVersion()
  : TestRunner("sc-version", 
                "Check if SC firmware is up-to-date"){}

boost::property_tree::ptree
TestSCVersion::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();

  auto sc_ver = xrt_core::device_query_default<xrt_core::query::xmc_sc_version>(dev, "");
  auto exp_sc_ver = xrt_core::device_query_default<xrt_core::query::expected_sc_version>(dev, "");

  if (!exp_sc_ver.empty() && sc_ver.compare(exp_sc_ver) != 0) {
    XBValidateUtils::logger(ptree, "Warning", "SC firmware mismatch");
    XBValidateUtils::logger(ptree, "Warning", boost::str(boost::format("SC firmware version %s is running on the platform, but SC firmware version %s is expected for the installed base platform. %s, and %s.")
                                          % sc_ver % exp_sc_ver % "Please use xbmgmt examine to see the compatible SC version corresponding to this base platform"
                                          % "reprogram the base partition using xbmgmt program --base ... to update the SC version"));
  }
  ptree.put("status", XBValidateUtils::test_token_passed);
  return ptree;
}
