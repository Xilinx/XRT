// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestNPULatency_h_
#define __TestNPULatency_h_

#include "tools/common/TestRunner.h"
#include "xrt/xrt_device.h"

// Forward declaration
namespace xrt_core { class archive; }

class TestNPULatency : public TestRunner {
  public:
    boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);
    
    // Archive-aware version - extracts test artifacts from archive
    boost::property_tree::ptree 
    run (std::shared_ptr<xrt_core::device> dev, 
        const xrt_core::archive* archive) override;

  public:
    TestNPULatency();
};

#endif
