// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestPsVerify.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestPsVerify::TestPsVerify()
  : TestRunner("ps-verify", 
                "Run 'Hello World' PS kernel test", 
                "ps_validate.xclbin",
                true){}

boost::property_tree::ptree
TestPsVerify::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.put("xclbin_directory", "/lib/firmware/xilinx/ps_kernels/");
  runTestCase(dev, "ps_validate.exe", ptree);
  return ptree;
}
