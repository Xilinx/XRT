// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __Testm2m_h_
#define __Testm2m_h_

#include "tools/common/TestRunner.h"
#include "xrt/xrt_bo.h"

class Testm2m : public TestRunner {
  public:
    boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);
  
  // helper functions
  private:
    double m2mtest_bank(const std::shared_ptr<xrt_core::device>& handle,
             boost::property_tree::ptree& _ptTest,
             uint32_t bank_a, uint32_t bank_b, size_t bo_size);
    xrt::bo m2m_alloc_init_bo(const xrt::device& device, boost::property_tree::ptree& _ptTest,
                  char*& boptr, size_t bo_size, uint32_t bank, char pattern);

  public:
    Testm2m();
};

#endif
