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

#include <CL/opencl.h>
#include "xocl/core/device.h"
#include "xocl/core/context.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/event.h"
#include "xocl/core/memory.h"

// To run all tests in this suite use
//  % em -env opt txocl --run_test=test_RefCount
// To run selective tests in this suite use
//  % em -env opt txocl --run_test=test_RefCount/<test case name>

// test_RefCount1
//  Verify that APIs return CL objects with refcount equal 1

namespace {

template <typename OclType>
void check_equal(OclType* cl, unsigned int count = 1)
{
  BOOST_CHECK_EQUAL(xocl::xocl(cl)->count(),count);
}

}

BOOST_AUTO_TEST_SUITE ( test_RefCount )

BOOST_AUTO_TEST_CASE( test_RefCount1 )
{
  cl_int err = CL_SUCCESS;
  cl_platform_id platform = nullptr;
  BOOST_CHECK_EQUAL(clGetPlatformIDs(1,&platform,nullptr),CL_SUCCESS);

  // Device is owned by platform, we don't know how many reference it keeps
  // Calling clGetDeviceIDs does not change the refcount
  cl_device_id device = nullptr;
  BOOST_CHECK_EQUAL(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ACCELERATOR, 1, &device, nullptr),CL_SUCCESS);
  auto save = xocl::xocl(device)->count();
  cl_device_id device_copy = nullptr;
  BOOST_CHECK_EQUAL(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ACCELERATOR, 1, &device_copy, nullptr),CL_SUCCESS);
  BOOST_CHECK_EQUAL(device,device_copy);
  check_equal(device_copy,save);

  // Context keeps a reference to a device
  cl_context context = clCreateContext(0, 1, &device, nullptr, nullptr, &err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);
  check_equal(context,1);
  check_equal(device,save+1);

  auto cq = clCreateCommandQueue(context,device,0,&err);
  check_equal(cq,1);

  auto ev = clCreateUserEvent(context,nullptr);
  check_equal(ev,1);
  clSetUserEventStatus(ev,CL_COMPLETE);
  check_equal(ev,1);

  auto mem = clCreateBuffer(context,0,128,nullptr,nullptr);
  check_equal(mem,1);

  clReleaseMemObject(mem);
  clReleaseEvent(ev);
  clReleaseCommandQueue(cq);
}

BOOST_AUTO_TEST_SUITE_END()



