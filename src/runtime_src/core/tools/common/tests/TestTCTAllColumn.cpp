// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestTCTAllColumn.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
namespace XBU = XBUtilities;

// System - Include Files
#include <fstream>
#include <filesystem>

static constexpr size_t buffer_size = 4;
static constexpr size_t word_count = buffer_size/4;
static constexpr int itr_count = 20000;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestTCTAllColumn::TestTCTAllColumn()
  : TestRunner("tct-all-col", "Measure average TCT processing time for all columns")
{}

boost::property_tree::ptree
TestTCTAllColumn::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.erase("xclbin");

  auto elf = XBValidateUtils::get_elf();
  std::string xclbin_path; 
  
  if (!elf) {
    xclbin_path = XBValidateUtils::get_xclbin_path(dev, xrt_core::query::xclbin_name::type::validate, ptree);
    if (XBU::getVerbose())
      XBValidateUtils::logger(ptree, "Details", "Using DPU Sequence");
  } else {
    xclbin_path = XBValidateUtils::get_xclbin_path(dev, xrt_core::query::xclbin_name::type::validate_elf, ptree);
    if (XBU::getVerbose())
      XBValidateUtils::logger(ptree, "Details", "Using ELF");
  }

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

  size_t instr_size = 0;
  std::string dpu_instr;

  xrt::hw_context hwctx;
  xrt::kernel kernel;

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

    const auto seq_name = xrt_core::device_query<xrt_core::query::sequence_name>(dev, xrt_core::query::sequence_name::type::tct_one_column);
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
    const auto elf_name = xrt_core::device_query<xrt_core::query::elf_name>(dev, xrt_core::query::elf_name::type::tct_all_column);
    auto elf_path = XBValidateUtils::findPlatformFile(elf_name, ptree);
    
    if (!std::filesystem::exists(elf_path))
      return ptree;
  
    try {
      hwctx = xrt::hw_context(working_dev, xclbin.get_uuid());
      kernel = get_kernel(hwctx, kernelName, elf_path);
    } 
    catch (const std::exception& )
    {
      XBValidateUtils::logger (ptree, "Error", "Not enough columns available. Please make sure no other workload is running on the device.");
      ptree.put("status", XBValidateUtils::test_token_failed);
      return ptree;
    }
  }

  //Create BOs
  xrt::bo bo_ifm, bo_ofm, bo_instr; 
  if (!elf) {
    bo_ifm = xrt::bo(working_dev, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(1));
    bo_ofm = xrt::bo(working_dev, 4*buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(3));
    bo_instr = xrt::bo(working_dev, instr_size*sizeof(int), XCL_BO_FLAGS_CACHEABLE, kernel.group_id(5));
    XBValidateUtils::init_instr_buf(bo_instr, dpu_instr);
  }
  else {
    bo_ifm = xrt::ext::bo{working_dev, buffer_size};
    bo_ofm = xrt::ext::bo{working_dev, buffer_size};
  }

  // map input buffer
  auto ifm_mapped = bo_ifm.map<int*>();
	for (size_t i = 0; i < word_count; i++)
		ifm_mapped[i] = rand() % 4096;

  //Sync BOs
  bo_ifm.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  if (!elf) { 
    bo_instr.sync(XCL_BO_SYNC_BO_TO_DEVICE); 
  }

  //Log
  if(XBU::getVerbose()) {
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Buffer size: %f bytes") % buffer_size));
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("No. of iterations: %f") % itr_count));
  }

  auto start = std::chrono::high_resolution_clock::now();
  try {
    xrt::run run;
    if (!elf) {
      run = kernel(XBValidateUtils::get_opcode(), bo_ifm, NULL, bo_ofm, NULL, bo_instr, instr_size, NULL);
    } else {
      run = kernel(XBValidateUtils::get_opcode(), 0, 0, bo_ifm, 0, bo_ofm, 0, 0);
    }
    
    // Wait for kernel to be done
    run.wait2();
  }
  catch (const std::exception& ex) {
    XBValidateUtils::logger(ptree, "Error", ex.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }
  auto end = std::chrono::high_resolution_clock::now();

  //map ouput buffer
  bo_ofm.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
  auto ofm_mapped = bo_ofm.map<int*>();
  for (size_t i = 0; i < word_count; i++) {
    if (ofm_mapped[i] != ifm_mapped[i]) {
      auto msg = boost::str(boost::format("Data mismatch at out buffer[%d]") % i);
      XBValidateUtils::logger(ptree, "Error", msg);
      return ptree;
    }
  }

  //Calculate throughput
  auto elapsedSecs = std::chrono::duration_cast<std::chrono::duration<double>>(end-start).count();
  double throughput = itr_count / elapsedSecs;
  double latency = (elapsedSecs / itr_count) * 1000000; //convert s to us

  if(XBU::getVerbose())
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average time for TCT: %.1f us") % latency));
  XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average TCT throughput: %.1f TCT/s") % throughput));
  ptree.put("status", XBValidateUtils::test_token_passed);

  return ptree;
}
