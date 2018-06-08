/**
 * Copyright (C) 2016-2017 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include <boost/test/unit_test.hpp>
#include "setup.h"

#include "xocl/core/device.h"
#include "xocl/core/memory.h"

// To run all tests in this suite use
//  % em -env opt txocl --run_test=test_clReleaseMemObject
// To run selective tests in this suite use
//  % em -env opt txocl --run_test=test_clReleastMemObject/<test case name>

// test_clReleaseMemObject1
//  Ensure that clReleaseMemObject obeys 
//    "After the memobj reference count becomes zero *and* commands queued
//    for execution on a command-queue(s) that use memobj have
//    finished, the memory object is deleted."
//  This is implemented by clReleaseMemObject essentially performing a
//  clFinish on all command queues in the context before deleting the 
//  the mem object.   As the comments note, it is easy to cause a hang.

BOOST_AUTO_TEST_SUITE ( test_clReleaseMemObject )

BOOST_AUTO_TEST_CASE( test_clReleaseMemObject1 )
{
  ocl_sw_emulation ocl;
  cl_int err = CL_SUCCESS;

  auto cq = clCreateCommandQueue(ocl.context,ocl.device,0,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  auto sz=120;
  auto mem = clCreateBuffer(ocl.context,CL_MEM_READ_WRITE,sz,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);
  auto map = clEnqueueMapBuffer(cq,mem,CL_TRUE,CL_MAP_WRITE,0,sz,0,0,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  auto user_event = clCreateUserEvent(ocl.context,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  // Induce hang when commenting out this line, because following
  // unmap will never submit and release mem object will wait for
  // all commands to finish.
  err = clSetUserEventStatus(user_event,CL_COMPLETE);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  cl_event unmap_event = nullptr;
  err = clEnqueueUnmapMemObject(cq,mem,map,1,&user_event,&unmap_event);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  // This will hang if user event was not marked complete first 
  //  After the memobj reference count becomes zero *and* commands queued
  //  for execution on a command-queue(s) that use memobj have
  //  finished, the memory object is deleted.
  // The memobject refcount is 1, since no enqueue command shares
  // ownership of the mem object and the memobject is now owned by
  // any other object.
  err = clReleaseMemObject(mem);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  err = clWaitForEvents(1,&unmap_event);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);
}

BOOST_AUTO_TEST_SUITE_END()



