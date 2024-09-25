// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _TESTSPATIALSHARINGOVD_
#define _TESTSPATIALSHARINGOVD_

#include "tools/common/TestRunner.h"

/**
*  @brief Test control flow: 
*
* Two threads are spawned to run two instances of testcases concurrently. There's one hardware context created on each thread so 
* the 2 threads are doing spatial sharing. The threads are added to the threads vector and started using the runTestcase() lambda. 
* The program then waits for both threads to signal they are ready using wait_for_threads_ready().
* Once ready, the latency for running the test cases in parallel is measured by recording the start and 
* end times around the join calls for each thread. After the first run, the thread_ready counter is reset, 
* and a second run is performed with a single TestCase instance executed in a single thread without spatial sharing. 
* The latency for this single-threaded run is similarly measured. 
* Finally, the latencies for both runs are logged to assess the overhead of running the test cases in parallel versus sequentially.
* 
* @see runTestcase()
* @see wait_for_threads_ready()
* @see std::thread::join()

   * | col1         | col2       | col3       | col4       | col5       | col6       | col7       | col8       |
   * |--------------|------------|------------|------------|------------|------------|------------|------------|
   * |                      shared 4x1                     |                   shared 4x1                      |
*/

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
