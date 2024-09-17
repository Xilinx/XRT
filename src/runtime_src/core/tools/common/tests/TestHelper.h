// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestHelper_h_
#define __TestHelper_h_

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
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
  size_t buffer_size; // Size of the buffer

public:
  // Constructor to initialize buffer objects
  BO_set(xrt::device&, xrt::kernel&, size_t);

  // Method to set kernel arguments
  void set_kernel_args(xrt::run&);

  // Method to synchronize buffer objects to the device
  void sync_bos_to_device();
};


// Class representing a test case, which is created for a single run on a single thread//
class TestCase {
  xrt::device device;         // Device object
  xrt::xclbin xclbin;         // Xclbin object
  std::string kernel_name;    // Name of the kernel
  xrt::hw_context hw_ctx;     // Hardware context
  uint32_t queue_len = 4;     // Queue length
  size_t buffer_size;         // Size of the buffer
  int itr_count;              // Number of iterations

  // Method to signal that a thread is ready to run
  void thread_ready_to_run(std::mutex&, std::condition_variable&, int&);

public:
  // Constructor to initialize the test case with xclbin and kernel name with hardware context creation
  TestCase(xrt::xclbin& xclbin, std::string& kernel, xrt::device& device)
      : device(device), xclbin(xclbin), kernel_name(kernel), buffer_size(1024), itr_count(1000) 
  {
    hw_ctx = xrt::hw_context(device, xclbin.get_uuid());
  }

  void run(std::mutex&, std::condition_variable&, int&);
};
#endif
