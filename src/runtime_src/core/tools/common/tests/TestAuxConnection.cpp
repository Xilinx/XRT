// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestAuxConnection.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// System - Include Files
#include <vector>

// ----- C L A S S   M E T H O D S -------------------------------------------
TestAuxConnection::TestAuxConnection()
  : TestRunner("aux-connection", 
                "Check if auxiliary power is connected"){}

boost::property_tree::ptree
TestAuxConnection::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  const std::vector<std::string> auxPwrRequiredDevice = { "VCU1525", "U200", "U250", "U280" };

  std::string name;
  uint64_t max_power = 0;
  try {
    name = xrt_core::device_query<xrt_core::query::xmc_board_name>(dev);
    max_power = xrt_core::device_query<xrt_core::query::max_power_level>(dev);
  }
  catch (const xrt_core::query::exception&) { }

  //check if device has aux power connector
  bool auxDevice = false;
  for (const auto& bd : auxPwrRequiredDevice) {
    if (name.find(bd) != std::string::npos) {
      auxDevice = true;
      break;
    }
  }

  if (!auxDevice) {
      XBValidateUtils::logger(ptree, "Details", "Aux power connector is not available on this board");
      ptree.put("status", XBValidateUtils::test_token_skipped);
      return ptree;
  }

  //check aux cable if board u200, u250, u280
  if (max_power == 0) {
    XBValidateUtils::logger(ptree, "Warning", "Aux power is not connected");
    XBValidateUtils::logger(ptree, "Warning", "Device is not stable for heavy acceleration tasks");
  }
  ptree.put("status", XBValidateUtils::test_token_passed);
  return ptree;
}
