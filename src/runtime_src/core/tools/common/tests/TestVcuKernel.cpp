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
  runTest(dev, ptree);
  return ptree;
}

void
TestVcuKernel::runTest(std::shared_ptr<xrt_core::device> dev, boost::property_tree::ptree& ptree)
{
  logger(ptree, "Details", "Test not supported.");
  ptree.put("status", test_token_skipped);
  return;
}
