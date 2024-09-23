// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestHelper_h_
#define __TestHelper_h_

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "tools/common/TestRunner.h"

// Class representing a set of buffer objects (BOs)
class BO_set {
  size_t buffer_size; // Size of the buffer
  xrt::bo bo_instr;   // Buffer object for instructions
  xrt::bo bo_ifm;     // Buffer object for input feature map
  xrt::bo bo_param;   // Buffer object for parameters
  xrt::bo bo_ofm;     // Buffer object for output feature map
  xrt::bo bo_inter;   // Buffer object for intermediate data
  xrt::bo bo_mc;      // Buffer object for memory controller
  uint32_t instr_size; // Size of the instruction buffer

public:
  // Constructor to initialize buffer objects
  BO_set(const xrt::device&, const xrt::kernel&, size_t);

  // Method to set kernel arguments
  void set_kernel_args(xrt::run&) const;

  // Method to synchronize buffer objects to the device
  void sync_bos_to_device();
};


// Class representing a test case, which is created for a single run on a single thread//
class TestCase {
  xrt::device device;               // Device object
  xrt::xclbin xclbin;               // Xclbin object
  std::string kernel_name;          // Name of the kernel
  xrt::hw_context hw_ctx;           // Hardware context
  uint32_t queue_len;               // Queue length
  size_t buffer_size;               // Size of the buffer
  int itr_count;                    // Number of iterations
  std::vector<xrt::run> run_list;   // Collection of run objects
  std::vector<xrt::kernel> kernels; // Collection of kernel objects
  std::vector<BO_set> bo_set_list;  // Collection of buffer object sets

public:
  // Constructor to initialize the test case with xclbin and kernel name with hardware context creation
  TestCase(const xrt::xclbin& xclbin, const std::string& kernel, const xrt::device& device)
      : device(device), xclbin(xclbin), kernel_name(kernel), hw_ctx(device, xclbin.get_uuid()), queue_len(4), buffer_size(1024), itr_count(1000) {}

  void initialize();
  void run();
};;;
#endif
