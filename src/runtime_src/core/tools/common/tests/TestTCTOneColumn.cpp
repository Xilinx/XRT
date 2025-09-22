// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestTCTOneColumn.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_device.h"
#include "core/common/runner/runner.h"
#include "core/common/json/nlohmann/json.hpp"

// System - Include Files
#include <filesystem>

using json = nlohmann::json;
namespace XBU = XBUtilities;

/* This host application measures the average TCT latency and TCT throughput 
 * for one columns tests.
 * The ELF code loopbacks the small chunk of input data from DDR through 
 * a AIE MM2S Shim DMA channel back to DDR through a S2MM Shim DMA channel.
 * TCT is used for dma transfer completion. Host app measures the time for
 * predefined number of Tokens and calculate the latency and throughput.
 */

//This is an assumption coming from elf code running on the device.
static constexpr int samples = 10000;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestTCTOneColumn::TestTCTOneColumn()
  : TestRunner("tct-one-col", "Measure average TCT processing time for one column")
{}

boost::property_tree::ptree
TestTCTOneColumn::run(const std::shared_ptr<xrt_core::device>& dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  std::string recipe = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::tct_one_column_recipe);
  std::string profile = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::tct_one_column_profile);
  std::string test = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::tct_one_column_path);
  auto recipe_path = XBValidateUtils::findPlatformFile(recipe, ptree);
  auto profile_path = XBValidateUtils::findPlatformFile(profile, ptree);
  auto test_path = XBValidateUtils::findPlatformFile(test, ptree);

  try
  {
    // Create runner once 
    xrt_core::runner runner(xrt::device(dev), recipe_path, profile_path, std::filesystem::path(test_path));
    
    runner.execute();
    runner.wait();

    // Get final metrics from the last run 
    auto report = json::parse(runner.get_report());
    double latency = report["cpu"]["latency"].get<double>(); // Should be in microseconds
    double throughput = report["cpu"]["throughput"].get<double>(); // Should be in operations/second

    if(XBU::getVerbose())
      XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average time for TCT: %.1f us") % (latency / samples)));

    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average TCT throughput: %.1f TCT/s") % (samples * throughput)));
    ptree.put("status", XBValidateUtils::test_token_passed);
  }
  catch(const std::exception& e)
  {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
  }

  return ptree;
}
