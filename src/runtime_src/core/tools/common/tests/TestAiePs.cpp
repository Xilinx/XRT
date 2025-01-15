// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestAiePs.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBUtilitiesCore.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
namespace XBU = XBUtilities;

#define WIDTH 8
#define HEIGHT 8 
#define SIZE (WIDTH * HEIGHT)

#ifdef _WIN32
#pragma warning(disable : 4702) //TODO remove when test is implemented properly
#endif

// ----- C L A S S   M E T H O D S -------------------------------------------
TestAiePs::TestAiePs()
  : TestRunner("ps-aie", 
                "Run PS controlled AIE test", 
                "ps_aie.xclbin",
                true){}

boost::property_tree::ptree
TestAiePs::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.put("xclbin_directory", "/lib/firmware/xilinx/ps_kernels/");
  runTest(dev, ptree);
  return ptree;
}

void
TestAiePs::runTest(std::shared_ptr<xrt_core::device> dev, boost::property_tree::ptree& ptree)
{
  xrt::device device(dev);

  XBValidateUtils::logger(ptree, "Details", "Test not supported.");
  ptree.put("status", XBValidateUtils::test_token_skipped);
  return;

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

  const int input_size_in_bytes = SIZE * sizeof(float);
  const int output_size_in_bytes = SIZE * sizeof(float);
  const int input_size_allocated = ((input_size_in_bytes / 4096) + ((input_size_in_bytes % 4096) > 0)) * 4096;
  const int output_size_allocated = ((output_size_in_bytes / 4096) + ((output_size_in_bytes % 4096) > 0)) * 4096;

  auto uuid = device.load_xclbin(b_file);
  auto aie_kernel = xrt::kernel(device,uuid, "aie_kernel");
  auto out_bo= xrt::bo(device, output_size_allocated, aie_kernel.group_id(2));
  auto out_bomapped = out_bo.map<float*>();
  memset(out_bomapped, 0, output_size_in_bytes);

  auto in_bo_a = xrt::bo(device, input_size_allocated, aie_kernel.group_id(0));
  auto in_bomapped_a = in_bo_a.map<float*>();
  auto in_bo_b = xrt::bo(device, input_size_allocated, aie_kernel.group_id(1));
  auto in_bomapped_b = in_bo_b.map<float*>();

  //setting input data
  std::vector<float> golden(SIZE);
  for (int i = 0; i < SIZE; i++){
    in_bomapped_a[i] = static_cast<float>(rand() % SIZE);
    in_bomapped_b[i] = static_cast<float>(rand() % SIZE);
  }
  for (int i = 0; i < HEIGHT ; i++) {
    for (int j = 0; j < WIDTH ; j++){
        golden[i*WIDTH+j] = 0;
        for (int k=0; k <WIDTH; k++)
    golden[i*WIDTH+j] += in_bomapped_a[i*WIDTH + k] * in_bomapped_b[k+WIDTH * j];
    }
  } 

  in_bo_a.sync(XCL_BO_SYNC_BO_TO_DEVICE, input_size_in_bytes, 0);
  in_bo_b.sync(XCL_BO_SYNC_BO_TO_DEVICE, input_size_in_bytes, 0);

  auto run = aie_kernel(in_bo_a, in_bo_b, out_bo, input_size_in_bytes, output_size_in_bytes);
  run.wait();

  out_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, output_size_in_bytes, 0);
  
  for (int i = 0; i < SIZE; i++) {
    if (out_bomapped[i] != golden[i]) {
      XBValidateUtils::logger(ptree, "Error", boost::str(boost::format("Error found in sample %d: golden: %f, hardware: %f") % i % golden[i] % out_bomapped[i]));
      ptree.put("status", XBValidateUtils::test_token_failed);
      return;
    }
  }

  ptree.put("status", XBValidateUtils::test_token_passed);
}
