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

#include "xrt/config.h"
#include "xrt/util/regmap.h"
#include <iostream>

BOOST_AUTO_TEST_SUITE ( test_regmap )

BOOST_AUTO_TEST_CASE( test_regmap1 )
{
  {
    using word = uint32_t;
    using regmap_type = xrt_xocl::regmap<word,10,4096>;
    regmap_type regmap;
    BOOST_CHECK_EQUAL(regmap.size(),0);
    BOOST_CHECK_EQUAL(regmap.bytes(),0);
    regmap[1] = 1;
    BOOST_CHECK_EQUAL(regmap.size(),2);
    BOOST_CHECK_EQUAL(regmap.bytes(),sizeof(word)*regmap.size());
    regmap[9] = 9;
    BOOST_CHECK_EQUAL(regmap.size(),10);
    BOOST_CHECK_EQUAL(regmap.bytes(),sizeof(word)*regmap.size());

    // check alignment
    auto data = regmap.data();
    BOOST_CHECK_EQUAL(reinterpret_cast<uintptr_t>(data) % 4096,0);
  }

  {
    using word = uint32_t;
    using regmap_type = xrt_xocl::regmap<word,4096,128>;
    regmap_type regmap;
    BOOST_CHECK_EQUAL(regmap.size(),0);
    BOOST_CHECK_EQUAL(regmap.bytes(),0);
    regmap[1] = 1;
    BOOST_CHECK_EQUAL(regmap.size(),2);
    BOOST_CHECK_EQUAL(regmap.bytes(),sizeof(word)*regmap.size());
    regmap[9] = 9;
    BOOST_CHECK_EQUAL(regmap.size(),10);
    BOOST_CHECK_EQUAL(regmap.bytes(),sizeof(word)*regmap.size());
    regmap[4095] = 9;
    BOOST_CHECK_EQUAL(regmap.size(),4096);
    BOOST_CHECK_EQUAL(regmap.bytes(),sizeof(word)*regmap.size());

    // check alignment
    auto data = regmap.data();
    BOOST_CHECK_EQUAL(reinterpret_cast<uintptr_t>(data) % 128,0);
  }

  {
    using word = uint64_t;
    using regmap_type = xrt_xocl::regmap<word,4096>;
    regmap_type regmap;
    BOOST_CHECK_EQUAL(regmap.size(),0);
    BOOST_CHECK_EQUAL(regmap.bytes(),0);
    regmap[1] = 1;
    BOOST_CHECK_EQUAL(regmap.size(),2);
    BOOST_CHECK_EQUAL(regmap.bytes(),sizeof(word)*regmap.size());
    regmap[9] = 9;
    BOOST_CHECK_EQUAL(regmap.size(),10);
    BOOST_CHECK_EQUAL(regmap.bytes(),sizeof(word)*regmap.size());
    regmap[4095] = 9;
    BOOST_CHECK_EQUAL(regmap.size(),4096);
    BOOST_CHECK_EQUAL(regmap.bytes(),sizeof(word)*regmap.size());

    // check alignment
    auto data = regmap.data();
    BOOST_CHECK_EQUAL(reinterpret_cast<uintptr_t>(data) % alignof(std::max_align_t),0);
  }

}

BOOST_AUTO_TEST_SUITE_END()


