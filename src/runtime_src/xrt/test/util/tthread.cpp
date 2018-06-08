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

#include "xrt/util/debug.h"
#include "xrt/util/time.h"
#include "xrt/util/thread.h"
#include "xrt/util/config_reader.h"
#include <iostream>

#ifdef __GNUC__
# include <sched.h>
#endif

// % sdaccel -exec truntime --run_test=test_config

BOOST_AUTO_TEST_SUITE ( test_thread )

BOOST_AUTO_TEST_CASE( test_thread1 )
{
  std::string ini(__FILE__);
  ini += ".ini";
  xrt::config::detail::debug(std::cout,ini);

  bool stop=false;
  std::vector<std::thread> threads;

  auto worker = [&stop]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    while (!stop) {
      XRT_PRINT(std::cout,"thread(",std::this_thread::get_id(),") on CPU(",sched_getcpu(),")\n");
      auto start = xrt::time_ns();
      while ((xrt::time_ns() - start)*1e-6 < 1000) ;
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  };

  try {
    for (int i=0;i<10; ++i)
      threads.push_back(xrt::thread(worker));
    std::this_thread::sleep_for(std::chrono::seconds(5));
    XRT_PRINT(std::cout,"setting stop=true\n");
    stop = true;
    for (auto& t : threads)
      t.join();
  }
  catch (const std::exception& ex) {
    BOOST_TEST_MESSAGE(ex.what());
  }

}

BOOST_AUTO_TEST_SUITE_END()



