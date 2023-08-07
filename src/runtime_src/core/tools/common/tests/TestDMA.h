// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestDMA_h_
#define __TestDMA_h_

#include "tools/common/TestRunner.h"

class TestDMA : public TestRunner {
  public:
    boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);
    void set_param(const std::string key, const std::string value);

  public:
    TestDMA();
  
  private:
    size_t m_block_size = 16 * 1024 * 1024; //16MB
};

#endif
