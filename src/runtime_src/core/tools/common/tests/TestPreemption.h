// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef TestPreemptionOverhead_h_
#define TestPreemptionOverhead_h_

#include "tools/common/TestRunner.h"
#include "xrt/xrt_device.h"

class TestPreemptionOverhead : public TestRunner {
  public:
    boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev) override;
    double run_preempt_test(const std::shared_ptr<xrt_core::device>& device, boost::property_tree::ptree& ptree, int no_of_cols, const std::string& level);

  public:
    TestPreemptionOverhead();
};

#endif
