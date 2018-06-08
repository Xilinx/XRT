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
#include "../xcl_test_helpers.h"

#include "xocl/core/object.h"
#include "xocl/core/platform.h"
#include "xocl/core/device.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/event.h"
#include "xocl/core/context.h"
#include "xocl/core/memory.h"
#include "xocl/core/sampler.h"
#include "xocl/core/program.h"
#include "xocl/core/kernel.h"

#include <type_traits>

namespace {

template <typename T1, typename T2>
static void
check_equal(T1 t1, T2 t2)
{
  BOOST_CHECK_EQUAL(t1,t2);
  bool value = std::is_same<T1,T2>::value;
  BOOST_CHECK_EQUAL(value,true);
}

}

BOOST_AUTO_TEST_SUITE ( test_object )

BOOST_AUTO_TEST_CASE( test_object_cast )
{
  xocl::platform p;
  xocl::platform* platform = nullptr;
  {
    // platform
    auto vp = xocl::get_platforms();
    BOOST_CHECK_EQUAL(vp.size(),1);
    BOOST_CHECK_EQUAL(vp.size(),xocl::get_num_platforms());
    xocl::platform* x = platform = vp[0];

    cl_platform_id c = x;
    xocl::platform* obj1 = xocl::xocl(c);
    check_equal(obj1,x);
  }

#if 0
  {
    xocl::device* x = platform->getDefaultDevice();
    cl_device_id c = x;
    xocl::device* obj1 = xocl::xocl(c);
    check_equal(obj1,x);
  }
#endif
  
  {
    // context
    xocl::context context(nullptr,0,nullptr);
    xocl::context* x = &context;

    cl_context c = x;
    xocl::context* obj1 = xocl::xocl(c);
    check_equal(obj1,x);
  }

#if 0
  {
    // command_queue
    xocl::context ctx;
    xocl::device* dev = platform->getDefaultDevice();
    xocl::command_queue command_queue(&ctx,dev,0);
    xocl::command_queue* x = &command_queue;

    cl_command_queue c = x;
    xocl::command_queue* obj1 = xocl::xocl(c);
    check_equal(obj1,x);
  }
#endif

#if 0
  {
    // event
    xocl::context ctx;
    xocl::device* dev = platform->getDefaultDevice();
    xocl::command_queue queue(&ctx,dev,0);
    xocl::event event(&queue,&ctx);
    xocl::event* x = &event;

    cl_event c = x;
    xocl::event* obj1 = xocl::xocl(c);
    check_equal(obj1,x);
  }
#endif

  {
    // program
    xocl::context context(nullptr,0,nullptr);
    xocl::program program(&context);
    xocl::program* x = &program;

    cl_program c = x;
    xocl::program* obj1 = xocl::xocl(c);
    check_equal(obj1,x);
  }

  {
    // kernel
    xocl::kernel kernel(nullptr,"");
    xocl::kernel* x = &kernel;

    cl_kernel c = x;
    xocl::kernel* obj1 = xocl::xocl(c);
    check_equal(obj1,x);
  }

  {
    // sampler
    xocl::sampler sampler(nullptr,false,0,0);
    xocl::sampler* x = &sampler;

    cl_sampler c = x;
    xocl::sampler* obj1 = xocl::xocl(c);
    check_equal(obj1,x);
  }

  {
    // memory
    xocl::context ctx(nullptr,0,nullptr);
    xocl::buffer<xocl::memory> memory(&ctx,0,0,nullptr);
    xocl::memory* x = &memory;

    cl_mem c = x;
    xocl::memory* obj1 = xocl::xocl(c);
    check_equal(obj1,x);
  }
}

BOOST_AUTO_TEST_SUITE_END()


