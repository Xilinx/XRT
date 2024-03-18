// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestTCTAllColumn_h_
#define __TestTCTAllColumn_h_

#include "tools/common/TestRunner.h"
#include "xrt/xrt_device.h"

class TestTCTAllColumn : public TestRunner {
  public:
    boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);

  public:
    TestTCTAllColumn();
};

#endif
