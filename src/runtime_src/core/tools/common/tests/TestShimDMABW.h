// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

#ifndef TestShimDMABW_h_
#define TestShimDMABW_h_

#include "tools/common/TestRunner.h"
#include "xrt/xrt_device.h"

class TestShimDMABW : public TestRunner {
  public:
    boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&) override;
    boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&, const xrt_core::archive*) override;

  public:
    TestShimDMABW();
};

#endif
