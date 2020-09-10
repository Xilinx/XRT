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

#include "xocl/core/range.h"
#include "xocl/core/param.h"
#include "xocl/core/refcount.h"


#include <vector>

namespace { }


BOOST_AUTO_TEST_SUITE ( test_param )

BOOST_AUTO_TEST_CASE( test_param_1 )
{
  size_t sz=0;
  {
    // scalar
    int ubuf = 0;
    int* buffer = &ubuf;
    xocl::param_buffer param { buffer, sizeof(int)*1, &sz };
    param.as<int>() = 5;
    BOOST_CHECK_EQUAL(5,*buffer);
    BOOST_CHECK_EQUAL(sz,sizeof(int));
  }

  {
    // string arrray
    char buffer[6] = {0};
    xocl::param_buffer param { buffer, 6 , &sz };
    param.as<char>() = "hello";
    BOOST_CHECK_EQUAL("hello",buffer);
    BOOST_CHECK_EQUAL(sz,6);
  }

  {
    // std::string
    char buffer[6] = {0};
    xocl::param_buffer param { buffer, 6 , &sz };
    std::string str("world");
    param.as<unsigned char>() = str;
    BOOST_CHECK_EQUAL("world",buffer);
    BOOST_CHECK_EQUAL(sz,6);
  }

  {
    // std::string empty
    char buffer[6];
    xocl::param_buffer param { &buffer, 6 , &sz };
    std::string str("");
    param.as<unsigned char>() = str;
    BOOST_CHECK_EQUAL("",buffer);
    BOOST_CHECK_EQUAL(sz,1);
  }

  {
    // std::vector of scalar
    int buffer[4] = {0};
    std::vector<int> vec = {1, 2, 3, 4};
    xocl::param_buffer param { buffer, 4*sizeof(int), &sz };
    param.as<int>() = vec;
    BOOST_CHECK_EQUAL(buffer[0],1);
    BOOST_CHECK_EQUAL(buffer[1],2);
    BOOST_CHECK_EQUAL(buffer[2],3);
    BOOST_CHECK_EQUAL(buffer[3],4);
    BOOST_CHECK_EQUAL(sz,4*sizeof(int));
  }

#if 0
  {
    // std::vector of non-scalar, fails compilation as it should
    int buffer[4] = {0};
    std::vector<std::string> vec = {"1", "2", "3", "4"};
    xocl::param_buffer param { buffer, 4*sizeof(int), &sz };
    param = vec;
    BOOST_CHECK_EQUAL(buffer[0],1);
    BOOST_CHECK_EQUAL(buffer[1],2);
    BOOST_CHECK_EQUAL(buffer[2],3);
    BOOST_CHECK_EQUAL(buffer[3],4);
    BOOST_CHECK_EQUAL(sz,4*sizeof(int));
  }
#endif


  {
    // xocl::range
    int buffer[4] = {0};
    std::vector<int> vec = {1, 2, 3, 4};
    xocl::range<std::vector<int>::iterator> r(vec.begin(),vec.end());
    xocl::param_buffer param { buffer, 4*sizeof(int), &sz };
    param.as<int>() = r;
    BOOST_CHECK_EQUAL(buffer[0],1);
    BOOST_CHECK_EQUAL(buffer[1],2);
    BOOST_CHECK_EQUAL(buffer[2],3);
    BOOST_CHECK_EQUAL(buffer[3],4);
    BOOST_CHECK_EQUAL(sz,4*sizeof(int));
  }

  {
    // xocl::range with get_range
    unsigned int buffer[4] = {0};
    std::vector<int> vec = {1, 2, 3, 4};
    xocl::param_buffer param { buffer, 4*sizeof(int), &sz };
    param.as<unsigned int>() = xocl::get_range(vec);
    BOOST_CHECK_EQUAL(buffer[0],1);
    BOOST_CHECK_EQUAL(buffer[1],2);
    BOOST_CHECK_EQUAL(buffer[2],3);
    BOOST_CHECK_EQUAL(buffer[3],4);
    BOOST_CHECK_EQUAL(sz,4*sizeof(int));
  }

  {
    char buffer[4] = {0};
    xocl::param_buffer param { buffer, 6 , &sz };
    
    bool exception = false;
    try {
      param.as<char>() = "hello world";
    }
    catch (const std::exception& ex) {
      exception = true;
    }
    BOOST_CHECK_EQUAL(true,exception);
  }

  {
    // test buffer is nullptr, size (6) ignored,
    // return size of requested data
    xocl::param_buffer param { nullptr, 6 , &sz };
    
    bool exception = false;
    param.as<char>() = "hello world";
    BOOST_CHECK_EQUAL(sz,12);
  }

  {
    // allocate memory to partition into char[]
    char** stuff = (char**) malloc(4 * sizeof(char*));
    for (auto i : {0,1,2,3}) {
      stuff[i] = (char*)malloc(10);
    }
    xocl::param_buffer param { stuff, 4 * sizeof(char*), &sz };
    char** buf1 = param.as_array<char*>(1);
    char** buf2 = param.as_array<char*>(1);
    char** buf34 = param.as_array<char*>(2);

    // We have exhausted the storage
    bool exception = false;
    try {
      char** x = param.as_array<char*>(1);
    }
    catch (const std::exception& ex) {
      exception = true;
    }
    BOOST_CHECK_EQUAL(true,exception);
    BOOST_CHECK_EQUAL(sz,4*sizeof(char*));
  }

  {
    // xocl::get_range of a ptr type
    intptr_t p[8];
    p[0] = 0x0;
    p[1] = 0x1;
    p[2] = 0x2;
    p[3] = 0x3;

    intptr_t stuff[4];
    xocl::param_buffer param { &stuff, 4*sizeof(intptr_t), &sz };
    param.as<intptr_t>() = xocl::get_range(p,p+4);
    BOOST_CHECK_EQUAL(stuff[0],p[0]);
    BOOST_CHECK_EQUAL(stuff[1],p[1]);
    BOOST_CHECK_EQUAL(stuff[2],p[2]);
    BOOST_CHECK_EQUAL(stuff[3],p[3]);
  }

  {
    // range of reference counted ptr wrapped objects 
    struct blah : public xocl::refcount
    {
      int mi;
      blah(int i) : mi(i) {}
    };

    using cl_blah = blah*;
    using vec_type = std::vector<xocl::ptr<blah>>;
    using blah_iterator_type = xocl::ptr_iterator<vec_type::iterator>;
    static_assert(std::is_same<blah_iterator_type::value_type,blah*>::value,"not the same");

    auto b1 = std::make_unique<blah>(1); // clBlahCreate()
    auto b2 = std::make_unique<blah>(2); // clBlahCreate()
    auto b3 = std::make_unique<blah>(3); // clBlahCreate()
    auto b4 = std::make_unique<blah>(4); // clBlahCreate()

    BOOST_CHECK_EQUAL(b1->count(),1);
    {
      std::vector<xocl::ptr<blah>> vec;
      vec.push_back(b1.get());
      vec.push_back(b2.get());
      vec.push_back(b3.get());
      vec.push_back(b4.get());

      BOOST_CHECK_EQUAL(b1->count(),2);

      cl_blah buffer[4];
      xocl::param_buffer param( &buffer, 4*sizeof(cl_blah), &sz);
      param.as<cl_blah>() = xocl::get_range<blah_iterator_type>(vec.begin(),vec.end());
      BOOST_CHECK_EQUAL(buffer[0]->mi,1);
      BOOST_CHECK_EQUAL(buffer[1]->mi,2);
      BOOST_CHECK_EQUAL(buffer[2]->mi,3);
      BOOST_CHECK_EQUAL(buffer[3]->mi,4);

      BOOST_CHECK_EQUAL(b1->count(),2);
    }
    BOOST_CHECK_EQUAL(b1->count(),1);
  }
}

BOOST_AUTO_TEST_SUITE_END()


