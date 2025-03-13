// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestValidateUtilities_h_
#define __TestValidateUtilities_h_

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "xrt/xrt_device.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"
#include <vector>
#include <string>

#include <boost/property_tree/ptree.hpp>
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

constexpr std::string_view test_token_skipped = "SKIPPED";
constexpr std::string_view test_token_failed = "FAILED";
constexpr std::string_view test_token_passed = "PASSED";

void init_instr_buf(xrt::bo &bo_instr, const std::string& dpu_file);
size_t get_instr_size(const std::string& dpu_file);
void logger(boost::property_tree::ptree& , const std::string&, const std::string&);
std::string findPlatformPath(const std::shared_ptr<xrt_core::device>& dev, boost::property_tree::ptree& ptTest);
std::string findPlatformFile(const std::string& file_path, boost::property_tree::ptree& ptTest);
std::string findXclbinPath(const std::shared_ptr<xrt_core::device>& dev,
                           boost::property_tree::ptree& ptTest);
std::string searchLegacyXclbin(const uint16_t vendor, const std::string& dev_name, 
                               boost::property_tree::ptree& _ptTest);
std::string searchSSV2Xclbin(const std::string& logic_uuid,
                             boost::property_tree::ptree& _ptTest);
void program_xclbin(const std::shared_ptr<xrt_core::device>& device, const std::string& xclbin);
bool search_and_program_xclbin(const std::shared_ptr<xrt_core::device>& dev, boost::property_tree::ptree& ptTest);
int validate_binary_file(const std::string& binaryfile);
std::string dpu_or_elf(const std::shared_ptr<xrt_core::device>& dev, const xrt::xclbin& xclbin,
              boost::property_tree::ptree& ptTest);
} //End of namespace XBValidateUtils
#endif
