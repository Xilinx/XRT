// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestBist_h_
#define __TestBist_h_

#include "tools/common/TestRunner.h"
#include "core/include/ert.h"

class TestBist : public TestRunner {
  public:
    boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);

  //helper functions
  private:
  bool bist_alloc_execbuf_and_wait(const std::shared_ptr<xrt_core::device>& device, 
                enum ert_cmd_opcode opcode, boost::property_tree::ptree& _ptTest);
  bool clock_calibration(const std::shared_ptr<xrt_core::device>& _dev, 
                boost::property_tree::ptree& _ptTest);
  bool ert_validate(const std::shared_ptr<xrt_core::device>& _dev, 
                boost::property_tree::ptree& _ptTest);


  public:
    TestBist();
};

#endif
