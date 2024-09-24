// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _TESTTEMPORALSHARINGOVD_
#define _TESTTEMPORALSHARINGOVD_

#include "tools/common/TestRunner.h"

class TestTemporalSharingOvd : public TestRunner {
public:
  boost::property_tree::ptree ptree;
  boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);
  TestTemporalSharingOvd()
      : TestRunner("temporal-sharing-overhead", "Run Temporal Sharing Overhead Test"), ptree(get_test_header()) {}
};

#endif