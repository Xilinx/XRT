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

// Struct to hold buffer sizes
struct BufferSizes {
  size_t ifm_size;
  size_t param_size;
  size_t inter_size;
  size_t mc_size;
  size_t ofm_size;
  size_t instr_word_size;
  size_t instr_size; // Derived from instr_word_size
};

class TestParams {
public:
  xrt::xclbin xclbin;               // Xclbin object
  xrt::device device;              
  std::string kernel_name;
  std::string elf_file;
  std::string ifm_file;
  std::string param_file;
  std::string buffer_sizes_file;
  int queue_len;
  int itr_count;
  
  TestParams(xrt::xclbin xclbin, 
             xrt::device device, 
             std::string kernel_name, 
             std::string elf_file, 
             std::string ifm_file, 
             std::string param_file, 
             std::string buffer_sizes_file,
             int queue_len, 
             int itr_count
             )
    : xclbin(std::move(xclbin)), 
      device(std::move(device)), 
      kernel_name(std::move(kernel_name)), 
      elf_file(std::move(elf_file)), 
      ifm_file(std::move(ifm_file)), 
      param_file(std::move(param_file)), 
      buffer_sizes_file(std::move(buffer_sizes_file)),
      queue_len(queue_len), 
      itr_count(itr_count) 
    {}
};

// Class representing a set of buffer objects (BOs)
class BO_set {
  BufferSizes buffer_sizes;
  xrt::bo bo_ifm;     // Buffer object for input feature map
  xrt::bo bo_param;   // Buffer object for parameters
  xrt::bo bo_ofm;     // Buffer object for output feature map
  xrt::bo bo_inter;   // Buffer object for intermediate data
  xrt::bo bo_mc;      // Buffer object for memory controller

public:
  // Constructor to initialize buffer objects
  BO_set(const xrt::device&, const BufferSizes&, const std::string&, const std::string&);

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

BufferSizes read_buffer_sizes(const std::string& json_file);
void init_instr_buf(xrt::bo &bo_instr, const std::string& dpu_file);
void init_buf_bin(int* buff, size_t bytesize, const std::string &filename);
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
bool get_elf();
int get_opcode();
std::string get_xclbin_path(const std::shared_ptr<xrt_core::device>& device, xrt_core::query::xclbin_name::type test_type, boost::property_tree::ptree& ptTest);
std::string get_kernel_name(const xrt::xclbin& xclbin, boost::property_tree::ptree& ptTest);
} //End of namespace XBValidateUtils
#endif
