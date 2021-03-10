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
#include "../test_helpers.h"

#include "xrt/device/hal.h"
#include "xrt/device/hal2.h"
#include <vector>
#include <iostream>
#include <cstring>
#include <list>
#include <future>
#include <thread>

using namespace xrt_xocl::test;

namespace {

static void
run(xrt_xocl::device* mydev1, xrt_xocl::device* mydev2)
{
  std::thread::id tid = std::this_thread::get_id();
  std::cout << "Thread ID: " << tid << "\n";
  std::cout << "Running BO tests ...\n";
  std::hash<std::thread::id> hasher;
  unsigned randomChar1 = hasher(tid) % 127;
  if (randomChar1 < 32)
    randomChar1 += 32;

  unsigned randomChar2 = randomChar1 + 1;
  if (randomChar2 >= 127)
    randomChar2 /= 2;

  const int bufSize = 1024;
  xrt_xocl::hal::buffer_object_handle bo1 = mydev1->alloc(bufSize);
  char *data1 = new char[bufSize];
  char *data2 = new char[bufSize];

  std::memset(data1, randomChar1, bufSize);
  std::memset(data2, 0, bufSize);
  // Populate bo1 with randomChar1
  xrt_xocl::event ev = mydev1->write(bo1, data1, bufSize, 0);
  ev.wait();

  // bo2 now aliases bo1
  xrt_xocl::hal::buffer_object_handle bo2 = mydev2->import(bo1);
  ev = mydev2->read(bo2, data2, bufSize, 0);
  ev.wait();

  int result = std::memcmp(data1, data2, bufSize);
  BOOST_CHECK_EQUAL(result, 0);

  // mydev1 now has randomChar1
  ev = mydev1->sync(bo1, bufSize, 0, xrt_xocl::device::direction::HOST2DEVICE);
  ev.wait();

  // host's view of bo1 is all zeros
  std::memset(data2, 0, bufSize);
  ev = mydev1->write(bo1, data2, bufSize, 0);
  ev.wait();
  // bo1 now has randomChar1 again
  ev = mydev1->sync(bo1, bufSize, 0, xrt_xocl::device::direction::DEVICE2HOST);
  ev.wait();

  // mydev2 now has randomChar1
  ev = mydev2->sync(bo2, bufSize, 0, xrt_xocl::device::direction::HOST2DEVICE);
  ev.wait();

  // host's view of bo2 is all zeros
  ev = mydev2->write(bo2, data2, bufSize, 0);
  ev.wait();
  // bo2 now has randomChar1 again
  ev = mydev2->sync(bo2, bufSize, 0, xrt_xocl::device::direction::DEVICE2HOST);
  ev.wait();

  void *data3 = mydev2->map(bo2);
  result = std::memcmp(data1, data3, bufSize);
  BOOST_CHECK_EQUAL(result, 0);

  // bo2 now has randomChar2 and mydev2 also has randomChar2
  std::memset(data3, randomChar2, bufSize);
  ev = mydev2->sync(bo2, bufSize, 0, xrt_xocl::device::direction::HOST2DEVICE);
  ev.wait();

  // Both bo1 and bo2 should have randomChar2
  void *data4 = mydev1->map(bo1);
  result = std::memcmp(data4, data3, bufSize);
  BOOST_CHECK_EQUAL(result, 0);

  // host's view of bo1 and bo2 is all zeros
  std::memset(data4, 0, bufSize);
  result = std::memcmp(data4, data3, bufSize);
  BOOST_CHECK_EQUAL(result, 0);

  // bo2 now has randomChar2
  ev = mydev2->sync(bo2, bufSize, 0, xrt_xocl::device::direction::DEVICE2HOST);
  ev.wait();
  result = std::memcmp(data4, data3, bufSize);
  BOOST_CHECK_EQUAL(result, 0);

  std::memset(data2, randomChar2, bufSize);
  result = std::memcmp(data2, data3, bufSize);
  BOOST_CHECK_EQUAL(result, 0);

  // bo1 now has randomChar1;
  ev = mydev1->sync(bo1, bufSize, 0, xrt_xocl::device::direction::DEVICE2HOST);
  ev.wait();

  std::memset(data2, randomChar1, bufSize);
  result = std::memcmp(data2, data4, bufSize);
  BOOST_CHECK_EQUAL(result, 0);

  result = std::memcmp(data2, data3, bufSize);
  BOOST_CHECK_EQUAL(result, 0);

  mydev1->unmap(bo1);
  mydev1->free(bo1);
  mydev2->unmap(bo2);
  mydev2->free(bo2);
  delete [] data1;
  delete [] data2;
}

static void
runThreads(xrt_xocl::device* mydev1, xrt_xocl::device* mydev2)
{
  std::cout << "Launching concurrent BO tests ...\n";
  auto future0 = std::async(std::launch::async, run, mydev1, mydev2);
  auto future1 = std::async(std::launch::async, run, mydev1, mydev2);
  auto future2 = std::async(std::launch::async, run, mydev1, mydev2);
  auto future3 = std::async(std::launch::async, run, mydev1, mydev2);
  future0.get();
  future1.get();
  future2.get();
  future3.get();
}

}

BOOST_AUTO_TEST_SUITE(test_bo_import)

BOOST_AUTO_TEST_CASE(bo1)
{
  auto pred = [](const xrt_xocl::hal::device& hal) {
    return (hal.getDriverLibraryName().find("xcldrv")!=std::string::npos);
  };

  auto devices = xrt_xocl::test::loadDevices(std::move(pred));
  for (auto& device : devices) {
    device.open();
    device.setup(); // this creates the worker threads
    device.printDeviceInfo(std::cout) << "\n";
    std::cout << device.getDriverLibraryName() << "\n";
  }

  for (auto& device1 : devices) {
    for (auto& device2 : devices) {
      run(&device1, &device2);
    }
  }

  for (auto& device : devices) {
    device.close();
  }
}

BOOST_AUTO_TEST_CASE(bo2)
{
  auto pred = [](const xrt_xocl::hal::device& hal) {
    return (hal.getDriverLibraryName().find("xcldrv")!=std::string::npos);
  };
  auto devices = xrt_xocl::test::loadDevices(std::move(pred));

  for (auto& device : devices) {
    device.open();
    device.setup(); // this creates the worker threads
    device.printDeviceInfo(std::cout) << "\n";
    std::cout << device.getDriverLibraryName() << "\n";
  }

  for (auto& device1 : devices) {
    for (auto& device2 : devices) {
      runThreads(&device1, &device2);
    }
  }

  for (auto& device : devices) {
    device.close();
  }
}

BOOST_AUTO_TEST_SUITE_END()


