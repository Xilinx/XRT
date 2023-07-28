// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestPsIops_h_
#define __TestPsIops_h_

#include "tools/common/TestRunner.h"

class TestPsIops : public TestRunner {
  public:
    boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);

  public:
    TestPsIops();
};

#endif
