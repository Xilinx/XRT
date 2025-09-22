// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef TestPsIops_h_
#define TestPsIops_h_

#include "tools/common/TestRunner.h"

class TestPsIops : public TestRunner {
  public:
    boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>& dev) override;
    void runTest(const std::shared_ptr<xrt_core::device>& dev, boost::property_tree::ptree& ptree);
    TestPsIops();

  private:
    void testMultiThreads(const std::string& dev, const std::string& xclbin_fn, int threadNumber, int queueLength, unsigned int total, boost::property_tree::ptree& ptree);
};

#endif
