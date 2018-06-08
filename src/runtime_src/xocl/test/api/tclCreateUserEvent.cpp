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

#include "xocl/core/context.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/event.h"
#include <vector>

BOOST_AUTO_TEST_SUITE ( test_clCreateUserEvent )

BOOST_AUTO_TEST_CASE( test_clCreateUserEvent1 )
{
  ocl_sw_emulation ocl;
  cl_int err = CL_SUCCESS;

  auto cq = clCreateCommandQueue(ocl.context,ocl.device,0,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  // Create a user cl_event
  auto cev0 = clCreateUserEvent(ocl.context,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);
  BOOST_CHECK_EQUAL(xocl::xocl(cev0)->get_status(),CL_SUBMITTED);

  // Dependent events are create in xocl domain
  auto xc = xocl::xocl(ocl.context);
  auto xq = xocl::xocl(cq);

  // Wait on user event
  std::vector<cl_event> waitlist;
  waitlist.push_back(cev0);

  // xocl event 
  xocl::event xev0(xq,xc,0,1,waitlist.data());
  xev0.queue();
  BOOST_CHECK_EQUAL(xev0.get_status(),CL_QUEUED);

  // Mark user event complete, which in turn submits
  // the waiting event (xev0).
  err = clSetUserEventStatus(cev0,CL_COMPLETE);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  clFinish(cq);
  BOOST_CHECK_EQUAL(xev0.get_status(),CL_COMPLETE);
}

BOOST_AUTO_TEST_SUITE_END()


