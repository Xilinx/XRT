// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestCmdChainThroughput.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_device.h"
#include "core/common/runner/runner.h"
#include "core/common/json/nlohmann/json.hpp"
#include "core/common/archive.h"
namespace XBU = XBUtilities;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestCmdChainThroughput::TestCmdChainThroughput()
  : TestRunner("cmd-chain-throughput", "Run end-to-end throughput test using command chaining")
{}

boost::property_tree::ptree
TestCmdChainThroughput::run(const std::shared_ptr<xrt_core::device>&)
{
  boost::property_tree::ptree ptree = get_test_header();
  return ptree;
}

boost::property_tree::ptree
TestCmdChainThroughput::run(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive)
{
  boost::property_tree::ptree ptree = get_test_header();

  try {
    std::string recipe_data = archive->data("recipe_cmd_chain_throughput.json");
    std::string profile_data = archive->data("profile_cmd_chain_throughput.json"); 
    
    // Extract artifacts using helper method
    auto artifacts_repo = extract_artifacts_from_archive(archive, {
      "validate.xclbin", 
      "nop.elf" 
    }, ptree);
    
    // Create runner with recipe, profile, and artifacts repository
    xrt_core::runner runner(xrt::device(dev), recipe_data, profile_data, artifacts_repo);
    runner.execute();
    runner.wait();

    auto report = nlohmann::json::parse(runner.get_report());
    auto throughput = report["cpu"]["throughput"].get<double>();

    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average throughput: %.1f ops/s") % throughput));
    ptree.put("status", XBValidateUtils::test_token_passed);
  }
  catch (const std::exception& ex) {
    XBValidateUtils::logger(ptree, "Error", ex.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
  }

  return ptree;
}
