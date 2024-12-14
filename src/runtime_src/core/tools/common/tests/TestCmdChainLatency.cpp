// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestCmdChainLatency.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_kernel.h"
#include "xrt/experimental/xrt_kernel.h"
namespace XBU = XBUtilities;

#include <filesystem>

static constexpr size_t buffer_size = 20;
static constexpr int itr_count = 10;
static constexpr int run_count = 1000;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestCmdChainLatency::TestCmdChainLatency()
  : TestRunner("cmd-chain-latency", "Run end-to-end latency test using command chaining")
{}

boost::property_tree::ptree
TestCmdChainLatency::run(std::shared_ptr<xrt_core::device> dev)
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
  xrt::kernel testker;
  try {
    hwctx = xrt::hw_context(working_dev, xclbin.get_uuid());
    testker = xrt::kernel(hwctx, kernelName);
  }
  catch (const std::exception& )
  {
    XBValidateUtils::logger (ptree, "Error", "Not enough columns available. Please make sure no other workload is running on the device.");
    ptree.put("status", XBValidateUtils::test_token_failed);ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }

  // Find PS kernel instance as expected by KMD, but
  // construct the xrt::kernel from the CU base name
  std::string kernel_name;
  xrt::xclbin::ip cu;
  for (const auto& ip : xclbin.get_ips()) {
    if (ip.get_type() != xrt::xclbin::ip::ip_type::ps)
      continue;

    cu = ip;
    auto cu_name = cu.get_name();
    kernel_name = cu_name.substr(0, cu_name.find(':'));
    break;
  }

  // create specified number of runs and populate with arguments
  std::vector<xrt::bo> global_args;
  std::vector<xrt::run> runs;

  for (int i=0; i < run_count; ++i) {
    auto run = xrt::run(testker);
    for (const auto& arg : cu.get_args()) {
      auto arg_idx = static_cast<int>(arg.get_index());
      if (arg.get_host_type() == "uint64_t")
	      run.set_arg(arg_idx, static_cast<uint64_t>(1));
      else if (arg.get_host_type() == "uint32_t")
	      run.set_arg(arg_idx, static_cast<uint32_t>(1));
      else if (arg.get_host_type().find('*') != std::string::npos) {
        xrt::bo bo;

        if (arg.get_name() == "instruct")
          bo = xrt::bo(hwctx, arg.get_size(), xrt::bo::flags::cacheable, testker.group_id(arg_idx));
        else 
          bo = xrt::bo(working_dev, arg.get_size(), xrt::bo::flags::host_only, testker.group_id(arg_idx));

      bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
	    global_args.push_back(bo);
	    run.set_arg(arg_idx, bo);
      }
    }
    runs.push_back(std::move(run));
  }

  //Log
  if(XBU::getVerbose()) {
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Instruction size: %f bytes") % buffer_size));
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("No. of commands: %f") % (itr_count*run_count)));
  }

  // Start via runlist
  xrt::runlist runlist{hwctx};
  for (auto& run : runs)
    runlist.add(run);

  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < itr_count; ++i) {
    try {
      runlist.execute();
    }
    catch (const std::exception& ex) {
      XBValidateUtils::logger(ptree, "Error", ex.what());
      ptree.put("status", XBValidateUtils::test_token_failed);
      return ptree;
    }

    try {
      runlist.wait();
    }
    catch (const std::exception& ex) {
      XBValidateUtils::logger(ptree, "Error", ex.what());
      ptree.put("status", XBValidateUtils::test_token_failed);
      return ptree;
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsedSecs = std::chrono::duration_cast<std::chrono::duration<double>>(end-start).count();

  // Calculate end-to-end latency of one job execution
  const double latency = (elapsedSecs / (itr_count*run_count)) * 1000000; //convert s to us

  XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average latency: %.1f us") % latency));
  ptree.put("status", XBValidateUtils::test_token_passed);
  return ptree;
}
