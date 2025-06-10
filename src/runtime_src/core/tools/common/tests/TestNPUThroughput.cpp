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

static constexpr std::string_view recipe_file = "recipe_throughput.json";
static constexpr std::string_view profile_file = "profile_throughput.json";

// ----- C L A S S   M E T H O D S -------------------------------------------
TestNPUThroughput::TestNPUThroughput()
  : TestRunner("throughput", "Run end-to-end throughput test")
{}

boost::property_tree::ptree
TestNPUThroughput::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  std::string repo_path = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::throughput);
  repo_path = XBValidateUtils::findPlatformFile(repo_path, ptree);
  std::string recipe = repo_path + std::string(recipe_file);
  std::string profile = repo_path + std::string(profile_file);
  try
  {
    xrt_core::runner runner(xrt::device(dev), recipe, profile, std::filesystem::path(repo_path));
    runner.execute();
    runner.wait();

    auto report = json::parse(runner.get_report());
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average throughput: %.1f ops") % report["cpu"]["throughput"].get<double>()));
    ptree.put("status", XBValidateUtils::test_token_passed);
    return ptree;
  }
  catch(const std::exception& e)
  {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }
  return ptree;
}
