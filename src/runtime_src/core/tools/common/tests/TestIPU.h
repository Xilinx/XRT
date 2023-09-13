// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestIPU_h_
#define __TestIPU_h_

#include "tools/common/TestRunner.h"
#include "xrt/xrt_device.h"

class TestIPU : public TestRunner {
  public:
    boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);

  public:
    TestIPU();
};

#endif
