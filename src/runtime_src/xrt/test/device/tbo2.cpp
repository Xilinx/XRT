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
run(xrt_xocl::device* mydev, unsigned count)
{
  std::thread::id tid = std::this_thread::get_id();
  std::cout << "Thread ID: " << tid << "\n";
  std::cout << "Running BO tests ...\n";
  std::hash<std::thread::id> hasher;
  unsigned randomChar = hasher(tid) % 127;
  if (randomChar < 32)
    randomChar += 32;
  const int bufSize = 1024;

  std::vector<xrt_xocl::hal::BufferObjectHandle> boArray(count);
  for (unsigned i = 0; i < count; i++) {
    boArray[i] = mydev->alloc(bufSize);
    char *data1 = new char[bufSize];
    char *data2 = new char[bufSize];

    std::memset(data1, randomChar, bufSize);
    std::memset(data2, 0, bufSize);
    xrt_xocl::event ev1 = mydev->write(boArray[i], data1, bufSize, 0);
    ev1.wait();
    xrt_xocl::event ev2 = mydev->read(boArray[i], data2, bufSize, 0);
    ev1.wait();
    int result = std::memcmp(data1, data2, bufSize);
    BOOST_CHECK_EQUAL(result, 0);

    std::memset(data2, 0, bufSize);
    xrt_xocl::event ev3 = mydev->sync(boArray[i], bufSize, 0, xrt_xocl::device::direction::HOST2DEVICE);
    ev3.wait();
    xrt_xocl::event ev4 = mydev->write(boArray[i], data2, bufSize, 0);
    ev4.wait();
    ev4 = mydev->sync(boArray[i], bufSize, 0, xrt_xocl::device::direction::DEVICE2HOST);
    ev4.wait();

    void *data3 = mydev->map(boArray[i]);
    result = std::memcmp(data1, data3, bufSize);
    BOOST_CHECK_EQUAL(result, 0);

    randomChar++;
    if (randomChar >= 127)
        randomChar /= 2;

    std::memset(data3, randomChar, bufSize);
    std::memset(data1, randomChar, bufSize);
    xrt_xocl::event ev5 = mydev->sync(boArray[i], bufSize, 0, xrt_xocl::device::direction::HOST2DEVICE);
    ev5.wait();
    std::memset(data3, 0, bufSize);
    xrt_xocl::event ev6 = mydev->sync(boArray[i], bufSize, 0, xrt_xocl::device::direction::DEVICE2HOST);
    ev6.wait();
    xrt_xocl::event ev7 = mydev->read(boArray[i], data2, bufSize, 0);
    ev7.wait();
    result = std::memcmp(data2, data3, bufSize);
    BOOST_CHECK_EQUAL(result, 0);
    result = std::memcmp(data1, data3, bufSize);
    BOOST_CHECK_EQUAL(result, 0);
    delete [] data1;
    delete [] data2;
  }
  for (xrt_xocl::hal::BufferObjectHandle bo : boArray) {
    mydev->unmap(bo);
    mydev->free(bo);
  }
}

static void
runThreads(xrt_xocl::device* mydev, unsigned count)
{
  std::cout << "Launching concurrent BO tests ...\n";
  auto future0 = std::async(std::launch::async, run, mydev, count);
  auto future1 = std::async(std::launch::async, run, mydev, count);
  auto future2 = std::async(std::launch::async, run, mydev, count);
  auto future3 = std::async(std::launch::async, run, mydev, count);
  future0.get();
  future1.get();
  future2.get();
  future3.get();
}

}

BOOST_AUTO_TEST_SUITE(test_bo_stress)

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
    std::string libraryName = device.getDriverLibraryName();
    std::cout << libraryName << "\n";
    run(&device, 10000);
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
    std::string libraryName = device.getDriverLibraryName();
    std::cout << libraryName << "\n";
    runThreads(&device, 1000);
    device.close();
  }
}

BOOST_AUTO_TEST_SUITE_END()


