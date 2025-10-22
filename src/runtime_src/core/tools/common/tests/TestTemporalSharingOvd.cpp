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
#include "core/common/info_telemetry.h"
#include <thread>

using json = nlohmann::json;
namespace XBU = XBUtilities;

static uint64_t
get_total_frame_events(const std::shared_ptr<xrt_core::device>& dev)
{
  uint64_t total_frame_events = 0;
  try {
    auto telemetry_pt = xrt_core::telemetry::preemption_telemetry_info(dev.get());
    auto telemetry_array = telemetry_pt.get_child("telemetry");
    
    for (const auto& [name, user_task] : telemetry_array) {
      std::string frame_events_str = user_task.get<std::string>("frame_events");
      if (frame_events_str != "N/A") {
        total_frame_events += std::stoull(frame_events_str);
      }
    }
  } catch (const std::exception&) {
    // If telemetry is not available, return 0
    total_frame_events = 0;
  }
  return total_frame_events;
}

boost::property_tree::ptree TestTemporalSharingOvd::run(const std::shared_ptr<xrt_core::device>&)
{
  boost::property_tree::ptree ptree = get_test_header();
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
      "gemm.xclbin", 
      "gemm.elf" 
    });
    
    // Run parallel temporal sharing test (2 contexts)
    std::vector<std::thread> threads;
    std::vector<std::unique_ptr<xrt_core::runner>> runners;
    std::vector<json> reports;
    
    // Get initial frame event count
    uint64_t initial_frame_events = get_total_frame_events(dev);
    
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
    
    // Get frame event count after parallel execution
    uint64_t final_frame_events = get_total_frame_events(dev);
    
    // Extract elapsed time from parallel execution reports
    double latencyShared = 0.0;
    for (const auto& full_report : reports) {
      const auto& exec_report = full_report["executions"][0];
      latencyShared = std::max(latencyShared, exec_report["cpu"]["elapsed"].get<double>());
    }
    
    threads.clear();
    runners.clear();
    
    // Run sequential test (1 context)
    xrt_core::runner sequential_runner(xrt::device(dev), recipe_data, profile_data, artifacts_repo);
    sequential_runner.execute();
    sequential_runner.wait();
    
    auto full_report = json::parse(sequential_runner.get_report());
    const auto& sequential_report = full_report["executions"][0];
    
    // Extract elapsed time from sequential execution report
    double latencySingle = sequential_report["cpu"]["elapsed"].get<double>();
    
    uint64_t frame_events_diff = final_frame_events - initial_frame_events;
    double overhead = 0.0;
    
    overhead = (latencyShared - 2 * latencySingle) / (frame_events_diff);
    
    // Log results
    if(XBU::getVerbose()){
      XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Single context duration: %.1f us") % latencySingle));
      XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Temporally shared multiple context duration: %.1f us") % latencyShared));
      XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Frame events difference: %d") % frame_events_diff));
    }
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Overhead: %.1f us") % overhead));
    ptree.put("status", XBValidateUtils::test_token_passed);
  }
  catch(const std::exception& e) {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
  }
  return ptree;
}
