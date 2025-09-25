// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef TestNPULatency_h_
#define TestNPULatency_h_

#include "tools/common/TestRunner.h"
#include "xrt/xrt_device.h"

class TestNPULatency : public TestRunner {
  public:
    boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&) override;
    
    // Archive-aware version - extracts test artifacts from archive
    boost::property_tree::ptree 
    run (const std::shared_ptr<xrt_core::device>&,
        const xrt_core::archive* archive) override;

  public:
    TestNPULatency();
};

#endif
