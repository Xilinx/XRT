// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestGemm.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#include "core/common/api/bo_int.h"

namespace XBU = XBUtilities;

// System - Include Files
#include <fstream>
#include <filesystem>
#include <thread>

static constexpr size_t host_app = 1; //opcode
static constexpr uint32_t num_of_cores = 32;

/*
* Total OPs= = 196K OPs.
*/
static constexpr uint32_t total_ops = 196608; //192K OPs

// ----- C L A S S   M E T H O D S -------------------------------------------
TestGemm::TestGemm()
  : TestRunner("gemm", "Measure the TOPS value of GEMM operations")
{}

boost::property_tree::ptree
TestGemm::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.erase("xclbin");

  const auto xclbin_name = xrt_core::device_query<xrt_core::query::xclbin_name>(dev, xrt_core::query::xclbin_name::type::gemm);
  auto xclbin_path = XBValidateUtils::findPlatformFile(xclbin_name, ptree);
  if (!std::filesystem::exists(xclbin_path)){
    XBValidateUtils::logger(ptree, "Details", "The test is not supported on this device.");
    return ptree;
  }

  xrt::xclbin xclbin;
  try {
    xclbin = xrt::xclbin(xclbin_path);
  }
  catch (const std::runtime_error& ex) {
    XBValidateUtils::logger(ptree, "Error", ex.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }

  // Determine The DPU Kernel Name
  auto xkernels = xclbin.get_kernels();

  auto itr = std::find_if(xkernels.begin(), xkernels.end(), [](xrt::xclbin::kernel& k) {
    auto name = k.get_name();
    return name.rfind("DPU",0) == 0; // Starts with "DPU"
  });

  xrt::xclbin::kernel xkernel;
  if (itr!=xkernels.end())
    xkernel = *itr;
  else {
    XBValidateUtils::logger(ptree, "Error", "No kernel with `DPU` found in the xclbin");
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }
  auto kernelName = xkernel.get_name();

  auto working_dev = xrt::device(dev);
  working_dev.register_xclbin(xclbin);

  xrt::hw_context hwctx;
  xrt::kernel kernel;
  try {
    hwctx = xrt::hw_context(working_dev, xclbin.get_uuid());
    kernel = xrt::kernel(hwctx, kernelName);
  } 
  catch (const std::exception& )
  {
    XBValidateUtils::logger (ptree, "Error", "Not enough columns available. Please make sure no other workload is running on the device.");
    ptree.put("status", XBValidateUtils::test_token_failed);ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }

  const auto seq_name = xrt_core::device_query<xrt_core::query::sequence_name>(dev, xrt_core::query::sequence_name::type::gemm_int8);
  auto dpu_instr = XBValidateUtils::findPlatformFile(seq_name, ptree);
  if (!std::filesystem::exists(dpu_instr))
    return ptree;

  size_t instr_size = 0;
  try {
    instr_size = XBValidateUtils::get_instr_size(dpu_instr); 
  }
  catch(const std::exception& ex) {
    XBValidateUtils::logger(ptree, "Error", ex.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }

  //Create Instruction BO
  xrt::bo bo_instr(working_dev, instr_size*sizeof(int), XCL_BO_FLAGS_CACHEABLE, kernel.group_id(5));
  XBValidateUtils::init_instr_buf(bo_instr, dpu_instr);
  //Sync Instruction BO
  bo_instr.sync(XCL_BO_SYNC_BO_TO_DEVICE);

  // Create 128KB Debug BO to capture TOPS data
  xrt::bo bo_result = xrt_core::bo_int::create_debug_bo(hwctx, 0x20000);

  try {
    //run kernel
    for(int i=0; i < 200; i++) {
      auto run = kernel(host_app, NULL, NULL, NULL, NULL, bo_instr, instr_size, NULL);
      // Wait for kernel to be done
      run.wait2();
    }
  }
  catch (const std::exception& ex) {
    XBValidateUtils::logger(ptree, "Error", ex.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }

  //map ouput buffer
  bo_result.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
  auto bo_result_map = bo_result.map<uint8_t*>();

  //Calculate TOPS
  uint64_t ipu_hclock = 0;
  auto res_info = xrt_core::device_query_default<xrt_core::query::xrt_resource_raw>(dev, {});
  for (auto &res : res_info) {
    if (res.type != xrt_core::query::xrt_resource_raw::resource_type::ipu_clk_max)
      continue;
    ipu_hclock = res.data_uint64;
  }

  if (ipu_hclock == 0) {
    XBValidateUtils::logger(ptree, "Error", "IPU H-clock is 0");
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }
  double ipu_hclck_period = 1000000000.0 / (ipu_hclock * 1000000); // MHz to ns

  uint32_t* core_ptr = reinterpret_cast<uint32_t*>(bo_result_map);
  double TOPS = 0.0;
  double total_cycle_count = 0.0;

  for (uint32_t n = 0 ; n < num_of_cores; n++) {
    auto cycle_count = *core_ptr;
    if(cycle_count == 0) {
      XBValidateUtils::logger(ptree, "Error", "cycle count is 0");
      ptree.put("status", XBValidateUtils::test_token_failed);
      return ptree;
    }
    auto temp_TOPS_per_core = total_ops/(ipu_hclck_period * cycle_count * 1000);
    total_cycle_count = total_cycle_count + cycle_count;
    TOPS = TOPS + temp_TOPS_per_core;
    core_ptr++;
  }

  if(XBU::getVerbose()) {
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Total Duration: %.1f ns") % (ipu_hclck_period * (total_cycle_count/num_of_cores))));
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average cycle count: %.1f") % (total_cycle_count/num_of_cores)));
  }

  XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("TOPS: %.1f") % TOPS));
  ptree.put("status", XBValidateUtils::test_token_passed);

  return ptree;
}
