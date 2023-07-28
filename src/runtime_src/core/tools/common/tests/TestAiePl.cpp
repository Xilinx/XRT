// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestAiePl.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestAiePl::TestAiePl()
  : TestRunner("aie", 
                "Run AIE PL test", 
                "aie_control_config.json"){}

boost::property_tree::ptree
TestAiePl::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  runTestCase(dev, "aie_pl.exe", ptree);
  return ptree;
}
