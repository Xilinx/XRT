// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestPreemption_h_
#define __TestPreemption_h_

#include "tools/common/TestRunner.h"
#include "xrt/xrt_device.h"

class TestPreemption : public TestRunner {
  public:
    boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);
    double run_preempt_test(const std::shared_ptr<xrt_core::device>& device, boost::property_tree::ptree& ptree, int no_of_cols, const std::string& level);

  public:
    TestPreemption();
};

#endif
