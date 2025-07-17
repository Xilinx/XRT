// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestCmdChainThroughput.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_device.h"
#include "core/common/runner/runner.h"
#include "core/common/json/nlohmann/json.hpp"
namespace XBU = XBUtilities;

static constexpr std::string_view recipe_file = "recipe_cmd_chain_throughput.json";
static constexpr std::string_view profile_file = "profile_cmd_chain_throughput.json";
// ----- C L A S S   M E T H O D S -------------------------------------------
TestCmdChainThroughput::TestCmdChainThroughput()
  : TestRunner("cmd-chain-throughput", "Run end-to-end throughput test using command chaining")
{}

boost::property_tree::ptree
TestCmdChainThroughput::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  std::string repo_path = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::cmd_chain_throughput);
  repo_path = XBValidateUtils::findPlatformFile(repo_path, ptree);
  std::string recipe = repo_path + std::string(recipe_file);
  std::string profile = repo_path + std::string(profile_file);
  try
  {
    xrt_core::runner runner(xrt::device(dev), recipe, profile, std::filesystem::path(repo_path));
    runner.execute();
    runner.wait();

    auto report = nlohmann::json::parse(runner.get_report());
    auto elapsed_us = report["cpu"]["elapsed"].get<double>();
    auto iterations = report["cpu"]["iterations"].get<int>();
    auto runs = report["resources"]["runs"].get<int>();

    const double throughput = ((iterations * runs) / elapsed_us) * 1e6;

    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average throughput: %.1f ops") % throughput));
    ptree.put("status", XBValidateUtils::test_token_passed);
  }
  catch (const std::exception& ex)
  {
    XBValidateUtils::logger(ptree, "Error", ex.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }

  return ptree;
}
