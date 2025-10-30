// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef TestValidateUtilities_h_
#define TestValidateUtilities_h_

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "xrt/xrt_device.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"
#include <vector>
#include <string>

#include <boost/property_tree/ptree.hpp>

namespace XBValidateUtils{

constexpr std::string_view test_token_skipped = "SKIPPED";
constexpr std::string_view test_token_failed = "FAILED";
constexpr std::string_view test_token_passed = "PASSED";

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
