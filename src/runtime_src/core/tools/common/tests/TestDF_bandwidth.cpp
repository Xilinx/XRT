// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestDF_bandwidth.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_kernel.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files

// System - Include Files
#include <fstream>
#include <filesystem>

static constexpr size_t host_app = 1; //opcode
static constexpr size_t buffer_size_gb = 1;
static constexpr size_t buffer_size = buffer_size_gb * 1024 * 1024 * 1024; //1 GB
static constexpr size_t word_count = buffer_size/4;
static constexpr int itr_count = 600;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestDF_bandwidth::TestDF_bandwidth()
  : TestRunner("df-bw", "Run bandwidth test on data fabric")
{}

boost::property_tree::ptree
TestDF_bandwidth::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.erase("xclbin");

  const auto xclbin_name = xrt_core::device_query<xrt_core::query::xclbin_name>(dev, xrt_core::query::xclbin_name::type::validate);
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

  const auto seq_name = xrt_core::device_query<xrt_core::query::sequence_name>(dev, xrt_core::query::sequence_name::type::df_bandwidth);
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

  //Create BOs
  xrt::bo bo_ifm(working_dev, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(1));
  xrt::bo bo_ofm(working_dev, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(3));
  xrt::bo bo_instr(working_dev, instr_size*sizeof(int), XCL_BO_FLAGS_CACHEABLE, kernel.group_id(5));

  XBValidateUtils::init_instr_buf(bo_instr, dpu_instr);

  // map input buffer
  auto ifm_mapped = bo_ifm.map<int*>();
	for (size_t i = 0; i < word_count; i++)
		ifm_mapped[i] = rand() % 4096;

  //Sync BOs
  bo_instr.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_ifm.sync(XCL_BO_SYNC_BO_TO_DEVICE);

  //Log
  if(XBU::getVerbose()) { 
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Buffer size: %f GB") % buffer_size_gb));
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("No. of iterations: %f") % itr_count));
  }

  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < itr_count; i++) {
    try {
      auto run = kernel(host_app, bo_ifm, NULL, bo_ofm, NULL, bo_instr, instr_size, NULL);
      // Wait for kernel to be done
      run.wait2();
    }
    catch (const std::exception& ex) {
      XBValidateUtils::logger(ptree, "Error", ex.what());
      ptree.put("status", XBValidateUtils::test_token_failed);
      return ptree;
    }
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

  //Calculate bandwidth
  auto elapsedSecs = std::chrono::duration_cast<std::chrono::duration<double>>(end-start).count();
  //Data is read and written in parallel hence x2
  double bandwidth = (buffer_size_gb*itr_count*2) / elapsedSecs;

  if(XBU::getVerbose())
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Total duration: %.1fs") % elapsedSecs));
  XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average bandwidth per shim DMA: %.1f GB/s") % bandwidth));
  ptree.put("status", XBValidateUtils::test_token_passed);

  return ptree;
}
