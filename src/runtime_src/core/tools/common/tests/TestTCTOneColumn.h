// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef TestTCTOneColumn_h_
#define TestTCTOneColumn_h_

#include "tools/common/TestRunner.h"
#include "xrt/xrt_device.h"

class TestTCTOneColumn : public TestRunner {
  public:
    boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&) override;
    boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&, const xrt_core::archive*) override;

  public:
    TestTCTOneColumn();
};

#endif
