// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2026 Advanced Micro Devices, Inc. All rights reserved.

#ifndef TestRunlistThroughput_h_
#define TestRunlistThroughput_h_

#include "tools/common/TestRunner.h"
#include "core/common/json/nlohmann/json.hpp"
#include "xrt/xrt_device.h"

class TestRunlistThroughput : public TestRunner {
  public:
    boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&, const xrt_core::archive*) override;

    TestRunlistThroughput();

  private:
    static double
    get_runlist_throughput_from_report(const nlohmann::json& report);

    static double
    get_ops_throughput_from_report(const nlohmann::json& report);
};

#endif
