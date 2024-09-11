// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _TESTSPATIALSHARINGOVD_
#define _TESTSPATIALSHARINGOVD_

#include <memory>
#include "tools/common/TestRunner.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"
class BO_set {
  xrt::bo bo_instr;
  xrt::bo bo_ifm;
  xrt::bo bo_param;
  xrt::bo bo_ofm;
  xrt::bo bo_inter;
  xrt::bo bo_mc;
  uint32_t instr_size;

public:
  BO_set(xrt::device& device_ptr, xrt::kernel& kernel);
  void set_kernel_args(xrt::run& run);
  void sync_bos_to_device();
};

class TestSpatialSharingOvd : public TestRunner {
  void wait_for_threads_ready(uint32_t, std::mutex&, std::condition_variable&, uint32_t&);
public:
  boost::property_tree::ptree ptree;   
  boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);
  TestSpatialSharingOvd()
    : TestRunner("spatial-sharing-overhead", "Run Spatial Sharing Overhead Test"), ptree(get_test_header()){}
};

class TestCase : public TestSpatialSharingOvd {
  xrt::device device;
  xrt::xclbin xclbin;
  std::string kernel_name;
  xrt::hw_context hw_ctx;
  uint32_t queue_len = 4;
  void thread_ready_to_run(std::mutex&, std::condition_variable&, uint32_t&); 

public:

  TestCase(xrt::xclbin& xclbin, std::string& kernel)
      : device(xrt::device(0)), xclbin(xclbin), kernel_name(kernel) 
  {
    device.register_xclbin(xclbin);
    hw_ctx = xrt::hw_context(device, xclbin.get_uuid());
  }
  void run(std::mutex&, std::condition_variable&, uint32_t&);
};

#endif
