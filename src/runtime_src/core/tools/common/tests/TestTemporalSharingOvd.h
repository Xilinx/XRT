// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _TESTTEMPORALSHARINGOVD_
#define _TESTTEMPORALSHARINGOVD_

#include "tools/common/TestRunner.h"
#include "TestValidateUtilities.h"

/*   
  *  @brief Test control flow:

  * Two threads are spawned to run two instances of testcases concurrently. There's one hardware context created on each thread so
  * the 2 threads are doing spatial sharing. The threads are added to the threads vector and started using the runTestcase() lambda.
  * Once ready, the latency for running the test cases in parallel is measured by recording the start and
  * end times around the join calls for each thread. The second run is performed with three TestCase instances executed in 3 threads
  * with 1 and 3 doing temporal sharing and 1 and 2 doing spatial sharing. The extra time taken for run corresponds to the overhead 
  * going into context switching for temporal sharing. 
  * 
   * | col1         | col2       | col3       | col4       | col5       | col6       | col7       | col8       |
   * |--------------|------------|------------|------------|------------|------------|------------|------------|
   * |                      shared 4x1                     |                   shared 4x1                      |
   * |--------------|------------|------------|------------|------------|------------|------------|------------|
   * |                      shared 4x1                     |                   shared 4x1                      |
*/

class TestTemporalSharingOvd : public TestRunner {
public:
  boost::property_tree::ptree ptree;
  boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);
  void initializeTests(std::vector<TestCase>&);
  TestTemporalSharingOvd()
      : TestRunner("temporal-sharing-overhead", "Run Temporal Sharing Overhead Test"), ptree(get_test_header()) {}
};

#endif
