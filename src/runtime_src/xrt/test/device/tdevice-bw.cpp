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

#include "xrt/device/device.h"
#include <algorithm>
#include <iostream>

using namespace xrt_xocl::test;

namespace {

static size_t alignment = 128;

static int 
transferSizeTest(xrt_xocl::device* device, size_t maxSize)
{
  AlignedAllocator<char> buf1(alignment, maxSize);
  AlignedAllocator<char> buf2(alignment, maxSize);

  auto writeBuffer = buf1.getBuffer();
  auto readBuffer = buf2.getBuffer();

  std::cout << "Running transfer test with various buffer sizes...\n";

  size_t size = 128;
  bool flag = true;
  for (unsigned i = 0; flag; i++) {
    size <<= i;
    if (size > maxSize) {
      size = maxSize;
      flag = false;
    }
    std::cout << "Size " << size << " B\n";
    auto bo = device->alloc(size);
    {
      auto ev = device->write(bo,writeBuffer,size,0);
      ev.wait();
    }
    {
      auto ev = device->sync(bo,size,0,xrt_xocl::device::direction::HOST2DEVICE); // h2d
      std::fill(readBuffer,readBuffer+size,0);
      ev.wait(); // todo: check for throw if unmap failed
    }
    {
      auto ev = device->sync(bo,size,0,xrt_xocl::device::direction::HOST2DEVICE); // d2h
      ev.wait();
    }
    {
      auto ev = device->read(bo,readBuffer,size,0);
      ev.wait();
    }
    if (!std::equal(writeBuffer,writeBuffer+size,readBuffer)) {
      std::cout << "FAILED TEST\n";
      std::cout << size << " B verification failed\n";
      return 1;
    }
  }

  return 0;
}

static int 
transferBenchmarkTest(xrt_xocl::device* device, size_t blockSize, size_t count, bool async)
{
  AlignedAllocator<char> buf1(alignment, blockSize);
  AlignedAllocator<char> buf2(alignment, blockSize);

  auto writeBuffer = buf1.getBuffer();
  auto readBuffer = buf2.getBuffer();

  std::vector<xrt_xocl::device::buffer_object_handle> deviceHandleList;

  unsigned long long totalData = 0;

  std::cout << "Running " << (async?"*async* ":"") << "benchmark tests...\nWriting/reading " 
            << count 
            << " blocks of " 
            << blockSize / 1024 
            << " KB\n";

  // Allocate count device buffers and verify data transfer integrity
  for (int i = 0; i < count; i++) {
    auto bo = device->alloc(blockSize,writeBuffer);
    deviceHandleList.push_back(bo);

    {
      auto ev = device->sync(bo,blockSize,0,xrt_xocl::device::direction::HOST2DEVICE,false); // h2d
      std::fill(readBuffer,readBuffer+blockSize,0);
      ev.wait();
    }
    {
      auto ev = device->sync(bo,blockSize,0,xrt_xocl::device::direction::DEVICE2HOST,false); // d2h
      ev.wait();
    }
    {
      auto ev = device->read(bo,readBuffer,blockSize,0,false);
      ev.wait();
    }
    if (!std::equal(writeBuffer,writeBuffer+blockSize,readBuffer)) {
      std::cout << "FAILED TEST\n";
      std::cout << blockSize/1024 << " KB read/write verification failed\n";
      return 1;
    }
    totalData += blockSize;
  }

  // Transfer bw test
  totalData = 0;
  std::vector<xrt_xocl::event> events;
  Timer myclock;

  for (auto& bo : deviceHandleList) {

    {
      auto ev = device->sync(bo,blockSize,0,xrt_xocl::device::direction::HOST2DEVICE,async); // h2d
      if (async) 
        events.push_back(std::move(ev));
      else 
        ev.wait();
    }

    {
      auto ev = device->sync(bo,blockSize,0,xrt_xocl::device::direction::DEVICE2HOST,async); // d2h
      if (async)
        events.push_back(std::move(ev));
      else 
        ev.wait();
    }

    totalData += blockSize;
  }

  for (auto& ev : events)
    ev.wait();

  double totalTime = myclock.stop();

  // Account for both read and write
  totalData *= 2;
  totalData /= 1024000;

  std::cout << "Host <-> Device PCIe RW bandwidth = " << totalData/totalTime << " MB/s\n";

  return 0;
}

void
run(xrt_xocl::device* device)
{
  std::string libraryName = device->getDriverLibraryName();
  std::cout << libraryName << "\n";

  device->open();
  device->setup(); // this creates the worker threads

  try {
    // Max size is 8 MB
    if (transferSizeTest(device,0x7D0000) != 0) {
      std::cout << "FAILED TEST\n";
      BOOST_CHECK_EQUAL(true,false);
    }

    for (bool async : {true,false}) {

      // BlockSize = 16 KB, 245760 blocks
      if (transferBenchmarkTest(device, 0x3E80, 245760, async) != 0) {
        std::cout << "FAILED TEST\n";
        BOOST_CHECK_EQUAL(true,false);
      }

      // BlockSize = 256 KB, 15360 blocks
      if (transferBenchmarkTest(device, 0x3E800, 15360, async) != 0) {
        std::cout << "FAILED TEST\n";
        BOOST_CHECK_EQUAL(true,false);
      }

      // BlockSize = 8 MB, 480 blocks
      if (transferBenchmarkTest(device, 0x7D0000, 480, async) != 0) {
        std::cout << "FAILED TEST\n";
        BOOST_CHECK_EQUAL(true,false);
      }

      // BlockSize = 16 MB, 240 blocks
      if (transferBenchmarkTest(device, 0xFA0000, 240, async) != 0) {
        std::cout << "FAILED TEST\n";
        BOOST_CHECK_EQUAL(true,false);
      }

      // BlockSize = 64 MB, 60 blocks
      if (transferBenchmarkTest(device, 0x3E80000, 60, async) != 0) {
        std::cout << "FAILED TEST\n";
        BOOST_CHECK_EQUAL(true,false);
      }


      // BlockSize = 128 MB, 30 blocks
      if (transferBenchmarkTest(device, 0x7D00000, 30, async) != 0) {
        std::cout << "FAILED TEST\n";
        BOOST_CHECK_EQUAL(true,false);
      }


      // BlockSize = 256 MB, 15 blocks
      if (transferBenchmarkTest(device, 0xFA00000, 15, async) != 0) {
        std::cout << "FAILED TEST\n";
        BOOST_CHECK_EQUAL(true,false);
      }
    }

  }
  catch (const std::exception& ex) {
    std::cout << ex.what() << std::endl;
  }
}

}

// invoke with --run_test=device_bw_pcie
BOOST_AUTO_TEST_SUITE ( test_device_bw )

BOOST_AUTO_TEST_CASE ( test_device_bw1)
{
  auto pred = [](const xrt_xocl::hal::device& hal) {
    return (hal.getDriverLibraryName().find("690")!=std::string::npos);
  };
  auto devices = xrt_xocl::test::loadDevices(std::move(pred));

  for (auto& device : devices) {
    run(&device);
  }
}

BOOST_AUTO_TEST_SUITE_END()

// invoke with --run_test=device_bw_swemu
BOOST_AUTO_TEST_SUITE ( test_device_bw )

BOOST_AUTO_TEST_CASE( test_swemu )
{
  std::cout << "test_device_bw[test_swemu]" << std::endl;

  auto pred = [](const xrt_xocl::hal::device& hal) {
    return (hal.getDriverLibraryName().find("sw_em")!=std::string::npos);
  };

  auto devices = xrt_xocl::test::loadDevices(std::move(pred));

  for (auto& device : devices) {
    run(&device);
    break;    // once is enough
  }
}

BOOST_AUTO_TEST_SUITE_END()


