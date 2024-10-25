// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestVerify.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include <filesystem>
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

static constexpr size_t buffer_size = 64;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestVerify::TestVerify()
  : TestRunner("verify", 
                "Run 'Hello World' kernel test", 
                "verify.xclbin", 
                false){}

boost::property_tree::ptree
TestVerify::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree;
  ptree = get_test_header();
  xrt::device device(dev);

  const std::string test_path = XBValidateUtils::findPlatformPath(dev, ptree);
  if (test_path.empty()) {
    XBValidateUtils::logger(ptree, "Error", "Platform test path was not found.");
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }

  const std::string b_file = XBValidateUtils::findXclbinPath(dev, ptree);
  // 0RP (nonDFX) flat shell support.
  // Currently, there isn't a clean way to determine if a nonDFX shell's interface is truly flat.
  // At this time, this is determined by whether or not it delivers an accelerator (e.g., verify.xclbin)
  const auto logic_uuid = xrt_core::device_query_default<xrt_core::query::logic_uuids>(dev, {});
  if (!logic_uuid.empty() && !std::filesystem::exists(b_file)) {
    XBValidateUtils::logger(ptree, "Details", "Verify xclbin not available or shell partition is not programmed. Skipping validation.");
    ptree.put("status", XBValidateUtils::test_token_skipped);
    return ptree;
  }
  auto xclbin_uuid = device.load_xclbin(b_file);

  xrt::kernel krnl;
  try {
    krnl = xrt::kernel(device, xclbin_uuid, "verify");
  } catch (const std::exception&) {
    try {
      krnl = xrt::kernel(device, xclbin_uuid, "hello");
    } catch (const std::exception&) {
      XBValidateUtils::logger(ptree, "Error", "Kernel could not be found.");
      ptree.put("status", XBValidateUtils::test_token_failed);
      return ptree;
    }
  }

  // Allocate the output buffer to hold the kernel output
  auto output_buffer = xrt::bo(device, sizeof(char) * buffer_size, krnl.group_id(0));

  // Run the kernel and store its contents within the allocated output buffer
  auto run = krnl(output_buffer);
  run.wait();

  // Prepare local buffer
  char received_data[buffer_size] = {};

  // Acquire and read the buffer data
  output_buffer.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
  output_buffer.read(received_data);

  // Compare received data against expected data
  std::string expected_data = "Hello World\n";
  if (std::memcmp(received_data, expected_data.data(), expected_data.size())) {
    XBValidateUtils::logger(ptree, "Error", "Value read back does not match reference");
    ptree.put("status", XBValidateUtils::test_token_failed);
  }

  ptree.put("status", XBValidateUtils::test_token_passed);
  return ptree;
}
