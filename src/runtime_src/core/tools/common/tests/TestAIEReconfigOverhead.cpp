// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

#include "TestAIEReconfigOverhead.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_kernel.h"
namespace XBU = XBUtilities;

// System - Include Files
#include <fstream>
#include <filesystem>
#include <thread>
#include <iostream>
static constexpr size_t buffer_size_mb = 128;
static constexpr size_t buffer_size = buffer_size_mb * 1024 * 1024; //128 MB
static constexpr size_t word_count = buffer_size/16;
static constexpr int itr_count = 1000;
static constexpr size_t inter_size = 1024 * 1024;
static constexpr unsigned int StartAddr = 32 * 1024 * 1024;

TestAIEReconfigOverhead::TestAIEReconfigOverhead()
  : TestRunner("aie-reconfig-overhead", "Run end-to-end array reconfiguration overhead through shim DMA")
{}

boost::property_tree::ptree
TestAIEReconfigOverhead::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.erase("xclbin");

  std::string xclbin_path = XBValidateUtils::get_xclbin_path(dev, xrt_core::query::xclbin_name::type::validate_elf, ptree);

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
  auto kernelName = XBValidateUtils::get_kernel_name(xclbin, ptree);

  auto working_dev = xrt::device(dev);
  working_dev.register_xclbin(xclbin);


  xrt::hw_context hwctx;
  xrt::kernel kernel, kernel_no_op;

  const auto elf_name = xrt_core::device_query<xrt_core::query::elf_name>(dev, xrt_core::query::elf_name::type::aie_reconfig_overhead);
  auto elf_path = XBValidateUtils::findPlatformFile(elf_name, ptree);
  
  if (!std::filesystem::exists(elf_path))
    return ptree;
  
  try {
    hwctx = xrt::hw_context(working_dev, xclbin.get_uuid());
    kernel = get_kernel(hwctx, kernelName, elf_path);
    kernel_no_op = get_kernel(hwctx, kernelName); 
  } 
  catch (const std::exception& )
  {
    XBValidateUtils::logger (ptree, "Error", "Not enough columns available. Please make sure no other workload is running on the device.");
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }

  //Create BOs
  xrt::bo bo_ifm, bo_ofm, bo_inter, bo_instr, bo_instr_no_op;
  
  bo_ifm = xrt::ext::bo{working_dev, buffer_size};
  bo_ofm = xrt::ext::bo{working_dev, buffer_size};
  bo_inter = xrt::ext::bo{working_dev, inter_size};

  // map input buffer
  // Incremental byte pattern
  auto ifm_mapped = bo_ifm.map<int*>();
  for (size_t i = 0; i < word_count; i++)
    ifm_mapped[i] = (int)(i % word_count);

  //Sync BOs
  bo_ifm.sync(XCL_BO_SYNC_BO_TO_DEVICE);

  //Log
  if(XBU::getVerbose()) { 
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Buffer size: %f MB") % buffer_size_mb));
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("No. of iterations: %f") % itr_count));
  }

  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0 ;i < itr_count ; i++){
    try{
      xrt::run run;
      run = kernel_no_op(3, 0, 0, bo_ifm, 0, bo_ofm, bo_inter, 0);

      // Wait for kernel to be done
      run.wait2();
    }
    catch (const std::exception& ex)
    {
      XBValidateUtils::logger(ptree, "Error", ex.what());
      ptree.put("status", XBValidateUtils::test_token_failed);
      return ptree;
    }
    bo_ofm.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
  }
  auto end = std::chrono::high_resolution_clock::now();
  float elapsedSecsNoOpAverage = std::chrono::duration_cast<std::chrono::duration<float>>(end-start).count();
  elapsedSecsNoOpAverage /= itr_count;

  start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i< itr_count; i++)
  {
    try{
      xrt::run run;
      run = kernel(3, 0, 0, bo_ifm, 0, bo_ofm, bo_inter, 0);
      // Wait for kernel to be done
      run.wait2();
    }
    catch (const std::exception& ex)
    {
      XBValidateUtils::logger(ptree, "Error", ex.what());
      ptree.put("status", XBValidateUtils::test_token_failed);
      return ptree;
    }
    bo_ofm.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    auto *ofm_mapped = bo_ofm.map<int8_t*>();
    if(std::memcmp(ifm_mapped, ofm_mapped + StartAddr, word_count)){
      XBValidateUtils::logger(ptree, "Error", "Value read back does not match reference for array reconfiguration instruction buffer");
      ptree.put("status", XBValidateUtils::test_token_failed);
      return ptree;
    }
  }

  end = std::chrono::high_resolution_clock::now();
  auto elapsedSecsAverage = std::chrono::duration_cast<std::chrono::duration<double>>(end-start).count();
  elapsedSecsAverage /= itr_count;
  double overhead = (elapsedSecsAverage - elapsedSecsNoOpAverage)*1000; //in ms

  XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Array reconfiguration overhead: %.1f ms") % overhead));
  ptree.put("status", XBValidateUtils::test_token_passed);
  return ptree;
}
