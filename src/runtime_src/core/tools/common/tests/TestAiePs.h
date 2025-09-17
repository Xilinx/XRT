// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef TestAiePs_h_
#define TestAiePs_h_

#include "tools/common/TestRunner.h"

class TestAiePs : public TestRunner {
  public:
    boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&) override;
    void runTest(const std::shared_ptr<xrt_core::device>& dev, boost::property_tree::ptree& ptree);

  public:
    TestAiePs();
};

#endif
