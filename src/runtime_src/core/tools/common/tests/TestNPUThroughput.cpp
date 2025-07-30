// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestNPUThroughput.h"
#include "TestValidateUtilities.h"
#include "core/common/runner/runner.h"
#include "xrt/xrt_device.h"
#include "core/common/json/nlohmann/json.hpp"

using json = nlohmann::json;
#include <filesystem>


// ----- C L A S S   M E T H O D S -------------------------------------------
TestNPUThroughput::TestNPUThroughput()
  : TestRunner("throughput", "Run end-to-end throughput test")
{}

boost::property_tree::ptree
TestNPUThroughput::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  std::string recipe = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::throughput_recipe);
  std::string profile = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::throughput_profile);
  std::string test = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::throughput_path);
  auto recipe_path = XBValidateUtils::findPlatformFile(recipe, ptree);
  auto profile_path = XBValidateUtils::findPlatformFile(profile, ptree);
  auto test_path = XBValidateUtils::findPlatformFile(test, ptree);
  try
  {
    xrt_core::runner runner(xrt::device(dev), recipe_path, profile_path, std::filesystem::path(test_path));
    runner.execute();
    runner.wait();

    auto report = json::parse(runner.get_report());
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average throughput: %.1f ops") % report["cpu"]["throughput"].get<double>()));
    ptree.put("status", XBValidateUtils::test_token_passed);
  }
  catch(const std::exception& e)
  {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }
  return ptree;
}
