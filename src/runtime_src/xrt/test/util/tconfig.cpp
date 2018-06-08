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
#include <iostream>

// % sdaccel -exec truntime --run_test=test_config

BOOST_AUTO_TEST_SUITE ( test_config )

BOOST_AUTO_TEST_CASE( test_config1 )
{
  std::string ini(__FILE__);
  ini += ".ini";
  xrt::config::detail::debug(std::cout,ini);

  if (xrt::config::get_dma_threads() != 2) {
    // This test works only if no other test has used get API.
    // To ensure that, run the test in isolation.
    std::cout << "Test case [test_config] not run because config values are already cached.\n";
    std::cout << "Run alone as --run_test=test_config/test_config1\n";
    return;
  }

  BOOST_CHECK_EQUAL(xrt::config::get_debug(),true);
  BOOST_CHECK_EQUAL(xrt::config::get_profile(),false);
  BOOST_CHECK_EQUAL(xrt::config::get_logging(),"console");
  BOOST_CHECK_EQUAL(xrt::config::get_api_checks(),true);
  BOOST_CHECK_EQUAL(xrt::config::get_dma_threads(),2);

  // not in ini file, default value is 0
  BOOST_CHECK_EQUAL(xrt::config::get_verbosity(),0);

  // no primary accessor for these
  BOOST_CHECK_EQUAL(xrt::config::detail::get_bool_value("Emulation.diagnostics",false),true);

  // not in ini file, check that we get desired default value
  BOOST_CHECK_EQUAL(xrt::config::detail::get_bool_value("Emulation.bogus",true),true);
  BOOST_CHECK_EQUAL(xrt::config::detail::get_bool_value("Emulation.bogus",false),false);
}

BOOST_AUTO_TEST_SUITE_END()



