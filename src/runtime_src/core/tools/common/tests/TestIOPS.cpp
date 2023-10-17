// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestIOPS.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestIOPS::TestIOPS()
  : TestRunner("iops", 
                "Run scheduler performance measure test", 
                "verify.xclbin"){}

boost::property_tree::ptree
TestIOPS::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  runTestCase(dev, "xcl_iops_test.exe", ptree);
  return ptree;
}
