// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestSpatialSharingOvd.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_kernel.h"
#include <thread>

namespace XBU = XBUtilities;

static constexpr size_t host_app = 1; //opcode
static constexpr size_t buffer_size = 1024 * 1024 * 1024; //1 GB

// Method to run the test
// Parameters:
// - dev: Shared pointer to the device
// Returns:
// - Property tree containing the test results
boost::property_tree::ptree TestSpatialSharingOvd::run(std::shared_ptr<xrt_core::device> dev) {
  // Clear any existing "xclbin" entry in the property tree
  ptree.erase("xclbin");

  // Query the xclbin name from the device
  const auto xclbin_name = xrt_core::device_query<xrt_core::query::xclbin_name>(dev, xrt_core::query::xclbin_name::type::validate);

  // Find the platform file path for the xclbin
  auto xclbin_path = XBValidateUtils::findPlatformFile(xclbin_name, ptree);

  // If the xclbin file does not exist, return the property tree
  if (!std::filesystem::exists(xclbin_path))
    return ptree;

  // Create an xclbin object
  xrt::xclbin xclbin;
  try {
    // Load the xclbin file
    xclbin = xrt::xclbin(xclbin_path);
  }
  catch (const std::runtime_error& ex) {
    // Log any runtime error and set the status to failed
    XBValidateUtils::logger(ptree, "Error", ex.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }

  // Determine The DPU Kernel Name
  auto xkernels = xclbin.get_kernels();

  // Find the first kernel whose name starts with "DPU"
  auto itr = std::find_if(xkernels.begin(), xkernels.end(), [](xrt::xclbin::kernel& k) {
    auto name = k.get_name();
    return name.rfind("DPU",0) == 0; // Starts with "DPU"
  });

  xrt::xclbin::kernel xkernel;
  if (itr != xkernels.end())
    xkernel = *itr;
  else {
    // Log an error if no kernel with "DPU" is found and set the status to failed
    XBValidateUtils::logger(ptree, "Error", "No kernel with `DPU` found in the xclbin");
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }

  // Get the name of the found kernel
  auto kernelName = xkernel.get_name();

  // Create a working device from the provided device
  auto working_dev = xrt::device(dev);
  working_dev.register_xclbin(xclbin);

  // Lambda function to run a test case. This will be sent to individual thread to be run.
  auto runTestcase = [&](TestCase& test) {
    try {
      test.run();
    } catch (const std::exception& ex) {
      XBValidateUtils::logger(ptree, "Error", ex.what());
      ptree.put("status", XBValidateUtils::test_token_failed);
      return;
    }
  };

  const auto seq_name = xrt_core::device_query<xrt_core::query::sequence_name>(dev, xrt_core::query::sequence_name::type::df_bandwidth);
  auto dpu_instr = XBValidateUtils::findPlatformFile(seq_name, ptree);
  if (!std::filesystem::exists(dpu_instr))
    return ptree;

  /* Run 1 */
  std::vector<std::thread> threads;
  std::vector<TestCase> testcases;

  // Create two test cases and add them to the vector
  TestParams params(xclbin, working_dev, kernelName, dpu_instr, 2, buffer_size, 10);
  testcases.emplace_back(params);
  testcases.emplace_back(params);

  for (uint32_t i = 0; i < testcases.size(); i++) {
    try{
      testcases[i].initialize();
    } catch (const std::exception& ex) {
      XBValidateUtils::logger(ptree, "Error", ex.what());
      ptree.put("status", XBValidateUtils::test_token_failed);
      return ptree;
    }
  }

  // Measure the latency for running the test cases in parallel
  auto start = std::chrono::high_resolution_clock::now(); 

  // Create two threads to run the test cases
  threads.emplace_back(runTestcase, std::ref(testcases[0]));
  threads.emplace_back(runTestcase, std::ref(testcases[1]));

  for (uint32_t i = 0; i < threads.size(); i++) {
    threads[i].join();
  }
  auto end = std::chrono::high_resolution_clock::now(); 
  auto latencyShared = std::chrono::duration_cast<std::chrono::duration<double>>(end-start).count();

  //Clearing so that the hardware contexts get destroyed and the Run 2 is start afresh
  testcases.clear();
  /* End of Run 1 */

  /* Run 2 */
  // Create a single test case and run it in a single thread
  TestCase singleHardwareCtxTest(params);
  try{
    singleHardwareCtxTest.initialize();
  } 
  catch (const std::exception& ex) {
    XBValidateUtils::logger(ptree, "Error", ex.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }
  // Measure the latency for running the test case in a single thread
  start = std::chrono::high_resolution_clock::now(); 
  std::thread thr(runTestcase, std::ref(singleHardwareCtxTest));

  thr.join();
  end = std::chrono::high_resolution_clock::now(); 
  auto latencySingle =  std::chrono::duration_cast<std::chrono::duration<double>>(end-start).count(); 
  /* End of Run 2 */

  // Log the latencies and the overhead
  if(XBU::getVerbose()){
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Single context latency: %.1f ms") % (latencySingle * 1000)));
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Spatially shared multiple context latency: %.1f ms") % (latencyShared * 1000)));
  }
  auto overhead = (latencyShared - latencySingle) * 1000;
  XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Overhead: %.1f ms") % overhead));
  ptree.put("status", XBValidateUtils::test_token_passed);

  return ptree;
}
