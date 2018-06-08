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

////////////////////////////////////////////////////////////////
// Unit testing of xrt/util/event.h
////////////////////////////////////////////////////////////////
#include <boost/test/unit_test.hpp>

#include "xrt/util/task.h"
#include "xrt/util/event.h"

#include <chrono>
#include <iostream>

BOOST_AUTO_TEST_SUITE ( test_event )

namespace {

int sleepy_waiter(int i)  
{
  std::this_thread::sleep_for(std::chrono::milliseconds(i));
  return i;
}

struct API
{
  int foo(int i, char ch) { return sleepy_waiter(i); }
  void bar(int i, char ch) { sleepy_waiter(i); }
};

xrt::event
sleepy_event_waiter(xrt::task::queue& q, int i)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(i));
  xrt::event ev(xrt::task::createF(q,&sleepy_waiter,i));
  return ev;
}

}

BOOST_AUTO_TEST_CASE( test_event1 )
{
  xrt::task::queue queue;
  std::vector<std::thread> workers;
  workers.push_back(std::thread(xrt::task::worker,std::ref(queue)));
  workers.push_back(std::thread(xrt::task::worker,std::ref(queue)));

  {
    // Wrap a task::event in an xrt::event
    xrt::event ev(xrt::task::createF(queue,&sleepy_waiter,1000));

    BOOST_CHECK_EQUAL(ev.ready(),false); // 1000 msec not yet passed
    BOOST_CHECK_EQUAL(ev.get<int>(),1000); // blocking wait
    BOOST_CHECK_EQUAL(ev.ready(),true);  // the event is now ready
    BOOST_CHECK_EQUAL(ev.get<int>(),1000); // its ok to get value twice

    // Cast event value to wrong type results in exception
    bool exception=false;
    try {
      ev.get<size_t>();
    } 
    catch (const std::exception& ex) {
      exception=true;
    }

    BOOST_CHECK_EQUAL(true,exception);
  }

  {
    // void event
    API api;
    xrt::event ev(xrt::task::createM(queue,&API::bar,api,1000,'x'));

    BOOST_CHECK_EQUAL(ev.ready(),false); // 1000 msec not yet passed
    ev.get<void>();
    BOOST_CHECK_EQUAL(ev.ready(),true);  // the event is now ready
    ev.get<void>();
    
  }

  {
    // event assignment
    xrt::event ev1;
    xrt::event ev2;
    ev1 = std::move(ev2);
  }

  queue.stop();
  for (auto& t : workers)
    t.join();
}

BOOST_AUTO_TEST_CASE( test_event2 )
{
  xrt::task::queue queue;
  std::vector<std::thread> workers;
  workers.push_back(std::thread(xrt::task::worker,std::ref(queue)));
  workers.push_back(std::thread(xrt::task::worker,std::ref(queue)));

  {
    // event of event, the hard way
    xrt::event ev(xrt::task::createF(queue,&sleepy_event_waiter,std::ref(queue),1000));
    BOOST_CHECK_EQUAL(ev.ready(),false); // 1000 msec not yet passed
    auto evr = std::move(ev.get<xrt::event>());
    BOOST_CHECK_EQUAL(ev.ready(),true);  // the event is now ready
    BOOST_CHECK_EQUAL(evr.ready(),false); //1000 msec not yet passed
    BOOST_CHECK_EQUAL(evr.get<int>(),1000); // its ok to get value twice
    BOOST_CHECK_EQUAL(evr.ready(),true); //1000 msec not yet passed
  }

  queue.stop();
  for (auto& t : workers)
    t.join();
}

BOOST_AUTO_TEST_SUITE_END()



