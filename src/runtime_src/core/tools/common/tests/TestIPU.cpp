// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestIPU.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_kernel.h"
namespace XBU = XBUtilities;

static constexpr size_t host_app = 1; //opcode
static constexpr size_t buffer_size = 128;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestIPU::TestIPU()
  : TestRunner("ipu", 
                "Run no-op test on IPU",
                "1x4.xclbin"){}

boost::property_tree::ptree
TestIPU::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();

  xrt::xclbin xclbin;  
  try {
    xclbin = search_drv_store_xclbin(dev, ptree);
  }
  catch (const std::runtime_error& ex) {
    logger(ptree, "Error", ex.what());
    ptree.put("status", test_token_failed);
    return ptree;
  }

  if(!xclbin) { // if xclbin not found
    return ptree;
  }

  // Determine The DPU Kernel Name
  auto xkernels = xclbin.get_kernels();

  auto xkernel = *std::find_if(xkernels.begin(), xkernels.end(), [](xrt::xclbin::kernel& k) {
    auto name = k.get_name();
    return name.rfind("DPU",0) == 0; // Starts with "DPU"
  });
  auto kernelName = xkernel.get_name();
  logger(ptree, "Details", boost::str(boost::format("Kernel name is '%s'") % kernelName));

  dev->register_xclbin(xclbin);
  xrt::hw_context hwctx{xrt::device{dev->get_device_id()}, xclbin.get_uuid()};
  xrt::kernel kernel{hwctx, kernelName};

  //Create BOs
  auto bo_ifm = xrt::bo(xrt::device{dev->get_device_id()}, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(1));
  auto bo_param = xrt::bo(xrt::device{dev->get_device_id()}, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(2));
  auto bo_ofm = xrt::bo(xrt::device{dev->get_device_id()}, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(3));
  auto bo_inter = xrt::bo(xrt::device{dev->get_device_id()}, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(4));
  auto bo_instr = xrt::bo(xrt::device{dev->get_device_id()}, buffer_size, XCL_BO_FLAGS_CACHEABLE, kernel.group_id(5));
  auto bo_mc = xrt::bo(xrt::device{dev->get_device_id()}, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(7));
  std::memset(bo_instr.map<char*>(), buffer_size, '0');

  //Sync BOs
  bo_instr.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_ifm.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_param.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_mc.sync(XCL_BO_SYNC_BO_TO_DEVICE);

  try {
    auto run = kernel(host_app, bo_ifm, bo_param, bo_ofm, bo_inter, bo_instr, buffer_size, bo_mc);
    // Wait for kernel to be done
    run.wait();
  }
  catch (const std::exception& ex) {
    logger(ptree, "Error", ex.what());
    ptree.put("status", test_token_failed);
  }

  ptree.put("status", test_token_passed);
  return ptree;
}