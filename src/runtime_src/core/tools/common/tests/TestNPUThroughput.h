// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef TestNPUThroughput_h_
#define TestNPUThroughput_h_

#include "tools/common/TestRunner.h"
#include "core/common/json/nlohmann/json.hpp"
#include "xrt/xrt_device.h"

class TestNPUThroughput : public TestRunner {
  public:
    boost::property_tree::ptree 
    run(const std::shared_ptr<xrt_core::device>&, const xrt_core::archive*) override;

    TestNPUThroughput();

  private:
    double 
    get_throughput_from_report(const nlohmann::json& report) const;
};

#endif
