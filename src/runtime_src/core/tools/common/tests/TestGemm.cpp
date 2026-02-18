// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestGemm.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/experimental/xrt_aie.h"
#include "xrt/experimental/xrt_ext.h"
#include "core/common/runner/runner.h"
#include "core/common/archive.h"
#include "core/common/smi.h"
#include "core/common/api/bo_int.h"
#include "xrt/detail/xclbin.h"

#include <stdexcept>

namespace XBU = XBUtilities;

static constexpr uint32_t num_of_cores_strix = 32;
constexpr size_t num_of_cores_npu3 = 12;
static constexpr uint32_t total_ops_strix = 196608; //192K OPs
constexpr uint64_t total_ops_npu3 = 2097152; // 2,097,152 OPs

/**
 * Get GEMM clock frequency in MHz for TOPS calculation from clock topology.
 * Strix: H Clock. NPU3: AIE Clock.
 */
static uint64_t
get_clock(const std::shared_ptr<xrt_core::device>& dev)
{
  uint64_t npu_hclock = 0;
  auto res_info = xrt_core::device_query_default<xrt_core::query::xrt_resource_raw>(dev, {});
  for (auto &res : res_info) {
    if (res.type != xrt_core::query::xrt_resource_raw::resource_type::npu_clk_max)
      continue;
    npu_hclock = res.data_uint64;
  } 
  return npu_hclock;
}

/** Clock period in ns from frequency in MHz. */
static double
clock_period_ns(uint64_t clock_mhz)
{
  return 1000000000.0 / (clock_mhz * 1000000); // NOLINT MHz to ns
}

/** Log GEMM results and optional verbose details. */
static void
log_gemm_results(boost::property_tree::ptree& ptree,
                 double tops,
                 double avg_cycle_count,
                 double period_ns)
{
  if (XBU::getVerbose()) {
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Total Duration (avg): %.1f ns") % (period_ns * avg_cycle_count)));
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average cycle count: %.1f") % avg_cycle_count));
  }
  XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("TOPS: %.1f") % tops));
}

// ----- S T A T I C   M E T H O D S -------------------------------------------
void static
run_strix(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive, boost::property_tree::ptree& ptree) {
  try {
    std::string recipe_data = archive->data("recipe_gemm.json");
    std::string profile_data = archive->data("profile_gemm.json"); 
    
    // Extract artifacts using helper method
    auto artifacts_repo = XBU::extract_artifacts_from_archive(archive, {
      "gemm.xclbin", 
      "gemm.elf" 
    });
    
    // Create runner with recipe, profile, and artifacts repository
    xrt_core::runner runner(xrt::device(dev), recipe_data, profile_data, artifacts_repo);

    uint64_t clock_mhz = get_clock(dev);
    double period_ns = clock_period_ns(clock_mhz);

    runner.execute();
    runner.wait();
    const auto bo_result_map = runner.map_buffer("bo_result");

    const auto* core_ptr = reinterpret_cast<const uint32_t*>(bo_result_map.data());
    double run_TOPS = 0.0;
    double run_total_cycle_count = 0.0;

    for (uint32_t n = 0; n < num_of_cores_strix; n++) {
      auto cycle_count = *core_ptr;
      if (cycle_count == 0) {
        XBValidateUtils::logger(ptree, "Error", "cycle count is 0");
        ptree.put("status", XBValidateUtils::test_token_failed);
        return;
      }
      auto temp_TOPS_per_core = total_ops_strix / (period_ns * cycle_count * 1000); // NOLINT
      run_total_cycle_count += cycle_count;
      run_TOPS += temp_TOPS_per_core;
      core_ptr++;
    }

    double TOPS = run_TOPS;
    double avg_cycle_count = run_total_cycle_count / num_of_cores_strix;

    log_gemm_results(ptree, TOPS, avg_cycle_count, period_ns);
    ptree.put("status", XBValidateUtils::test_token_passed);
  }
  catch(const std::exception& e) {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
  }
}

void static
run_npu3(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive, boost::property_tree::ptree& ptree) {
  try {
    // Extract gemm.elf from archive
    auto artifacts_repo = XBU::extract_artifacts_from_archive(archive, {
      "gemm.elf" 
    });
    xrt::device working_dev(dev);

    // Get the ELF data from artifacts repository
    const auto& elf_data = artifacts_repo.at("gemm.elf");
    
    // Create program from ELF data (full ELF flow)
    std::string_view elf_view(elf_data.data(), elf_data.size());
    xrt::aie::program program(elf_view);

    // Create hw_context with program (shared access mode for full ELF flow)
    xrt::hw_context hwctx(working_dev, program, {}, xrt::hw_context::access_mode::shared);

    xrt::ext::kernel kernel(hwctx, "DPU");
    xrt::run run {kernel};

    //Configure the debug buffer
    std::map<uint32_t, size_t> buf_map; //uc index, size of buffer
    buf_map[0] = num_of_cores_npu3 * 2 * sizeof(uint32_t); //registers are populated as core_num and cycle_count (24 reads)
    auto bo = xrt_core::bo_int::create_bo(hwctx, num_of_cores_npu3 * sizeof(uint32_t), xrt_core::bo_int::use_type::uc_debug);
    char* buf_result = bo.map<char*>();
    memset(buf_result, 0, num_of_cores_npu3);

    xrt_core::bo_int::config_bo(bo, buf_map);

    uint64_t clock_mhz = get_clock(dev);
    double period_ns = clock_period_ns(clock_mhz);

    for (int i = 0; i < 100; i++) { // NOLINT
      run.start();
      run.wait2();
    }

    bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    double total_cycle_count = 0.0;
    for (uint8_t i = 1; i <= num_of_cores_npu3 * 2; i = i + 2) { // read every other register
      uint32_t cycle_count = *reinterpret_cast<uint32_t*>(buf_result + i * sizeof(uint32_t));
      total_cycle_count += cycle_count;
    }

    double cycle_count_per_core = total_cycle_count / num_of_cores_npu3;
    double gops_per_core = (static_cast<double>(total_ops_npu3) * 1e9) / (cycle_count_per_core * period_ns); // NOLINT
    double tops_per_core = gops_per_core / 1e12; // NOLINT
    double aie4_tops_all_cores = tops_per_core * num_of_cores_npu3;
    double avg_cycle_count = total_cycle_count / num_of_cores_npu3;

    log_gemm_results(ptree, aie4_tops_all_cores, avg_cycle_count, period_ns);
    ptree.put("status", XBValidateUtils::test_token_passed);
  }
  catch(const std::exception& e) {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
  }
}

// ----- C L A S S   M E T H O D S -------------------------------------------
TestGemm::TestGemm()
  : TestRunner("gemm", "Measure the TOPS value of GEMM INT8 operations")
{}

boost::property_tree::ptree
TestGemm::run(const std::shared_ptr<xrt_core::device>&)
{
  boost::property_tree::ptree ptree = get_test_header();
  return ptree;
}

boost::property_tree::ptree
TestGemm::run(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive)
{
  boost::property_tree::ptree ptree = get_test_header();
  
  if (archive == nullptr) {
    ptree.put("status", XBValidateUtils::test_token_failed);
    XBValidateUtils::logger(ptree, "Error", "No archive provided, skipping test");
    return ptree;
  }

  // Determine the hardware type
  using query = xrt_core::query::pcie_id;
  auto pcie_id = xrt_core::device_query<query>(dev);

  xrt_core::smi::smi_hardware_config smi_hrdw;
  auto hardware_type = smi_hrdw.get_hardware_type(pcie_id);

  if (XBUtilities::is_strix_hardware(hardware_type))
    run_strix(dev, archive, ptree);
  else
    run_npu3(dev, archive, ptree);
  
  
  return ptree;
}
