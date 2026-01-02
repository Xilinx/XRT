// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef TESTTEMPORALSHARINGOVD_
#define TESTTEMPORALSHARINGOVD_

#include "tools/common/TestRunner.h"

/**
*  @brief Test control flow: 
*
* Two threads are spawned to run two instances of testcases concurrently. There's one hardware context created on each thread so 
* the 2 threads are doing temporal sharing. The threads are added to the threads vector and started using the run recipe. 
* The latency for this single-threaded run is similarly measured. 
* Finally, the latencies for both runs are logged to assess the overhead of running the test cases in parallel versus sequentially.
* 
* @see runTestcase()

   * | col1         | col2       | col3       | col4       | col5       | col6       | col7       | col8       |
   * |--------------|------------|------------|------------|------------|------------|------------|------------|
   * |                                                 shared 8x1                                              |
   * |                                                 shared 8x1                                              |
*/

// Class representing the TestSpatialSharingOvd test
class TestTemporalSharingOvd : public TestRunner {
  double 
  get_total_frame_events(const std::shared_ptr<xrt_core::device>& dev);

public:
  boost::property_tree::ptree 
  run(const std::shared_ptr<xrt_core::device>&) override;

  boost::property_tree::ptree 
  run(const std::shared_ptr<xrt_core::device>&, 
      const xrt_core::archive*) override;

  // Constructor to initialize the test runner with a name and description
  TestTemporalSharingOvd()
    : TestRunner("temporal-sharing-overhead", "Run Temporal Sharing Overhead Test"){}
};

#endif
