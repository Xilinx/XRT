// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestVerify.h"
#include "TestIPU.h"
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
  auto device_name = xrt_core::device_query_default<xrt_core::query::rom_vbnv>(dev, "");
  boost::property_tree::ptree ptree;
  //to-do: Do it in a cleaner way
  if (device_name.find("RyzenAI-Strix") != std::string::npos || device_name.find("RyzenAI-Phoenix") != std::string::npos) {
    //run ipu verify
    ptree = TestIPU{}.run(dev);
  }
  else {
    ptree = get_test_header();
    runTestCase(dev, "22_verify.py", ptree);
  }
  return ptree;
}
