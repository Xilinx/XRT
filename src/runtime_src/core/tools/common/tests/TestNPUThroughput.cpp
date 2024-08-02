// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestNPUThroughput.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_kernel.h"
namespace XBU = XBUtilities;

#include <filesystem>

static constexpr size_t host_app = 1; //opcode
static constexpr size_t buffer_size = 20;
static constexpr int itr_count_throughput = 2500;
// ----- C L A S S   M E T H O D S -------------------------------------------
TestNPUThroughput::TestNPUThroughput()
  : TestRunner("throughput", "Run end-to-end throughput test")
{}

boost::property_tree::ptree
TestNPUThroughput::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.erase("xclbin");

  const auto xclbin_name = xrt_core::device_query<xrt_core::query::xclbin_name>(dev, xrt_core::query::xclbin_name::type::validate);
  auto xclbin_path = findPlatformFile(xclbin_name, ptree);
  if (!std::filesystem::exists(xclbin_path))
    return ptree;

  logger(ptree, "Xclbin", xclbin_path);

  xrt::xclbin xclbin;
  try {
    xclbin = xrt::xclbin(xclbin_path);
  }
  catch (const std::runtime_error& ex) {
    logger(ptree, "Error", ex.what());
    ptree.put("status", test_token_failed);
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
    logger(ptree, "Error", "No kernel with `DPU` found in the xclbin");
    ptree.put("status", test_token_failed);
    return ptree;
  }
  auto kernelName = xkernel.get_name();
  if(XBU::getVerbose())
    logger(ptree, "Details", boost::str(boost::format("Kernel name is '%s'") % kernelName));

  auto working_dev = xrt::device(dev);
  working_dev.register_xclbin(xclbin);

  xrt::hw_context hwctx;
  xrt::kernel testker;
  try {
    hwctx = xrt::hw_context(working_dev, xclbin.get_uuid());
    testker = xrt::kernel(hwctx, kernelName);
  }
  catch (const std::exception& ex){
    logger(ptree, "Error", ex.what());
    ptree.put("status", test_token_failed);
    return ptree;
  }

  //Create BOs, the values are not initialized as they are not really used by this special test running on the device
  int argno = 1;
  xrt::bo bo_ifm(working_dev, buffer_size, XRT_BO_FLAGS_HOST_ONLY, testker.group_id(argno++));
  xrt::bo bo_param(working_dev, buffer_size, XRT_BO_FLAGS_HOST_ONLY, testker.group_id(argno++));
  xrt::bo bo_ofm(working_dev, buffer_size, XRT_BO_FLAGS_HOST_ONLY, testker.group_id(argno++));
  xrt::bo bo_inter(working_dev, buffer_size, XRT_BO_FLAGS_HOST_ONLY, testker.group_id(argno++));
  xrt::bo bo_instr(working_dev, buffer_size, XCL_BO_FLAGS_CACHEABLE, testker.group_id(argno++));
  argno++;
  xrt::bo bo_mc(working_dev, buffer_size, XRT_BO_FLAGS_HOST_ONLY, testker.group_id(argno++));
  //Create ctrlcode with NOPs
  std::memset(bo_instr.map<char*>(), 0, buffer_size);

  //Sync BOs
  bo_instr.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_ifm.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_param.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_mc.sync(XCL_BO_SYNC_BO_TO_DEVICE);

  //Log
  if(XBU::getVerbose()) {
    logger(ptree, "Details", boost::str(boost::format("Instruction size: '%f' bytes") % buffer_size));
    logger(ptree, "Details", boost::str(boost::format("No. of iterations: '%f'") % itr_count_throughput));
  }

  // Run the test to compute throughput where we saturate NPU with jobs and then wait for all
  // completions at the end
  float elapsedSecs = 0.0;
  std::array<xrt::run, itr_count_throughput> runhandles;

  try {
    auto start = std::chrono::high_resolution_clock::now();
    for (auto & hand : runhandles)
      hand = testker(host_app, bo_ifm, bo_param, bo_ofm, bo_inter, bo_instr, buffer_size, bo_mc);
    for (const auto& hand: runhandles)
      hand.wait2();
    auto end = std::chrono::high_resolution_clock::now();
    elapsedSecs = std::chrono::duration_cast<std::chrono::duration<float>>(end-start).count();
  }
  catch (const std::exception& ex) {
    logger(ptree, "Error", ex.what());
    ptree.put("status", test_token_failed);
  }

  // Compute the throughput
  const double throughput = (elapsedSecs != 0.0) ? runhandles.size() / elapsedSecs : 0.0;

  logger(ptree, "Details", boost::str(boost::format("Average throughput: '%.1f' ops") % throughput));
  ptree.put("status", test_token_passed);
  return ptree;
}
