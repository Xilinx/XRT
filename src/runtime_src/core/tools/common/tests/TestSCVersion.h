// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef TestSCVersion_h_
#define TestSCVersion_h_

#include "tools/common/TestRunner.h"

class TestSCVersion : public TestRunner {
  public:
    boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&) override;

  public:
    TestSCVersion();
};

#endif
