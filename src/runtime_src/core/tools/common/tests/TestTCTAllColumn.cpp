// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestTCTAllColumn.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_device.h"
#include "core/common/runner/runner.h"
#include "core/common/json/nlohmann/json.hpp"

// System - Include Files
#include <filesystem>

using json = nlohmann::json;
namespace XBU = XBUtilities;

static constexpr int run_count = 100;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestTCTAllColumn::TestTCTAllColumn()
  : TestRunner("tct-all-col", "Measure average TCT processing time for all columns")
{}

boost::property_tree::ptree
TestTCTAllColumn::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  std::string recipe = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::tct_all_column_recipe);
  std::string profile = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::tct_all_column_profile);
  std::string test = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::tct_all_column_path);
  auto recipe_path = XBValidateUtils::findPlatformFile(recipe, ptree);
  auto profile_path = XBValidateUtils::findPlatformFile(profile, ptree);
  auto test_path = XBValidateUtils::findPlatformFile(test, ptree);

  try
  {
    // Create runner once 
    xrt_core::runner runner(xrt::device(dev), recipe_path, profile_path, std::filesystem::path(test_path));
    
    // Run the execution run_count times 
    for (int run_num = 0; run_num < run_count; run_num++) {
      runner.execute();
      runner.wait();
    }

    // Get final metrics from the last run 
    auto report = json::parse(runner.get_report());
    double latency = report["cpu"]["latency"].get<double>(); // Should be in microseconds
    double throughput = report["cpu"]["throughput"].get<double>(); // Should be in operations/second

    if(XBU::getVerbose())
      XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average time for TCT (all columns): %.1f us") % latency));
    
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average TCT throughput (all columns): %.1f TCT/s") % throughput));
    ptree.put("status", XBValidateUtils::test_token_passed);
  }
  catch(const std::exception& e)
  {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
  }
  return ptree;
}
