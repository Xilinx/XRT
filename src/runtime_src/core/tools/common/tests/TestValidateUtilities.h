// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestValidateUtilities_h_
#define __TestValidateUtilities_h_

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "tools/common/TestRunner.h"

class TestParams {
public:
  xrt::xclbin xclbin;               // Xclbin object
  xrt::device device;              
  std::string kernel_name;
  std::string dpu_file;
  int queue_len;
  size_t buffer_size;
  int itr_count;
  
  TestParams(const xrt::xclbin& xclbin, xrt::device device, const std::string& kernel_name, const std::string& dpu_file, int queue_len, size_t buffer_size, int itr_count)
    : xclbin(xclbin), device(device), kernel_name(kernel_name), dpu_file(dpu_file), queue_len(queue_len), buffer_size(buffer_size), itr_count(itr_count) {}
};

// Class representing a set of buffer objects (BOs)
class BO_set {
  size_t buffer_size; // Size of the buffer
  xrt::bo bo_instr;   // Buffer object for instructions
  xrt::bo bo_ifm;     // Buffer object for input feature map
  xrt::bo bo_param;   // Buffer object for parameters
  xrt::bo bo_ofm;     // Buffer object for output feature map
  xrt::bo bo_inter;   // Buffer object for intermediate data
  xrt::bo bo_mc;      // Buffer object for memory controller

public:
  // Constructor to initialize buffer objects
  BO_set(const xrt::device&, const xrt::kernel&, const std::string&, size_t);

  // Method to set kernel arguments
  void set_kernel_args(xrt::run&) const;

  // Method to synchronize buffer objects to the device
  void sync_bos_to_device();
};


// Class representing a test case, which is created for a single run on a single thread//
class TestCase {
  TestParams params;           // Test parameters
  xrt::hw_context hw_ctx;           // Hardware context
  std::vector<xrt::run> run_list;   // Collection of run objects
  std::vector<xrt::kernel> kernels; // Collection of kernel objects
  std::vector<BO_set> bo_set_list;  // Collection of buffer object sets

public:
  // Constructor to initialize the test case with xclbin and kernel name with hardware context creation
  TestCase(const TestParams& params)
      : params(params) {}

  void initialize();
  void run();
};


namespace XBValidateUtils{

void init_instr_buf(xrt::bo &bo_instr, const std::string& dpu_file);
size_t get_instr_size(const std::string& dpu_file);
void wait_for_max_clock(int&, std::shared_ptr<xrt_core::device>);

} //End of namespace XBValidateUtils
#endif
