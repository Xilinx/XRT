// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestNPULatency.h"
#include "TestValidateUtilities.h"
#include "xrt/xrt_device.h"
#include "core/common/runner/runner.h"
#include "core/common/json/nlohmann/json.hpp"
#include "core/common/archive.h"

using json = nlohmann::json;
// System - Include Files
#include <filesystem>

// ----- C L A S S   M E T H O D S -------------------------------------------

TestNPULatency::TestNPULatency()
   : TestRunner("latency", "Run end-to-end latency test")
{}

boost::property_tree::ptree
TestNPULatency::run(const std::shared_ptr<xrt_core::device>&)
{
  boost::property_tree::ptree ptree = get_test_header();
  return ptree;
}

boost::property_tree::ptree
TestNPULatency::run(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive)
{
  boost::property_tree::ptree ptree = get_test_header();
  
  if (archive == nullptr) {
    XBValidateUtils::logger(ptree, "Info", "No archive provided, using standard latency test");
    return ptree;
  }
  
  try {
    std::string recipe_data = archive->data("recipe_latency.json");
    std::string profile_data = archive->data("profile_latency.json"); 
    
    xrt_core::runner::artifacts_repository artifacts_repo;
    
    std::vector<std::string> artifact_names = {
      "validate.xclbin", 
      "nop.elf" 
    };
    
    // Extract available artifacts from archive into repository
    for (const auto& artifact_name : artifact_names) {
      try {
        std::string artifact_data = archive->data(artifact_name);
        
        // Convert string to vector<char> for artifacts_repository
        std::vector<char> artifact_binary(artifact_data.begin(), artifact_data.end());
        artifacts_repo[artifact_name] = std::move(artifact_binary);
      }
      catch (const std::exception&) {
        XBValidateUtils::logger(ptree, "Error", boost::str(boost::format("Required artifact not found: %s") % artifact_name));
      }
    }
    
    // Create runner with recipe, profile, and artifacts repository
    xrt_core::runner runner(xrt::device(dev), recipe_data, profile_data, artifacts_repo);
    runner.execute();
    runner.wait();

    auto report = json::parse(runner.get_report());
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average latency: %.1f us") % report["cpu"]["latency"].get<double>()));
    ptree.put("status", XBValidateUtils::test_token_passed);
  }
  catch(const std::exception& e) {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }
  return ptree;
}
