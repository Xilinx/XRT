// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files

#include "TestDF_bandwidth.h"
#include "TestValidateUtilities.h"
#include "core/common/runner/runner.h"
#include "xrt/xrt_device.h"
#include "core/common/json/nlohmann/json.hpp"
#include "tools/common/XBUtilities.h"

using json = nlohmann::json;
#include <filesystem>

static constexpr std::string_view recipe_file = "recipe_df_bandwidth.json";
static constexpr std::string_view profile_file = "profile_df_bandwidth.json";

// ----- C L A S S   M E T H O D S -------------------------------------------
TestDF_bandwidth::TestDF_bandwidth()
  : TestRunner("df-bw", "Run bandwidth test on data fabric")
{}

boost::property_tree::ptree
TestDF_bandwidth::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  std::string repo_path = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::df_bandwidth);
  repo_path = XBValidateUtils::findPlatformFile(repo_path, ptree);
  std::string recipe = repo_path + std::string(recipe_file);
  std::string profile = repo_path + std::string(profile_file);
  try
  {
    xrt_core::runner runner(xrt::device(dev), recipe, profile, std::filesystem::path(repo_path));
    runner.execute();
    runner.wait();

    auto report = json::parse(runner.get_report());
    auto elapsed_us = report["cpu"]["elapsed"].get<double>();
    auto iterations = report["cpu"]["iterations"].get<int>();

    // Used buffer in runner is 1GB in size, thus reporting in GB/s
    double bandwidth = (2 * iterations ) / (elapsed_us / 1000000); // NOLINT: Runner reports in microseconds, so conversion is required until request supports timescales

    if(XBUtilities::getVerbose())
      XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Total duration: %.1fs") % (elapsed_us / 1000000)));

    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average bandwidth per shim DMA: %.1f GB/s") % bandwidth));
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
