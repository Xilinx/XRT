// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _TESTSPATIALSHARINGOVD_
#define _TESTSPATIALSHARINGOVD_

#include "tools/common/TestRunner.h"

// Class representing a set of buffer objects (BOs)
class BO_set {
  xrt::bo bo_instr;   // Buffer object for instructions
  xrt::bo bo_ifm;     // Buffer object for input feature map
  xrt::bo bo_param;   // Buffer object for parameters
  xrt::bo bo_ofm;     // Buffer object for output feature map
  xrt::bo bo_inter;   // Buffer object for intermediate data
  xrt::bo bo_mc;      // Buffer object for memory controller
  uint32_t instr_size; // Size of the instruction buffer

public:
  // Constructor to initialize buffer objects
  BO_set(xrt::device& device_ptr, xrt::kernel& kernel);

  // Method to set kernel arguments
  void set_kernel_args(xrt::run& run);

  // Method to synchronize buffer objects to the device
  void sync_bos_to_device();
};

// Class representing the TestSpatialSharingOvd test
class TestSpatialSharingOvd : public TestRunner {
  // Method to wait for threads to be ready so that the time measurement starts at the correct time
  void wait_for_threads_ready(uint32_t, std::mutex&, std::condition_variable&, uint32_t&);

public:
  boost::property_tree::ptree ptree;

  boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);

  // Constructor to initialize the test runner with a name and description
  TestSpatialSharingOvd()
    : TestRunner("spatial-sharing-overhead", "Run Spatial Sharing Overhead Test"), ptree(get_test_header()){}
};

// Class representing a test case, which is created for a single run on a single thread
class TestCase : public TestSpatialSharingOvd {
  xrt::device device;         // Device object
  xrt::xclbin xclbin;         // Xclbin object
  std::string kernel_name;    // Name of the kernel
  xrt::hw_context hw_ctx;     // Hardware context
  uint32_t queue_len = 4;     // Queue length

  // Method to signal that a thread is ready to run
  void thread_ready_to_run(std::mutex&, std::condition_variable&, uint32_t&);

public:
  // Constructor to initialize the test case with xclbin and kernel name with hardware context creation
  TestCase(xrt::xclbin& xclbin, std::string& kernel)
      : device(xrt::device(0)), xclbin(xclbin), kernel_name(kernel) 
  {
    device.register_xclbin(xclbin);
    hw_ctx = xrt::hw_context(device, xclbin.get_uuid());
  }

  void run(std::mutex&, std::condition_variable&, uint32_t&);
};

#endif
