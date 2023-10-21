// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestDF_bandwidth_h_
#define __TestDF_bandwidth_h_

#include "tools/common/TestRunner.h"
#include "xrt/xrt_device.h"

class TestDF_bandwidth : public TestRunner {
  public:
    boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);

  public:
    TestDF_bandwidth();
};

#endif
