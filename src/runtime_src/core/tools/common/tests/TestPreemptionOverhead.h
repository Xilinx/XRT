// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef TestPreemptionOverhead_h_
#define TestPreemptionOverhead_h_

#include "tools/common/TestRunner.h"
#include "xrt/xrt_device.h"

class TestPreemptionOverhead : public TestRunner {
  public:
    boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&) override;
    boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&, const xrt_core::archive*) override;

  public:
    TestPreemptionOverhead();
};

#endif
