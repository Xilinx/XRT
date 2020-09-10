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

#include <unistd.h>

#include "../test_helpers.h"

#include "xrt/device/hal.h"
#include "xrt/device/hal2.h"
#include <vector>
#include <iostream>
#include <cstring>
#include <list>
#include <future>
#include <thread>

using namespace xrt::test;

namespace {

static void
run(xrt::device* mydev)
{
  std::thread::id tid = std::this_thread::get_id();
  std::cout << "Thread ID: " << tid << "\n";
  std::cout << "Running MBUF tests ...\n";
}

}

BOOST_AUTO_TEST_SUITE(test_mbuf_basic)

BOOST_AUTO_TEST_CASE(mbuf1)
{
  auto pred = [](const xrt::hal::device& hal) {
    return (hal.getDriverLibraryName().find("xcldrv")!=std::string::npos);
  };
  auto devices = xrt::test::loadDevices(std::move(pred));

  for (auto& device : devices) {
    device.open();
    device.setup(); // this creates the worker threads
    device.printDeviceInfo(std::cout) << "\n";
    std::string libraryName = device.getDriverLibraryName();
    std::cout << libraryName << "\n";
    run(&device);
    device.close();
  }
}

BOOST_AUTO_TEST_SUITE_END()


