// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestPsVerify_h_
#define __TestPsVerify_h_

#include "tools/common/TestRunner.h"

class TestPsVerify : public TestRunner {
  public:
    boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);
    void runTest(std::shared_ptr<xrt_core::device> dev, boost::property_tree::ptree& ptree);

  public:
    TestPsVerify();
};

#endif
