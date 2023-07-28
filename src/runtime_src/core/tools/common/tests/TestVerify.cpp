// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestVerify.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestVerify::TestVerify()
  : TestRunner("verify", 
                "Run 'Hello World' kernel test", 
                "verify.xclbin", 
                false){}

boost::property_tree::ptree
TestVerify::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  runTestCase(dev, "22_verify.py", ptree);
  return ptree;
}
