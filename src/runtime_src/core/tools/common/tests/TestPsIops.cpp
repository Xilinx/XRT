// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestPsIops.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestPsIops::TestPsIops()
  : TestRunner("ps-iops", 
                "Run IOPS PS test", 
                "ps_validate.xclbin",
                true){}

boost::property_tree::ptree
TestPsIops::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.put("xclbin_directory", "/lib/firmware/xilinx/ps_kernels/");
  runTestCase(dev, "ps_iops_test.exe", ptree);
  return ptree;
}
