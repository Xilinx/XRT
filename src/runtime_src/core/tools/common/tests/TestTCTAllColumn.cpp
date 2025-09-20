// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestTCTAllColumn.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_device.h"
#include "core/common/runner/runner.h"
#include "core/common/json/nlohmann/json.hpp"
#include "core/common/archive.h"

// System - Include Files
#include <filesystem>

using json = nlohmann::json;
namespace XBU = XBUtilities;

/* This host application measures the average TCT latency and TCT throughput 
 * for all columns tests.
 * The ELF code loopbacks the small chunk of input data from DDR through 
 * a AIE MM2S Shim DMA channel back to DDR through a S2MM Shim DMA channel.
 * TCT is used for dma transfer completion. Host app measures the time for
 * predefined number of Tokens and calculate the latency and throughput.
 */

//Number of Sample Tokens to measure the throughtput. 
//This is an assumption coming from elf code running on the device.
static constexpr int samples = 20000;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestTCTAllColumn::TestTCTAllColumn()
  : TestRunner("tct-all-col", "Measure average TCT processing time for all columns")
{}

boost::property_tree::ptree
TestTCTAllColumn::run(const std::shared_ptr<xrt_core::device>&)
{
  boost::property_tree::ptree ptree = get_test_header();
  return ptree;
}

boost::property_tree::ptree
TestTCTAllColumn::run(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive)
{
  boost::property_tree::ptree ptree = get_test_header();
  
  try {
    std::string recipe_data = archive->data("recipe_tct_all_column.json");
    std::string profile_data = archive->data("profile_tct_all_column.json"); 
    
    std::vector<std::string> artifact_names = {
      "tct_all_col.xclbin", 
      "tct_4col.elf" 
    };
    
    // Extract artifacts using helper method
    auto artifacts_repo = extract_artifacts_from_archive(archive, artifact_names, ptree);
    
    // Create runner with archive data
    xrt_core::runner runner(xrt::device(dev), recipe_data, profile_data, artifacts_repo);
    
    runner.execute();
    runner.wait();

    // Get final metrics from the last run 
    auto report = json::parse(runner.get_report());
    double latency = report["cpu"]["latency"].get<double>(); // Should be in microseconds
    double throughput = report["cpu"]["throughput"].get<double>(); // Should be in operations/second

    if(XBU::getVerbose())
      XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average time for TCT (all columns): %.1f us") % (latency/samples)));
    
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average TCT throughput (all columns): %.1f TCT/s") % (samples * throughput)));
    ptree.put("status", XBValidateUtils::test_token_passed);
  }
  catch(const std::exception& e) {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
  }
  
  return ptree;
}
