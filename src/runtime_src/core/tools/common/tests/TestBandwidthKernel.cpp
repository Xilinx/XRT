// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestBandwidthKernel.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;


// ----- C L A S S   M E T H O D S -------------------------------------------
TestBandwidthKernel::TestBandwidthKernel()
  : TestRunner("mem-bw", 
              "Run 'bandwidth kernel' and check the throughput", 
              "bandwidth.xclbin"){}

boost::property_tree::ptree
TestBandwidthKernel::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  std::string name;
  try {
    name = xrt_core::device_query<xrt_core::query::rom_vbnv>(dev);
  } catch (const std::exception&) {
    logger(ptree, "Error", "Unable to find device VBNV");
    ptree.put("status", test_token_failed);
    return ptree;
  }
  std::string testcase = (name.find("vck5000") != std::string::npos) ? "versal_23_bandwidth.py" : "23_bandwidth.py";
  runTestCase(dev, testcase, ptree);
  
  return ptree;
}
