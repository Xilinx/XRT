// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
#include "tools/common/XBUtilities.h"
#include "tools/common/tests/TestTemporalSharingOvd.h"
#include "TestHelper.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_kernel.h"

#include <thread>

namespace XBU = XBUtilities;

boost::property_tree::ptree 
TestTemporalSharingOvd::run(std::shared_ptr<xrt_core::device> dev) {
  ptree.erase("xclbin");
  try {
    set_threshold(dev, ptree);
    if(XBU::getVerbose())
      logger(ptree, "Details", boost::str(boost::format("Threshold is %.1f ms") % get_threshold()));
  }
  catch (const std::runtime_error& ex) {
    logger(ptree, "Details", ex.what());
    ptree.put("status", test_token_skipped);
    return ptree;
  }
  const auto xclbin_name = xrt_core::device_query<xrt_core::query::xclbin_name>(dev, xrt_core::query::xclbin_name::type::validate);
  auto xclbin_path = findPlatformFile(xclbin_name, ptree);
  if (!std::filesystem::exists(xclbin_path))
    return ptree;

  logger(ptree, "Xclbin", xclbin_path);

  xrt::xclbin xclbin;
  try{
    xclbin = xrt::xclbin(xclbin_path);
  }
  catch(const std::runtime_error& ex) {
    logger(ptree, "Error", ex.what());
    ptree.put("status", test_token_failed);
    return ptree;
  }
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

  // Run 1 
  std::vector<std::thread> threads;
  std::vector<TestCase> testcases;

  // Create two test cases and add them to the vector
  testcases.emplace_back(xclbin, kernelName, working_dev);
  testcases.emplace_back(xclbin, kernelName, working_dev);

  initializeTests(testcases); 

  // Lambda function to run a test case. This will be sent to individual thread to be run.
  auto runTestcase = [&](TestCase& test) {
    try {
      test.run();
    } catch (const std::exception& ex) {
      logger(ptree, "Error", ex.what());
      ptree.put("status", test_token_failed);
      return;
    }
  };

  // Measure the latency for running the test cases in parallel
  auto start = std::chrono::high_resolution_clock::now(); 
  // Create two threads to run the test cases
  threads.emplace_back(runTestcase, std::ref(testcases[0]));
  threads.emplace_back(runTestcase, std::ref(testcases[1]));

  for (uint32_t i = 0; i < threads.size(); i++) {
    threads[i].join();
  }
  auto end = std::chrono::high_resolution_clock::now(); 
  float latencySpatial = std::chrono::duration_cast<std::chrono::duration<float>>(end-start).count();

  threads.clear();
  testcases.clear();

  // Run 2 
  // Create three test cases and add them to the vector
  testcases.emplace_back(xclbin, kernelName, working_dev);
  testcases.emplace_back(xclbin, kernelName, working_dev);
  testcases.emplace_back(xclbin, kernelName, working_dev);

  initializeTests(testcases); 

  // Measure the latency for running the test cases in parallel
  start = std::chrono::high_resolution_clock::now(); 
  // Create three threads to run the test cases
  threads.emplace_back(runTestcase, std::ref(testcases[0]));
  threads.emplace_back(runTestcase, std::ref(testcases[1]));
  threads.emplace_back(runTestcase, std::ref(testcases[2]));

  for (uint32_t i = 0; i < threads.size(); i++) {
    threads[i].join();
  }
  end = std::chrono::high_resolution_clock::now(); 
  float latencyTemporal = std::chrono::duration_cast<std::chrono::duration<float>>(end-start).count(); 
  // End of Run 2 

  if(XBU::getVerbose()){
    logger(ptree, "Details", boost::str(boost::format("Spatially shared contexts latency: '%.1f' ms") % (latencySpatial * 1000)));
    logger(ptree, "Details", boost::str(boost::format("Temporally shared contexts latency: '%.1f' ms") % (latencyTemporal * 1000)));
  }
  auto overhead = (latencyTemporal - latencySpatial); 
  logger(ptree, "Details", boost::str(boost::format("Overhead: '%.1f' ms") % (overhead * 1000)));

  // Set the test status to passed

  //check if the value is in range
  result_in_range(overhead, get_threshold(), ptree);
  
  ptree.put("status", test_token_passed);
  return ptree;
}

void
TestTemporalSharingOvd::initializeTests(std::vector<TestCase>& testcases) {
  for (unsigned int i = 0; i < testcases.size(); i++) {
    try{
      testcases[i].initialize();
    } catch (const std::exception& ex) {
      logger(ptree, "Error", ex.what());
      ptree.put("status", test_token_failed);
      return;
    }
  } 
}
