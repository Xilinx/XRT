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

static constexpr std::string_view recipe_file = "recipe_aie_reconfig.json";
static constexpr std::string_view recipe_nop_file = "recipe_nop.json";
static constexpr std::string_view profile_file = "profile.json";

TestAIEReconfigOverhead::TestAIEReconfigOverhead()
  : TestRunner("aie-reconfig-overhead", "Run end-to-end array reconfiguration overhead through shim DMA")
{}

boost::property_tree::ptree
TestAIEReconfigOverhead::run(std::shared_ptr<xrt_core::device> dev)
{

  boost::property_tree::ptree ptree = get_test_header();
  std::string repo_path = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::aie_reconfig_overhead);
  repo_path = XBValidateUtils::findPlatformFile(repo_path, ptree);
  std::string recipe = repo_path + std::string(recipe_file);
  std::string recipe_noop = repo_path + std::string(recipe_nop_file);
  std::string profile = repo_path + std::string(profile_file);
  try
  {
    xrt_core::runner runner(xrt::device(dev), recipe, profile, std::filesystem::path(repo_path));

    //Run 1
    runner.execute();
    runner.wait();
    auto report = json::parse(runner.get_report());
    auto elapsed = report["cpu"]["elapsed"].get<double>();

    runner = xrt_core::runner(xrt::device(dev), recipe_noop, profile, std::filesystem::path(repo_path));

    //Run 2
    runner.execute();
    runner.wait();
    report = json::parse(runner.get_report());
    auto elapsed_nop = report["cpu"]["elapsed"].get<double>();

    auto iterations = report["cpu"]["iterations"].get<double>(); 
    double overhead = (elapsed - elapsed_nop) / (iterations * 1000); //NOLINT conversion to ms 

    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Array reconfiguration overhead: %.1f ms") % overhead));
    ptree.put("status", XBValidateUtils::test_token_passed);
    return ptree;
  }
  catch(const std::exception& e)
  {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }
}
