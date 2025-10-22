// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestTemporalSharingOvd.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "core/common/runner/runner.h"
#include "core/common/json/nlohmann/json.hpp"
#include "core/common/archive.h"
#include <thread>

using json = nlohmann::json;
namespace XBU = XBUtilities;

boost::property_tree::ptree TestTemporalSharingOvd::run(const std::shared_ptr<xrt_core::device>&)
{
  ptree.erase("xclbin");
  return ptree;
}

boost::property_tree::ptree TestTemporalSharingOvd::run(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive)
{
  boost::property_tree::ptree ptree = get_test_header();
  
  if (archive == nullptr) {
    XBValidateUtils::logger(ptree, "Info", "No archive provided, falling back to standard method");
    return run(dev);
  }
  
  try {
    std::string recipe_data = archive->data("recipe_temporal_sharing_ovd.json");
    std::string profile_data = archive->data("profile_temporal_sharing_ovd.json"); 
    
    auto artifacts_repo = XBUtilities::extract_artifacts_from_archive(archive, {
      "validate.xclbin", 
      "nop.elf" 
    });
    
    // Run parallel temporal sharing test (2 contexts)
    std::vector<std::thread> threads;
    std::vector<std::unique_ptr<xrt_core::runner>> runners;
    std::vector<json> reports;
    
    // Create 2 runners for parallel execution
    for (int i = 0; i < 2; i++) {
      runners.push_back(std::make_unique<xrt_core::runner>(xrt::device(dev), recipe_data, profile_data, artifacts_repo));
    }
    
    // Execute both runners in parallel threads
    reports.resize(2);
    for (int i = 0; i < 2; i++) {
      threads.emplace_back([&runners, &reports, i]() {
        runners[i]->execute();
        runners[i]->wait();
        reports[i] = json::parse(runners[i]->get_report());
      });
    }
    
    // Wait for both to complete
    for (auto& t : threads) {
      t.join();
    }
    
    // Extract elapsed time from parallel execution reports
    double latencyShared = 0.0;
    for (const auto& report : reports) {
      latencyShared = std::max(latencyShared, report["cpu"]["elapsed"].get<double>());
    }
    
    threads.clear();
    runners.clear();
    
    // Run sequential test (1 context)
    xrt_core::runner sequential_runner(xrt::device(dev), recipe_data, profile_data, artifacts_repo);
    sequential_runner.execute();
    sequential_runner.wait();
    
    auto sequential_report = json::parse(sequential_runner.get_report());
    
    // Extract elapsed time from sequential execution report
    double latencySingle = 0.0;
    latencySingle = sequential_report["cpu"]["elapsed"].get<double>();
    
    // Calculate overhead using the same formula as original test
    int iterations = sequential_report["iterations"];
    int runs = sequential_report["resources"]["runs"];
    auto overhead = (latencyShared - 2 * latencySingle) / (iterations * runs);
    
    // Log results
    if(XBU::getVerbose()){
      XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Single context duration: %.1f us") % latencySingle));
      XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Temporally shared multiple context duration: %.1f us") % latencyShared));
    }
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Overhead archive: %.1f us") % (overhead > 0.0 ? overhead : 0.0)));
    ptree.put("status", XBValidateUtils::test_token_passed);
  }
  catch(const std::exception& e) {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
  }
  return ptree;
}
