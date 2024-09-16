// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _TESTSPATIALSHARINGOVD_
#define _TESTSPATIALSHARINGOVD_

#include "tools/common/TestRunner.h"

// Class representing the TestSpatialSharingOvd test
class TestSpatialSharingOvd : public TestRunner {
public:
  boost::property_tree::ptree ptree;

  boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);

  // Constructor to initialize the test runner with a name and description
  TestSpatialSharingOvd()
    : TestRunner("spatial-sharing-overhead", "Run Spatial Sharing Overhead Test"), ptree(get_test_header()){}
};

#endif
