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

#include <iostream>
#include "xrt/device/device.h"

BOOST_AUTO_TEST_SUITE ( test_device )

BOOST_AUTO_TEST_CASE( test_device1 )
{
  auto devices = xrt_xocl::test::loadDevices();

  for (auto& device : devices) {
    std::string libraryName = device.getDriverLibraryName();
    std::cout << libraryName << "\n";
    device.open();
    device.printDeviceInfo(std::cout) << std::endl;
  }

  for (auto& device : devices) {
  }

}

BOOST_AUTO_TEST_SUITE_END()



