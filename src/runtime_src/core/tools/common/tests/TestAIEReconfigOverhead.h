// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestArrayReconfOverhead_h_
#define __TestArrayReconfOverhead_h_

#include "tools/common/TestRunner.h"
#include "xrt/xrt_device.h"

class TestAIEReconfigOverhead : public TestRunner {
  public :
    boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);

    TestAIEReconfigOverhead();
};
#endif
