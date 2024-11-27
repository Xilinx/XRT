// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestPsVerify.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBUtilitiesCore.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
namespace XBU = XBUtilities;

static const int COUNT = 1024;

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
  runTest(dev, ptree);
  return ptree;
}

void
TestPsVerify::runTest(std::shared_ptr<xrt_core::device> dev, boost::property_tree::ptree& ptree)
{
  xrt::device device(dev);

  const std::string test_path = XBValidateUtils::findPlatformPath(dev, ptree);
  const std::vector<std::string> dependency_paths = findDependencies(test_path, m_xclbin);
  // Load dependency xclbins onto device if any
  for (const auto& path : dependency_paths) {
    auto retVal = XBValidateUtils::validate_binary_file(path);
    if (retVal == EOPNOTSUPP) {
      ptree.put("status", XBValidateUtils::test_token_skipped);
      return;
    }
    device.load_xclbin(path);
  }

  const std::string b_file = XBValidateUtils::findXclbinPath(dev, ptree);
  // Load ps kernel onto device
  auto retVal = XBValidateUtils::validate_binary_file(b_file);
  if (retVal == EOPNOTSUPP) {
    ptree.put("status", XBValidateUtils::test_token_skipped);
    return;
  }

  auto uuid = device.load_xclbin(b_file);
  auto hello_world = xrt::kernel(device, uuid.get(), "hello_world");
  const size_t DATA_SIZE = COUNT * sizeof(int);
  auto bo0 = xrt::bo(device, DATA_SIZE, hello_world.group_id(0));
  auto bo1 = xrt::bo(device, DATA_SIZE, hello_world.group_id(1));
  auto bo0_map = bo0.map<int*>();
  auto bo1_map = bo1.map<int*>();
  std::fill(bo0_map, bo0_map + COUNT, 0);
  std::fill(bo1_map, bo1_map + COUNT, 0);

  // Fill our data sets with pattern
  bo0_map[0] = 'h';
  bo0_map[1] = 'e';
  bo0_map[2] = 'l';
  bo0_map[3] = 'l';
  bo0_map[4] = 'o';
  for (int i = 5; i < COUNT; ++i) {
    bo0_map[i] = 0;
    bo1_map[i] = i;
  }
  
  bo0.sync(XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0);
  bo1.sync(XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0);
  
  auto run = hello_world(bo0, bo1, COUNT);
  run.wait();
  
  //Get the output;
  bo1.sync(XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE, 0);
  
  // Validate our results
  if (std::memcmp(bo1_map, bo0_map, DATA_SIZE)) {
    XBValidateUtils::logger(ptree, "Error", "Value read back does not match reference");
    ptree.put("status", XBValidateUtils::test_token_failed);
    return;
  }
  ptree.put("status", XBValidateUtils::test_token_passed);
}
