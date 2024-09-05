// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _TESTSPATIALSHARINGOVD_
#define _TESTSPATIALSHARINGOVD_

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
    BO_set(xrt::device* device_ptr, xrt::kernel* kernel);
    void set_kernel_args(xrt::run* run);
    void sync_bos_to_device();
};

class TestCase {
    // test cases to run in run_test
    public:
    xrt::device device;
    xrt::hw_context *hw_ctx;
    xrt::xclbin xclbin;
    std::string kernel_name;
    uint32_t queue_len = 4;
    std::string priority = "normal";

    TestCase(std::string& xclbin_str, std::string& kernel);
    void run();
};

class TestSpatialSharingOvd : public TestRunner {
  public:
    boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);
    //void runTestcase(TestCase* test);
    TestSpatialSharingOvd();
};
#endif