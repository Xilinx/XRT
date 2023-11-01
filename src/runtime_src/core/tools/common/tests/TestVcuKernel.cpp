// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestVcuKernel.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestVcuKernel::TestVcuKernel()
  : TestRunner("vcu", 
                "Run decoder test", 
                "transcode.xclbin"){}

boost::property_tree::ptree
TestVcuKernel::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  std::cout << "vcu runTest start!\n Running..." << std::endl;
  runTest(dev, ptree);
  std::cout << "vcu runTest finished!\n Exiting..." << std::endl;
  return ptree;
}

void
TestVcuKernel::runTest(std::shared_ptr<xrt_core::device> dev, boost::property_tree::ptree& ptree)
{
  const auto bdf_tuple = xrt_core::device_query<xrt_core::query::pcie_bdf>(dev);
  const std::string bdf = xrt_core::query::pcie_bdf::to_string(bdf_tuple);
  const std::string test_path = findPlatformPath(dev, ptree);
  const std::string b_file = findXclbinPath(dev, ptree); // transcode.xclbin

  xrt::device device(dev->get_device_id());

  logger(ptree, "Details", "Test not supported.");
  ptree.put("status", test_token_skipped);
  return;
}
