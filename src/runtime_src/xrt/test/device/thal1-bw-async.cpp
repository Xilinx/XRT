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
#include "../test_helpers.h"

#include "xrt/device/hal.h"
#include "xrt/device/hal2.h"
#include <vector>
#include <iostream>
#include <cstring>
#include <list>

using namespace xrt::test;

namespace {

static int transferSizeTest(xrt::hal2::device* hal, size_t alignment, unsigned maxSize)
{
  xrt::test::AlignedAllocator<unsigned> buf1(alignment, maxSize);
  xrt::test::AlignedAllocator<unsigned> buf2(alignment, maxSize);

  unsigned *writeBuffer = buf1.getBuffer();
  unsigned *readBuffer = buf2.getBuffer();

  for(unsigned j = 0; j < maxSize/4; j++){
    writeBuffer[j] = std::rand();
    readBuffer[j] = 0;
  }

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
    uint64_t pos = hal->allocDeviceBuffer(size);
    auto t1 = hal->addTaskM(&xrt::hal2::device::copyBufferHost2Device,xrt::hal::queue_type::write,pos, writeBuffer, size,0);
    std::memset(readBuffer, 0, size);
    if (t1.get() < 0) {
      std::cout << "FAILED TEST\n";
      std::cout << size << " B write failed\n";
      return 1;
    }
    auto t2 = hal->addTaskM(&xrt::hal2::device::copyBufferDevice2Host,xrt::hal::queue_type::read,readBuffer, pos, size,0);
    if (t2.get() < 0) {
      std::cout << "FAILED TEST\n";
      std::cout << size << " B read failed\n";
      return 1;
    }
    if (std::memcmp(writeBuffer, readBuffer, size)) {
      std::cout << "FAILED TEST\n";
      std::cout << size << " B verification failed\n";
      return 1;
    }

    hal->freeDeviceBuffer(pos);
  }

  return 0;
}

static int transferBenchmarkTest(xrt::hal2::device* hal, size_t alignment, unsigned blockSize, unsigned count)
{
  AlignedAllocator<unsigned> buf1(alignment, blockSize);
  AlignedAllocator<unsigned> buf2(alignment, blockSize);

  unsigned *writeBuffer = buf1.getBuffer();
  unsigned *readBuffer = buf2.getBuffer();

  for(unsigned j = 0; j < blockSize/4; j++) {
    writeBuffer[j] = std::rand();
    readBuffer[j] = 0;
  }

  std::list<uint64_t> deviceHandleList;

  unsigned long long totalData = 0;
  // First try with data verification

  std::cout << "Running benchmark tests...\nWriting/reading " << count << " blocks of " << blockSize / 1024 << " KB\n";
  for (int i = 0; i < count; i++) {
    auto t1 = hal->addTaskM(&xrt::hal2::device::allocDeviceBuffer,xrt::hal::queue_type::misc,blockSize);
    uint64_t writeOffset = t1.get();
    //uint64_t writeOffset = hal->allocDeviceBuffer(blockSize);
    if (writeOffset == -1) {
      std::cout << "FAILED TEST\n";
      std::cout << "Could not allocate device buffer\n";
      return 1;
    }
    deviceHandleList.push_back(writeOffset);

    auto t2 = hal->addTaskM(&xrt::hal2::device::copyBufferHost2Device,xrt::hal::queue_type::write,writeOffset,writeBuffer,blockSize,0);
    //size_t result = hal->copyBufferHost2Device(writeOffset, writeBuffer, blockSize,0);
    std::memset(readBuffer, 0, blockSize);
    ssize_t result = t2.get();
    if (result < 0) {
      std::cout << "FAILED TEST\n";
      std::cout << blockSize/1024 << " KB write failed\n";
      return 1;
    }

    auto t3 = hal->addTaskM(&xrt::hal2::device::copyBufferDevice2Host,xrt::hal::queue_type::read,readBuffer, writeOffset, blockSize,0);
    //result = hal->copyBufferDevice2Host(readBuffer, writeOffset, blockSize,0);
    result = t3.get();
    if (result < 0) {
      std::cout << "FAILED TEST\n";
      std::cout << blockSize/1024 << " KB read failed\n";
      return 1;
    }
    if (std::memcmp(writeBuffer, readBuffer, blockSize)) {
      std::cout << "FAILED TEST\n";
      std::cout << blockSize/1024 << " KB read/write verification failed\n";
      return 1;
    }
    totalData += blockSize;
  }

  totalData = 0;
  Timer myclock;

  std::vector<xrt::task::event<ssize_t>> events;

  for (std::list<uint64_t>::const_iterator i = deviceHandleList.begin(), e = deviceHandleList.end(); i != e; ++i) {
    uint64_t writeOffset = *i;
    events.push_back(hal->addTaskM(&xrt::hal2::device::copyBufferHost2Device,xrt::hal::queue_type::write,writeOffset, writeBuffer, blockSize,0));
    events.push_back(hal->addTaskM(&xrt::hal2::device::copyBufferDevice2Host,xrt::hal::queue_type::read,readBuffer, writeOffset, blockSize,0));
    totalData += blockSize;
  }

  // Wait for all writes and reads
  ssize_t result = 0;
  for (auto& e : events)
    result += e.get();
  
  double totalTime = myclock.stop();
  // Account for both read and write
  totalData *= 2;

  if (result != totalData) {
    std::cout << "FAILED TEST\n";
    std::cout << blockSize/1024 << " KB read failed\n";
    return 1;
  }
  totalData /= 1024000;

  std::cout << "Host <-> Device PCIe RW bandwidth = " << totalData/totalTime << " MB/s\n";

  for (std::list<uint64_t>::const_iterator i = deviceHandleList.begin(), e = deviceHandleList.end(); i != e; ++i) {
    hal->freeDeviceBuffer(*i);
  }
  return 0;
}

}

BOOST_AUTO_TEST_SUITE ( test_hal2_bw_async )

BOOST_AUTO_TEST_CASE( test_hal2_bw_async1 )
{
  auto devices = xrt::hal::loadDevices();
  xrt::hal::device* pcie_device = 0;
  for (auto& device : devices) {
    device->open("device.log",xrt::hal::verbosity_level::quiet);

    device->printDeviceInfo(std::cout) << "\n";
    std::string libraryName = device->getDriverLibraryName();
    std::cout << libraryName << "\n";
    if (libraryName.find("libvc690drv.so")!=std::string::npos)
      pcie_device = device.get();
  }

  xrt::hal2::device* hal2 = dynamic_cast<xrt::hal2::device*>(pcie_device);
  
  size_t alignment = 128;
  if (hal2) {

    // Max size is 8 MB
    if (transferSizeTest(hal2, alignment, 0x7D0000) != 0) {
      std::cout << "FAILED TEST\n";
      BOOST_CHECK_EQUAL(true,false);
    }

    // BlockSize = 16 KB, 245760 blocks
    if (transferBenchmarkTest(hal2, alignment, 0x3E80, 245760) != 0) {
      std::cout << "FAILED TEST\n";
      BOOST_CHECK_EQUAL(true,false);
    }

    // BlockSize = 256 KB, 15360 blocks
    if (transferBenchmarkTest(hal2, alignment, 0x3E800, 15360) != 0) {
      std::cout << "FAILED TEST\n";
      BOOST_CHECK_EQUAL(true,false);
    }

    // BlockSize = 8 MB, 480 blocks
    if (transferBenchmarkTest(hal2, alignment, 0x7D0000, 480) != 0) {
      std::cout << "FAILED TEST\n";
      BOOST_CHECK_EQUAL(true,false);
    }

    // BlockSize = 16 MB, 240 blocks
    if (transferBenchmarkTest(hal2, alignment, 0xFA0000, 240) != 0) {
      std::cout << "FAILED TEST\n";
      BOOST_CHECK_EQUAL(true,false);
    }

    // BlockSize = 64 MB, 60 blocks
    if (transferBenchmarkTest(hal2, alignment, 0x3E80000, 60) != 0) {
      std::cout << "FAILED TEST\n";
      BOOST_CHECK_EQUAL(true,false);
    }


    // BlockSize = 128 MB, 30 blocks
    if (transferBenchmarkTest(hal2, alignment, 0x7D00000, 30) != 0) {
      std::cout << "FAILED TEST\n";
      BOOST_CHECK_EQUAL(true,false);
    }


    // BlockSize = 256 MB, 15 blocks
    if (transferBenchmarkTest(hal2, alignment, 0xFA00000, 15) != 0) {
      std::cout << "FAILED TEST\n";
      BOOST_CHECK_EQUAL(true,false);
    }


  }

}

BOOST_AUTO_TEST_SUITE_END()


