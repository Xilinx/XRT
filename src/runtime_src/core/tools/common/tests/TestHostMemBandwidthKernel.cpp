// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestHostMemBandwidthKernel.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;


// ----- C L A S S   M E T H O D S -------------------------------------------
TestHostMemBandwidthKernel::TestHostMemBandwidthKernel()
  : TestRunner("hostmem-bw", 
              "Run 'bandwidth kernel' when host memory is enabled", 
              "bandwidth.xclbin"){}

boost::property_tree::ptree
TestHostMemBandwidthKernel::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  uint64_t shared_host_mem = 0;
  try {
    shared_host_mem = xrt_core::device_query<xrt_core::query::shared_host_mem>(dev);
  } catch (const std::exception& ) {
    logger(ptree, "Details", "Address translator IP is not available");
    ptree.put("status", test_token_skipped);
    return ptree;
  }

  if (!shared_host_mem) {
      logger(ptree, "Details", "Host memory is not enabled");
      ptree.put("status", test_token_skipped);
      return ptree;
  }
  runTestCase(dev, "host_mem_23_bandwidth.py", ptree);
  return ptree;
}
