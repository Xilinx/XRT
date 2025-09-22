// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestGemm.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_device.h"
#include "core/common/runner/runner.h"

namespace XBU = XBUtilities;

static constexpr uint32_t num_of_cores = 32;
/*
* Total OPs= = 196K OPs.
*/
static constexpr uint32_t total_ops = 196608; //192K OPs

// ----- C L A S S   M E T H O D S -------------------------------------------
TestGemm::TestGemm()
  : TestRunner("gemm", "Measure the TOPS value of GEMM INT8 operations")
{}

boost::property_tree::ptree
TestGemm::run(const std::shared_ptr<xrt_core::device>& dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  std::string recipe = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::gemm_recipe);
  std::string profile = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::gemm_profile);
  std::string test = xrt_core::device_query<xrt_core::query::runner>(dev, xrt_core::query::runner::type::gemm_path);
  auto recipe_path = XBValidateUtils::findPlatformFile(recipe, ptree);
  auto profile_path = XBValidateUtils::findPlatformFile(profile, ptree);
  auto test_path = XBValidateUtils::findPlatformFile(test, ptree);
  try
  {
    xrt_core::runner runner(xrt::device(dev), recipe_path, profile_path, std::filesystem::path(test_path));
    runner.execute();
    runner.wait();
    const auto bo_result_map = runner.map_buffer("bo_result");

    //Calculate TOPS
    uint64_t npu_hclock = 0;
    auto res_info = xrt_core::device_query_default<xrt_core::query::xrt_resource_raw>(dev, {});
    for (auto &res : res_info) {
      if (res.type != xrt_core::query::xrt_resource_raw::resource_type::npu_clk_max)
        continue;
      npu_hclock = res.data_uint64;
    }

    if (npu_hclock == 0) {
      XBValidateUtils::logger(ptree, "Error", "NPU H-clock is 0");
      ptree.put("status", XBValidateUtils::test_token_failed);
      return ptree;
    }
    double npu_hclck_period = 1000000000.0 / (npu_hclock * 1000000); // NOLINT MHz to ns

    const auto* core_ptr = reinterpret_cast<const uint32_t*>(bo_result_map.data());
    double TOPS = 0.0;
    double total_cycle_count = 0.0;

    for (uint32_t n = 0 ; n < num_of_cores; n++) {
      auto cycle_count = *core_ptr;
      if(cycle_count == 0) {
        XBValidateUtils::logger(ptree, "Error", "cycle count is 0");
        ptree.put("status", XBValidateUtils::test_token_failed);
        return ptree;
      }
      auto temp_TOPS_per_core = total_ops/(npu_hclck_period * cycle_count * 1000); // NOLINT 
      total_cycle_count = total_cycle_count + cycle_count;
      TOPS = TOPS + temp_TOPS_per_core;
      core_ptr++;
    }

    if(XBU::getVerbose()) {
      XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Total Duration: %.1f ns") % (npu_hclck_period * (total_cycle_count/num_of_cores))));
      XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average cycle count: %.1f") % (total_cycle_count/num_of_cores)));
    }

    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("TOPS: %.1f") % TOPS));
    ptree.put("status", XBValidateUtils::test_token_passed);
  }
  catch(const std::exception& e)
  {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }
  //map ouput buffer
  

  return ptree;
}
