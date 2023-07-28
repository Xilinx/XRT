// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestPsPlVerify.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestPsPlVerify::TestPsPlVerify()
  : TestRunner("ps-pl-verify", 
                "Run PS controlled 'Hello World' PL kernel test", 
                "ps_bandwidth.xclbin"){}

boost::property_tree::ptree
TestPsPlVerify::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.put("xclbin_directory", "/lib/firmware/xilinx/ps_kernels/");
  runTestCase(dev, "ps_bandwidth.exe", ptree);
  return ptree;
}
