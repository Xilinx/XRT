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

// ----- C L A S S   M E T H O D S -------------------------------------------
TestDF_bandwidth::TestDF_bandwidth()
  : TestRunner("df-bw", "Run bandwidth test on data fabric")
{}

boost::property_tree::ptree
TestDF_bandwidth::run(const std::shared_ptr<xrt_core::device>& dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  std::string recipe = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::df_bandwidth_recipe);
  std::string profile = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::df_bandwidth_profile);
  std::string test = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::df_bandwidth_path);
  auto recipe_path = XBValidateUtils::findPlatformFile(recipe, ptree);
  auto profile_path = XBValidateUtils::findPlatformFile(profile, ptree);
  auto test_path = XBValidateUtils::findPlatformFile(test, ptree);

  try
  {
    xrt_core::runner runner(xrt::device(dev), recipe_path, profile_path, std::filesystem::path(test_path));
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
