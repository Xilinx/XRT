// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef TestCmdChainLatency_h_
#define TestCmdChainLatency_h_

#include "tools/common/TestRunner.h"
#include "xrt/xrt_device.h"

namespace xrt_core { class archive; }

class TestCmdChainLatency : public TestRunner {
  public:
    boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&) override;
    boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&, const xrt_core::archive*);

  public:
    TestCmdChainLatency();
};

#endif
