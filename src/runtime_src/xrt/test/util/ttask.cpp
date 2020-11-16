/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

////////////////////////////////////////////////////////////////
// Unit testing of xrt/util/task.h
////////////////////////////////////////////////////////////////
#include <boost/test/unit_test.hpp>

#include "xrt/util/task.h"

#include <chrono>
#include <iostream>

BOOST_AUTO_TEST_SUITE ( test_task )

namespace {

static int sleepy_waiter(int i)  
{
  std::this_thread::sleep_for(std::chrono::milliseconds(i));
  return i;
}

static bool noargs()
{
  return true;
}

struct API
{
  int  foo(int i, char ch) { return sleepy_waiter(i); }
  bool noargs() { return true; }
};

}

BOOST_AUTO_TEST_CASE( test_task1 )
{
  xrt_xocl::task::queue queue;
  std::vector<std::thread> workers;
  workers.push_back(std::thread(xrt_xocl::task::worker,std::ref(queue)));
  workers.push_back(std::thread(xrt_xocl::task::worker,std::ref(queue)));

  {
    // create task from free function with args
    auto tev = xrt_xocl::task::createF(queue,&sleepy_waiter,1000);
    BOOST_CHECK_EQUAL(tev.ready(),false);
    BOOST_CHECK_EQUAL(tev.get(),1000);

    bool exception=false;
    try {
      tev.get();
    } 
    catch (const std::exception& ex) {
      exception=true;
    }

    BOOST_CHECK_EQUAL(true,exception);
  }

  {
    // create task from member function with args
    API api;
    auto tev = xrt_xocl::task::createM(queue,&API::foo,api,100,'a');
    BOOST_CHECK_EQUAL(tev.get(),100);
  }

  {
    // create task from free function without args
    auto tev = xrt_xocl::task::createF(queue,&noargs);
    BOOST_CHECK_EQUAL(tev.get(),true);
  }

  {
    // create task from member function without args
    API api;
    auto tev = xrt_xocl::task::createM(queue,&API::noargs,api);
    BOOST_CHECK_EQUAL(tev.get(),true);
  }

  {
    // test exception when calling get() twice
    auto tev = xrt_xocl::task::createF(queue,&sleepy_waiter,1);
    BOOST_CHECK_EQUAL(tev.get(),1);
    bool exception=false;
    try {
      tev.get();
    } 
    catch (const std::exception& ex) {
      exception=true;
    }
    BOOST_CHECK_EQUAL(true,exception);
  }

  queue.stop();
  for (auto& t : workers)
    t.join();
}

BOOST_AUTO_TEST_SUITE_END()



