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
#include "setup.h"

#include "xocl/core/device.h"
#include "xocl/core/memory.h"
#include "xocl/core/time.h"
#include <vector>

BOOST_AUTO_TEST_SUITE ( test_clCreateBuffer )

BOOST_AUTO_TEST_CASE( test_clCreateBuffer1 )
{
  ocl_sw_emulation ocl;
  cl_int err = CL_SUCCESS;

  size_t count=10000;
  size_t sz=128;
  unsigned long create_time = 0;
  unsigned long release_time = 0;
  unsigned long lookup_time = 0;

  std::vector<cl_mem> mem_vec(count,nullptr);

  // Creation time
  {
    xocl::time_guard tg1(create_time);
    for (int i=0; i<count; ++i) 
      mem_vec[i] = clCreateBuffer(ocl.context,CL_MEM_READ_WRITE,sz,nullptr,&err);
  }

  // Check that buffers were created
  for (int i=0; i<count; ++i) 
    BOOST_CHECK(mem_vec[i]);

  // Lookup time
  {
    auto device = xocl::xocl(ocl.device);
    xocl::time_guard tg1(lookup_time);
    for (int i=0; i<count; ++i) 
      auto boh = xocl::xocl(mem_vec[i])->get_buffer_object(device);
  }

  // Release time
  {
    auto device = xocl::xocl(ocl.device);
    xocl::time_guard tg1(release_time);
    for (int i=0; i<count; ++i) 
      clReleaseMemObject(mem_vec[i]);
  }

  std::cout << "Buffer stats for " << count << " buffers\n";
  std::cout << "Creation time: " << create_time*1e-6 << "\n";
  std::cout << "Lookup time: " << lookup_time*1e-6 << "\n";
  std::cout << "Release time: " << release_time*1e-6 << "\n";
}

BOOST_AUTO_TEST_SUITE_END()


