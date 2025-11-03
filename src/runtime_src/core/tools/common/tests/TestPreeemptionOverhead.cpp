// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestPreemptionOverhead.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "core/common/unistd.h"
#include "xrt/xrt_device.h"
#include "core/common/runner/runner.h"
#include "core/common/json/nlohmann/json.hpp"
#include "core/common/archive.h"

namespace XBU = XBValidateUtils;
namespace xq = xrt_core::query;
using json = nlohmann::json;

// System - Include Files
#include <filesystem>

static constexpr uint32_t num_of_preemptions = 500;

// ----- S T A T I C   M E T H O D S -------------------------------------------



static double
measure_preemption_overhead(const std::shared_ptr<xrt_core::device>& dev,
                            const std::string& recipe_data,
                            const std::string& profile_data,
                            const xrt_core::runner::artifacts_repository& artifacts_repo)
{
  // Run the test and return the execution time
  auto measure_exec_time = [&]() -> double {
    xrt_core::runner runner(xrt::device(dev), recipe_data, profile_data, artifacts_repo);
    runner.execute();
    runner.wait();
    auto report = json::parse(runner.get_report());
    return report["cpu"]["elapsed"].get<double>() / report["iterations"].get<int>();
  };

  // Run without preemption
  xrt_core::device_update<xq::preemption>(dev.get(), static_cast<uint32_t>(0));
  auto baseline_exec_time = measure_exec_time();

  // Run with preemption enabled
  xrt_core::device_update<xq::preemption>(dev.get(), static_cast<uint32_t>(1));
  
  auto preempt_exec_time = measure_exec_time();

  // Calculate and return overhead per preemption
  return (preempt_exec_time - baseline_exec_time) / num_of_preemptions;
}

TestPreemptionOverhead::TestPreemptionOverhead()
  : TestRunner("preemption-overhead", "Measure preemption overhead at noop and memtile levels")
{}

boost::property_tree::ptree
TestPreemptionOverhead::run(const std::shared_ptr<xrt_core::device>&)
{
  boost::property_tree::ptree ptree = get_test_header();
  return ptree;
}

boost::property_tree::ptree
TestPreemptionOverhead::run(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.erase("xclbin");

  // this test is only for privileged users as it requires enabling/disabling preemption
  if(!xrt_core::is_user_privileged()) {
    XBU::logger(ptree, "Details", "This test requires admin privileges");
    ptree.put("status", XBU::test_token_skipped);
    return ptree;
  }

  if (archive == nullptr) {
    XBU::logger(ptree, "Info", "No archive found, skipping test");
    ptree.put("status", XBU::test_token_skipped);
    return ptree;
  }
  
  const auto layer_boundary = xrt_core::device_query_default<xq::preemption>(dev.get(), 0);

  try {
    std::string recipe_4x4_noop_data = archive->data("recipe_preemption_noop_4x4.json");
    std::string recipe_4x8_noop_data = archive->data("recipe_preemption_noop_4x8.json");
    std::string recipe_4x4_memtile_data = archive->data("recipe_preemption_memtile_4x4.json");
    std::string recipe_4x8_memtile_data = archive->data("recipe_preemption_memtile_4x8.json");
    std::string profile_data = archive->data("profile_preemption.json"); 

    
    auto artifacts_repo = XBUtilities::extract_artifacts_from_archive(archive, {
      "preemption_4x4.xclbin",
      "preemption_4x8.xclbin",
      "preemption_noop_4x4.elf",
      "preemption_noop_4x8.elf",
      "preemption_memtile_4x4.elf",
      "preemption_memtile_4x8.elf",
    });

    // Measure preemption overhead for all 4 recipes
    auto overhead_noop_4x4 = measure_preemption_overhead(dev, recipe_4x4_noop_data, profile_data, artifacts_repo);
    XBU::logger(ptree, "Details", boost::str(boost::format("Average %s preemption overhead for 4x%d is %.1f us") % "noop" % 4 % overhead_noop_4x4));

    auto overhead_noop_4x8 = measure_preemption_overhead(dev, recipe_4x8_noop_data, profile_data, artifacts_repo);
    XBU::logger(ptree, "Details", boost::str(boost::format("Average %s preemption overhead for 4x%d is %.1f us") % "noop" % 8 % overhead_noop_4x8));

    auto overhead_memtile_4x4 = measure_preemption_overhead(dev, recipe_4x4_memtile_data, profile_data, artifacts_repo);
    XBU::logger(ptree, "Details", boost::str(boost::format("Average %s preemption overhead for 4x%d is %.1f us") % "memtile" % 4 % overhead_memtile_4x4));

    auto overhead_memtile_4x8 = measure_preemption_overhead(dev, recipe_4x8_memtile_data, profile_data, artifacts_repo);
    XBU::logger(ptree, "Details", boost::str(boost::format("Average %s preemption overhead for 4x%d is %.1f us") % "memtile" % 8 % overhead_memtile_4x8));
  }
  catch(const std::exception& e) {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
  }

  // Restore the original preemption state
  xrt_core::device_update<xq::preemption>(dev.get(), static_cast<uint32_t>(layer_boundary));

  ptree.put("status", XBU::test_token_passed);
  
  return ptree;
}
