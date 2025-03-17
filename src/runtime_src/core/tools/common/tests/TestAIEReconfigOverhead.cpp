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

  // Check Whether Use ELF or DPU Sequence
  auto elf = XBValidateUtils::getElf();
  if (!elf) 
    XBValidateUtils::logger(ptree, "Details", "Using DPU Sequence");
  else 
    XBValidateUtils::logger(ptree, "Details", "Using ELF");

  // Find xclbin File
  auto xclbin_path = XBValidateUtils::get_validate_xclbin_path(dev, elf, ptree);

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

  size_t instr_size = 0;
  std::string dpu_instr;

  xrt::hw_context hwctx;
  xrt::kernel kernel, kernel_no_op;

  if (!elf) { // DPU
    try {
      hwctx = xrt::hw_context(working_dev, xclbin.get_uuid());
      kernel = xrt::kernel(hwctx, kernelName);
    } 
    catch (const std::exception& )
    {
      XBValidateUtils::logger (ptree, "Error", "Not enough columns available. Please make sure no other workload is running on the device.");
      ptree.put("status", XBValidateUtils::test_token_failed);
      return ptree;
    }

    const auto seq_name = xrt_core::device_query<xrt_core::query::sequence_name>(dev, xrt_core::query::sequence_name::type::aie_reconfig_overhead);
    dpu_instr = XBValidateUtils::findPlatformFile(seq_name, ptree);
    if (!std::filesystem::exists(dpu_instr))
      return ptree;

    try {
      instr_size = XBValidateUtils::get_instr_size(dpu_instr); 
    }
    catch(const std::exception& ex) {
      XBValidateUtils::logger(ptree, "Error", ex.what());
      ptree.put("status", XBValidateUtils::test_token_failed);
      return ptree;
    }
  }
  else { // ELF
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
  }

  //Create BOs
  xrt::bo bo_ifm, bo_ofm, bo_inter, bo_instr, bo_instr_no_op, bo_mc;
  if (!elf) {
    int argno = 1;
    bo_ifm = xrt::bo(working_dev, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(argno++));
    argno++;
    bo_ofm = xrt::bo(working_dev, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(argno++));
    bo_inter = xrt::bo(working_dev, inter_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(argno++));
    bo_instr = xrt::bo(working_dev, instr_size*sizeof(int), XCL_BO_FLAGS_CACHEABLE, kernel.group_id(argno));
    bo_instr_no_op = xrt::bo(working_dev, instr_size*sizeof(int), XCL_BO_FLAGS_CACHEABLE, kernel.group_id(argno++));
    argno++;
    bo_mc = xrt::bo(working_dev, 16, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(argno++));
  
    XBValidateUtils::init_instr_buf(bo_instr, dpu_instr);
    //Create ctrlcode with NOPs
    std::memset(bo_instr_no_op.map<char*>(), 0, instr_size);
  }
  else {
    bo_ifm = xrt::ext::bo{working_dev, buffer_size};
    bo_ofm = xrt::ext::bo{working_dev, buffer_size};
    bo_inter = xrt::ext::bo{working_dev, inter_size};
    bo_mc = xrt::ext::bo{working_dev, 16};
  }

  // map input buffer
  // Incremental byte pattern
  auto ifm_mapped = bo_ifm.map<int*>();
  for (size_t i = 0; i < word_count; i++)
    ifm_mapped[i] = (int)(i % word_count);

  //Sync BOs
  bo_ifm.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  if (!elf) { 
    bo_instr.sync(XCL_BO_SYNC_BO_TO_DEVICE); 
    bo_mc.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  }

  //Log
  if(XBU::getVerbose()) { 
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Buffer size: %f MB") % buffer_size_mb));
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("No. of iterations: %f") % itr_count));
  }

  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0 ;i < itr_count ; i++){
    try{
      xrt::run run;
      if (!elf) {
        run = kernel(XBValidateUtils::getOpcode(), bo_ifm, NULL, bo_ofm, bo_inter, bo_instr_no_op, instr_size, bo_mc);
      } else { 
        run = kernel_no_op(XBValidateUtils::getOpcode(), 0, 0, bo_ifm, 0, bo_ofm, bo_inter, 0);
      }

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
      if (!elf) {
        run = kernel(XBValidateUtils::getOpcode(), bo_ifm, NULL, bo_ofm, bo_inter, bo_instr, instr_size, bo_mc);
      }
      else {
        run = kernel(XBValidateUtils::getOpcode(), 0, 0, bo_ifm, 0, bo_ofm, bo_inter, 0);
      }
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
