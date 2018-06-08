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
    ssize_t result = hal->copyBufferHost2Device(pos, writeBuffer, size,0);
    if (result < 0) {
      std::cout << "FAILED TEST\n";
      std::cout << size << " B write failed\n";
      return 1;
    }
    std::memset(readBuffer, 0, size);
    result = hal->copyBufferDevice2Host(readBuffer, pos, size,0);
    if (result < 0) {
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
    uint64_t writeOffset = hal->allocDeviceBuffer(blockSize);
    if (writeOffset == -1) {
      std::cout << "FAILED TEST\n";
      std::cout << "Could not allocate device buffer\n";
      return 1;
    }
    deviceHandleList.push_back(writeOffset);

    ssize_t result = hal->copyBufferHost2Device(writeOffset, writeBuffer, blockSize,0);
    if (result < 0) {
      std::cout << "FAILED TEST\n";
      std::cout << blockSize/1024 << " KB write failed\n";
      return 1;
    }
    std::memset(readBuffer, 0, blockSize);
    result = hal->copyBufferDevice2Host(readBuffer, writeOffset, blockSize,0);
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

  for (std::list<uint64_t>::const_iterator i = deviceHandleList.begin(), e = deviceHandleList.end(); i != e; ++i) {
    uint64_t writeOffset = *i;
    ssize_t result = hal->copyBufferHost2Device(writeOffset, writeBuffer, blockSize,0);
    if (result < 0) {
      std::cout << "FAILED TEST\n";
      std::cout << blockSize/1024 << " KB write failed\n";
      return 1;
    }
    result = hal->copyBufferDevice2Host(readBuffer, writeOffset, blockSize,0);
    if (result < 0) {
      std::cout << "FAILED TEST\n";
      std::cout << blockSize/1024 << " KB read failed\n";
      return 1;
    }
    totalData += blockSize;
  }

  double totalTime = myclock.stop();
  // Account for both read and write
  totalData *= 2;
  totalData /= 1024000;

  std::cout << "Host <-> Device PCIe RW bandwidth = " << totalData/totalTime << " MB/s\n";

  for (std::list<uint64_t>::const_iterator i = deviceHandleList.begin(), e = deviceHandleList.end(); i != e; ++i) {
    hal->freeDeviceBuffer(*i);
  }
  return 0;
}

}

BOOST_AUTO_TEST_SUITE ( test_hal2_bw )

BOOST_AUTO_TEST_CASE( test_hal2_bw1 )
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


