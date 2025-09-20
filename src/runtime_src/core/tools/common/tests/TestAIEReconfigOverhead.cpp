// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

#include "TestAIEReconfigOverhead.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_device.h"
#include "core/common/runner/runner.h"
#include "core/common/json/nlohmann/json.hpp"
#include "core/common/archive.h"
namespace XBU = XBUtilities;
using json = nlohmann::json;

TestAIEReconfigOverhead::TestAIEReconfigOverhead()
  : TestRunner("aie-reconfig-overhead", "Run end-to-end array reconfiguration overhead through shim DMA")
{}

boost::property_tree::ptree
TestAIEReconfigOverhead::run(const std::shared_ptr<xrt_core::device>&)
{
  boost::property_tree::ptree ptree = get_test_header();
  return ptree;
}

boost::property_tree::ptree
TestAIEReconfigOverhead::run(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive)
{
  boost::property_tree::ptree ptree = get_test_header();

  try {
    std::string recipe_data = archive->data("recipe_aie_reconfig.json");
    std::string recipe_noop_data = archive->data("recipe_aie_reconfig_nop.json");
    std::string profile_data = archive->data("profile_aie_reconfig.json"); 
    
    auto artifacts_repo = extract_artifacts_from_archive(archive, {
      "aie_reconfig.xclbin", 
      "aie_reconfig.elf",
      "nop.elf" 
    }, ptree);
    
    // Create runner with recipe, profile, and artifacts repository - Run 1
    xrt_core::runner runner(xrt::device(dev), recipe_data, profile_data, artifacts_repo);
    runner.execute();
    runner.wait();
    auto report = json::parse(runner.get_report());
    auto elapsed = report["cpu"]["elapsed"].get<double>();

    // Run 2 with noop recipe
    runner = xrt_core::runner(xrt::device(dev), recipe_noop_data, profile_data, artifacts_repo);
    runner.execute();
    runner.wait();
    report = json::parse(runner.get_report());
    auto elapsed_nop = report["cpu"]["elapsed"].get<double>();

    auto iterations = report["iterations"].get<double>(); 
    double overhead = (elapsed - elapsed_nop) / (iterations * 1000); //NOLINT conversion to ms 

    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Array reconfiguration overhead: %.1f ms") % overhead));
    ptree.put("status", XBValidateUtils::test_token_passed);
  }
  catch(const std::exception& e) {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
  }

  return ptree;
}
