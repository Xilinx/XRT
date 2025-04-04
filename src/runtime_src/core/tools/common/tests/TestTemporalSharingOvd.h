// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _TESTTEMPORALSHARINGOVD_
#define _TESTTEMPORALSHARINGOVD_

#include "tools/common/TestRunner.h"

/**
*  @brief Test control flow: 
*
* Two threads are spawned to run two instances of testcases concurrently. There's one hardware context created on each thread so 
* the 2 threads are doing spatial sharing. The threads are added to the threads vector and started using the runTestcase() lambda. 
* The latency for this single-threaded run is similarly measured. 
* Finally, the latencies for both runs are logged to assess the overhead of running the test cases in parallel versus sequentially.
* 
* @see runTestcase()

   * | col1         | col2       | col3       | col4       | col5       | col6       | col7       | col8       |
   * |--------------|------------|------------|------------|------------|------------|------------|------------|
   * |                      shared 4x1                     |                                                   |
   *                        shared 4x1                     |                                                   |
*/

// Class representing the TestSpatialSharingOvd test
class TestTemporalSharingOvd : public TestRunner {
public:
  boost::property_tree::ptree ptree;

  boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev);

  // Constructor to initialize the test runner with a name and description
  TestTemporalSharingOvd()
  //For the time, the driver mandates even 4 column hardware contexts to 
  //Occupy all 8 columns. Thus the logic for spatial sharing is implementing temporal sharing.
  //This should be renamed back once the MCDM driver switches to spatial sharing.
    : TestRunner("temporal-sharing-overhead", "Run Temporal Sharing Overhead Test"), ptree(get_test_header()){}
};

#endif
