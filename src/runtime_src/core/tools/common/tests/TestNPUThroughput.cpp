// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestNPUThroughput.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "core/common/runner/runner.h"
#include "xrt/xrt_device.h"
#include "core/common/json/nlohmann/json.hpp"
#include "core/common/archive.h"

using json = nlohmann::json;
#include <filesystem>
#include <stdexcept>

// ----- C L A S S   M E T H O D S -------------------------------------------
TestNPUThroughput::TestNPUThroughput()
  : TestRunner("throughput", "Run end-to-end throughput test")
{}

double
TestNPUThroughput::
get_throughput_from_report(const json& report) const
{
  if (report.contains("executions")) {
    const auto& execs = report.at("executions");
    if (!execs.is_array() || execs.size() != 1)
      throw std::runtime_error("profile_throughput.json must define exactly one execution");

    return execs.at(0).at("cpu").at("throughput").get<double>();
  }

  return report.at("cpu").at("throughput").get<double>();
}

boost::property_tree::ptree
TestNPUThroughput::run(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive)
{
  boost::property_tree::ptree ptree = get_test_header();

  if (archive == nullptr) {
    ptree.put("status", XBValidateUtils::test_token_failed);
    XBValidateUtils::logger(ptree, "Error", "No archive found, skipping test");
    return ptree;
  }
  
  try {
    std::string recipe_data = archive->data("recipe_throughput.json");
    std::string profile_data = archive->data("profile_throughput.json"); 
    
    auto artifacts_repo = XBUtilities::extract_artifacts_from_archive(archive, {
      "validate.xclbin", 
      "nop.elf" 
    });
    
    // Create runner with recipe, profile, and artifacts repository
    xrt_core::runner runner(xrt::device(dev), recipe_data, profile_data, artifacts_repo);
    runner.execute();
    runner.wait();

    const auto report = json::parse(runner.get_report());
    const double throughput = get_throughput_from_report(report);
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average throughput: %.1f op/s") % throughput));
    ptree.put("status", XBValidateUtils::test_token_passed);
  }
  catch(const std::exception& e) {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }
  return ptree;
}
