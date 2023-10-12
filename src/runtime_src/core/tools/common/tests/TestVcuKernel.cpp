// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestVcuKernel.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// #include <algorithm>
// #include <vector>
// #include "vcu_kernel_util/plugin_dec.h"

// #define TEST_INSTANCE_ID 0

// ----- C L A S S   M E T H O D S -------------------------------------------
TestVcuKernel::TestVcuKernel()
  : TestRunner("vcu", 
                "Run decoder test", 
                "transcode.xclbin"){}

boost::property_tree::ptree
TestVcuKernel::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  runTestCase(dev, "xcl_vcu_test.exe", ptree);
  return ptree;
}

// void
// TestVcuKernel::runTest(std::shared_ptr<xrt_core::device> dev, boost::property_tree::ptree& ptree)
// {
  // const auto bdf_tuple = xrt_core::device_query<xrt_core::query::pcie_bdf>(dev);
  // const std::string bdf = xrt_core::query::pcie_bdf::to_string(bdf_tuple);
  // const std::string test_path = findPlatformPath(dev, ptree);
  // const std::string b_file = findXclbinPath(dev, ptree); // transcode.xclbin
  // bool flag_s = false;

  // xrt::device device(dev->get_device_id());

  // if (test_path.empty()) {
  //   logger(ptree, "Error", "Platform test path was not found.");
  //   ptree.put("status", test_token_failed);
  //   return;
  // }

  // auto retVal = validate_binary_file(b_file);
  // if (flag_s || retVal == EOPNOTSUPP) {
  //   ptree.put("status", test_token_skipped);
  //   return;
  // } else if (retVal != EXIT_SUCCESS) {
  //   logger(ptree, "Error", "Unknown error validating transcode xclbin");
  //   ptree.put("status", test_token_failed);
  //   return;
  // }

  // // auto devices = xcl::get_xil_devices();
  // // auto pos = dev_id.find(":");
  // // int device_index = -1;
  // // if (pos == std::string::npos) {
  // //   device_index = stoi(dev_id);
  // // } else {
  // //   if (xcl::is_emulation()) {
  // //     std::cout << "Device bdf is not supported for the emulation flow\n";
  // //     return EXIT_FAILURE;
  // //   }

  // //   char device_bdf[20];
  // //   cl_int err;
  // //   for (uint32_t i = 0; i < devices.size(); i++) {
  // //     OCL_CHECK(err, err = devices[i].getInfo(CL_DEVICE_PCIE_BDF, &device_bdf));
  // //     if (dev_id == device_bdf) {
  // //       device_index = i;  
  // //       break;
  // //     }
  // //   }
  // // }

  // // Harcoding the number of processes/instances 
  // int ret = vcu_dec_test(b_file, TEST_INSTANCE_ID, device_index);
  // if (ret == FALSE) {
  //   ptree.put("status", test_token_failed);
  //   return;
  // }
  // else if (ret == NOTSUPP) {
  //   logger(ptree, "Details", "Test not supported on this device.");
  //   ptree.put("status", test_token_skipped);
  //   return;
  // }

  // ptree.put("status", test_token_passed);
// }
