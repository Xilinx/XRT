// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestPcieLink_h_
#define __TestPcieLink_h_

#include "tools/common/TestRunner.h"

class TestPcieLink : public TestRunner {
  public:
    boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);

  public:
    TestPcieLink();
};

#endif
