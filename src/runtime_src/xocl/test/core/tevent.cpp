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

#include <boost/test/unit_test.hpp>

#include "xocl/core/object.h"
#include "xocl/core/event.h"
#include "xocl/core/context.h"
#include "xocl/core/platform.h"
#include "xocl/core/command_queue.h"

#include <thread>
#include <iostream>

namespace {

// Thread synchronized printing of messages
template <typename T>
void println(const T& t)
{
#if defined(_debug) || defined(UNIT_VERBOSE)
  static std::mutex mutex;
  std::lock_guard<std::mutex> lk(mutex);
  std::cout << std::this_thread::get_id() << ": " << t << "\n";
#endif
}

}

BOOST_AUTO_TEST_SUITE ( test_event )

BOOST_AUTO_TEST_CASE( test_event_in_order_submit )
{
  xocl::context c(nullptr,0,nullptr);
  xocl::command_queue q(&c,nullptr,0); // in order queue

  {
    // Test wait submit of event with wait list
    // The submit should wait until dependencies are all complete
    xocl::event ev0(&q,&c,0);
    xocl::event ev1(&q,&c,0);

    std::vector<cl_event> waitlist;
    waitlist.push_back(&ev0);
    waitlist.push_back(&ev1);

    xocl::event ev2(&q,&c,0,2,&waitlist[0]);
    xocl::event ev3(&q,&c,0,2,&waitlist[0]);

    // Queue the events. In order queue, implies this is the order in
    // which they will be submitted.
    ev0.queue();
    ev1.queue();
    ev2.queue();
    ev3.queue();
  }
}

BOOST_AUTO_TEST_CASE( test_event_out_order_submit )
{
  xocl::context c(nullptr,0,nullptr);
  xocl::command_queue q(&c,nullptr,CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE); // out of order queue

  {
    // Test wait submit of event with wait list
    // The submit should wait until dependencies are all complete
    xocl::event ev0(&q,&c,0);
    xocl::event ev1(&q,&c,0);

    std::vector<cl_event> waitlist;
    waitlist.push_back(&ev0);
    waitlist.push_back(&ev1);

    xocl::event ev2(&q,&c,0,2,&waitlist[0]);
    xocl::event ev3(&q,&c,0,2,&waitlist[0]);

    // Queue the events. In order queue, implies this is the order in
    // which they will be submitted.
    ev2.queue();
    ev3.queue();
    ev0.queue();
    ev1.queue();
  }
}

BOOST_AUTO_TEST_CASE( test_event_threaded_submit )
{
  xocl::context c(nullptr,0,nullptr);
  xocl::command_queue q(&c,nullptr,CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE); // out of order queue

  {
    // Test wait submit of event with wait list
    // The submit should wait until dependencies are all complete
    xocl::event ev0(&q,&c,0);
    xocl::event ev1(&q,&c,0);

    std::vector<cl_event> waitlist;
    waitlist.push_back(&ev0);
    waitlist.push_back(&ev1);

    xocl::event ev2(&q,&c,0,2,&waitlist[0]);
    xocl::event ev3(&q,&c,0,2,&waitlist[0]);

    auto queue = [](cl_event ev) {
      auto event = xocl::xocl(ev);
      println(std::string("queing event: ") + std::to_string(event->get_uid()));
      event->queue();
    };
    
    std::vector<std::thread> workers;
    workers.push_back(std::thread(queue,&ev3));    // must wait for ev0 and ev1
    workers.push_back(std::thread(queue,&ev2));    // must wait for ev0 and ev1
    workers.push_back(std::thread(queue,&ev0));
    ev1.queue();
    q.flush();

    for (auto& t : workers)
      t.join();
  }
}

BOOST_AUTO_TEST_CASE( test_event_submit_wait )
{
  xocl::context c(nullptr,0,nullptr);
  xocl::command_queue q(&c,nullptr,CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE); // out of order queue

  {
    // Test wait submit of event with wait list
    // The submit should wait until dependencies are all complete
    xocl::event ev0(&q,&c,0);
    xocl::event ev1(&q,&c,0);

    std::vector<cl_event> waitlist;
    waitlist.push_back(&ev0);
    waitlist.push_back(&ev1);

    xocl::event ev2(&q,&c,0,2,&waitlist[0]);
    xocl::event ev3(&q,&c,0,2,&waitlist[0]);

    unsigned long times[4] = {0};

    auto queue = [&times](cl_event ev, unsigned int ms,bool wait,int idx) {
      auto event = xocl::xocl(ev);
      std::this_thread::sleep_for(std::chrono::milliseconds(ms));
      event->queue(wait);
      times[idx]=xrt_xocl::time_ns();
    };

    std::vector<std::thread> workers;
    workers.push_back(std::thread(queue,&ev3,0,false,0));    // must wait for ev0 and ev1, thread returns
    workers.push_back(std::thread(queue,&ev2,0,true,1));     // must wait for ev0 and ev1, thread sleeps
    workers.push_back(std::thread(queue,&ev0,200,false,2));
    workers.push_back(std::thread(queue,&ev1,300,false,3));
    q.flush();

    // Assert that thread[1] returned only after ev2 was submitted
    // but that thread[0] returned immediately.  This implies that
    // wait did in fact work
    BOOST_CHECK(times[1]>times[0]);

#if 0
    println(times[0]);
    println(times[1]);
    println(times[2]);
    println(times[3]);
#endif

    for (auto& t : workers)
      t.join();
  }
}

BOOST_AUTO_TEST_SUITE_END()


