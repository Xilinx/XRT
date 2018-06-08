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

#include "xrt/config.h"
#include "xrt/util/aligned_allocator.h"
#include <vector>

BOOST_AUTO_TEST_SUITE ( test_aligned_allocator )

BOOST_AUTO_TEST_CASE( test_aligned_allocator1 )
{
  {
    std::vector<int,xrt::aligned_allocator<int,4096>> vec;
    vec.push_back(10);
    auto data = vec.data();
    BOOST_CHECK_EQUAL(reinterpret_cast<uintptr_t>(data) % 4096,0);
  }
  {
    std::vector<int,xrt::aligned_allocator<int,128>> vec;
    vec.push_back(10);
    auto data = vec.data();
    BOOST_CHECK_EQUAL(reinterpret_cast<uintptr_t>(data) % 128,0);
  }
}

BOOST_AUTO_TEST_SUITE_END()


