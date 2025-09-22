// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef Testp2p_h_
#define Testp2p_h_

#include "tools/common/TestRunner.h"

class Testp2p : public TestRunner {
  public:
    boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&) override;
  
  //helper functions
  private:
  bool p2ptest_bank(xrt_core::device* device, boost::property_tree::ptree& _ptTest, 
             const std::string&,unsigned int mem_idx, uint64_t addr, 
             uint64_t bo_size, uint32_t no_dma);

  public:
    Testp2p();
};

#endif

