// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files

#include "TestShimDMABW.h"
#include "TestValidateUtilities.h"
#include "core/common/runner/runner.h"
#include "xrt/xrt_device.h"
#include "core/common/json/nlohmann/json.hpp"
#include "tools/common/XBUtilities.h"
#include "core/common/archive.h"

using json = nlohmann::json;
#include <filesystem>

// ----- C L A S S   M E T H O D S -------------------------------------------
TestShimDMABW::TestShimDMABW()
  : TestRunner("shim-dma-bw", "Run 2xRead/1xWrite bandwidth test for SHIM DMA")
  , m_test_configs({
      {"recipe_bw_2r_1w.json",  "profile_bw_2r_1w.json",  "bw_2r_1w.elf",  18, "2xRead/1xWrite"} //NOLINT(cppcoreguidelines-avoid-magic-numbers)
    })
{}

boost::property_tree::ptree
TestShimDMABW::run(const std::shared_ptr<xrt_core::device>&)
{
  boost::property_tree::ptree ptree = get_test_header();
  return ptree;
}

void
TestShimDMABW::run_flavors(boost::property_tree::ptree& test, const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive,
                           const std::vector<FlavorConfig>& test_configs)
{
  for (const auto& config : test_configs) {
    std::string recipe_data = archive->data(config.recipe);
    std::string profile_data = archive->data(config.profile);

    // Extract artifacts using helper method
    auto artifacts_repo = XBUtilities::extract_artifacts_from_archive(archive, {config.elf});

    // Create runner with recipe, profile, and artifacts repository
    xrt_core::runner runner(xrt::device(dev), recipe_data, profile_data, artifacts_repo);
    runner.execute();
    runner.wait();

    auto report = json::parse(runner.get_report());
    auto elapsed_us = report["cpu"]["elapsed"].get<double>();
    auto iterations = report["iterations"].get<int>();

    // Used buffer's size in runner is in MB, thus converting to GB/s
    double bandwidth = (config.size * iterations) / ((elapsed_us / 1000000) * 1000); // NOLINT: Runner reports in microseconds, so conversion is required until request supports timescales

    XBValidateUtils::logger(test, "Details", boost::str(boost::format("Average bandwidth (%s): %.1f GB/s") % config.flavor % bandwidth));
  }
}

boost::property_tree::ptree
TestShimDMABW::run(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive)
{
  boost::property_tree::ptree ptree = get_test_header();

  if (archive == nullptr) {
    ptree.put("status", XBValidateUtils::test_token_failed);
    XBValidateUtils::logger(ptree, "Error", "No archive found, skipping test");
    return ptree;
  }

  try {
    run_flavors(ptree, dev, archive, m_test_configs);
  } catch(const std::exception& e) {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }

  ptree.put("status", XBValidateUtils::test_token_passed);
  return ptree;
}
