// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestIOPS_h_
#define __TestIOPS_h_

#include "tools/common/TestRunner.h"

class TestIOPS : public TestRunner {
  public:
    boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);
    void runTest(std::shared_ptr<xrt_core::device> dev, boost::property_tree::ptree& ptree);
    TestIOPS();

  private:
    void testMultiThreads(const std::string& dev, const std::string& xclbin_fn, int threadNumber, int queueLength, unsigned int total, boost::property_tree::ptree& ptree);
};

#endif
