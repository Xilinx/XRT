// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestVerify.h"
#include "TestIPU.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#define LENGTH 64

// ----- C L A S S   M E T H O D S -------------------------------------------
TestVerify::TestVerify()
  : TestRunner("verify", 
                "Run 'Hello World' kernel test", 
                "verify.xclbin", 
                false){}

boost::property_tree::ptree
TestVerify::run(std::shared_ptr<xrt_core::device> dev)
{
  auto device_name = xrt_core::device_query_default<xrt_core::query::rom_vbnv>(dev, "");
  boost::property_tree::ptree ptree;
  //to-do: Do it in a cleaner way
  if (device_name.find("Ryzen") != std::string::npos) {
    //run ipu verify
    ptree = TestIPU{}.run(dev);
  }
  else {
    ptree = get_test_header();
    std::cout << "Verify runTest start!\n Running..." << std::endl;
    runTest(dev, ptree);
    std::cout << "Verify runTest finished!\n Exiting..." << std::endl;
  }
  return ptree;
}

void
TestVerify::runTest(std::shared_ptr<xrt_core::device> dev, boost::property_tree::ptree& ptree)
{
  const auto bdf_tuple = xrt_core::device_query<xrt_core::query::pcie_bdf>(dev);
  const std::string bdf = xrt_core::query::pcie_bdf::to_string(bdf_tuple);
  const std::string test_path = findPlatformPath(dev, ptree);
  const std::string b_file = findXclbinPath(dev, ptree);
  const std::vector<std::string> dependency_paths = findDependencies(test_path, m_xclbin);
  // bool flag_s = false;

  xrt::device device(dev->get_device_id());

  if (test_path.empty()) {
    logger(ptree, "Error", "Platform test path was not found.");
    ptree.put("status", test_token_failed);
    return;
  }

  auto xclbin_uuid = device.load_xclbin(b_file);

  auto krnl = xrt::kernel(device, xclbin_uuid, "verify");

  // Allocate the output buffer to hold the kernel ooutput
  auto output_buffer = xrt::bo(device, sizeof(char) * LENGTH, krnl.group_id(0));

  // Run the kernel and store its contents within the allocated output buffer
  auto run = krnl(output_buffer);
  run.wait();

  // Prepare local buffer
  char received_data[LENGTH] = {};

  // Acquire and read the buffer data
  output_buffer.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
  output_buffer.read(received_data);

  // Compare received data against expected data
  std::string expected_data = "Hello World\n";
  if (std::memcmp(received_data, expected_data.data(), expected_data.size())) {
    logger(ptree, "Error", "Value read back does not match reference");
    ptree.put("status", test_token_failed);
  }

  ptree.put("status", test_token_passed);
}
