// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

#include "TestAIEReconfigOverhead.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_device.h"
#include "core/common/runner/runner.h"
#include "core/common/json/nlohmann/json.hpp"
namespace XBU = XBUtilities;
using json = nlohmann::json;

TestAIEReconfigOverhead::TestAIEReconfigOverhead()
  : TestRunner("aie-reconfig-overhead", "Run end-to-end array reconfiguration overhead through shim DMA")
{}

boost::property_tree::ptree
TestAIEReconfigOverhead::run(std::shared_ptr<xrt_core::device> dev)
{

  boost::property_tree::ptree ptree = get_test_header();
  std::string recipe = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::aie_reconfig_overhead_recipe);
  std::string recipe_noop = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::aie_reconfig_overhead_nop_recipe);
  std::string profile = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::aie_reconfig_overhead_profile);
  std::string test = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::aie_reconfig_overhead_path); 
  auto recipe_path = XBValidateUtils::findPlatformFile(recipe, ptree);
  auto recipe_noop_path = XBValidateUtils::findPlatformFile(recipe_noop, ptree);
  auto profile_path = XBValidateUtils::findPlatformFile(profile, ptree);
  auto test_path =  XBValidateUtils::findPlatformFile(test, ptree); 
  try
  {
    xrt_core::runner runner(xrt::device(dev), recipe_path, profile_path, std::filesystem::path(test_path));

    //Run 1
    runner.execute();
    runner.wait();
    auto report = json::parse(runner.get_report());
    auto elapsed = report["cpu"]["elapsed"].get<double>();

    runner = xrt_core::runner(xrt::device(dev), recipe_noop_path, profile_path);

    //Run 2
    runner.execute();
    runner.wait();
    report = json::parse(runner.get_report());
    auto elapsed_nop = report["cpu"]["elapsed"].get<double>();

    auto iterations = report["cpu"]["iterations"].get<double>(); 
    double overhead = (elapsed - elapsed_nop) / (iterations * 1000); //NOLINT conversion to ms 

    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Array reconfiguration overhead: %.1f ms") % overhead));
    ptree.put("status", XBValidateUtils::test_token_passed);
  }
  catch(const std::exception& e)
  {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }
}
