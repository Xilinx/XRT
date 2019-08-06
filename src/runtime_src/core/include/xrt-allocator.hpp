/*
 * Copyright (C) 2019, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

#ifndef __XRT_ALLOCATOR__H__
#define __XRT_ALLOCATOR__H__


#include "xrt_helper.hpp"
#include <iostream>

namespace xrt_util {

template <typename T>
struct allocator 
{
  using value_type = T;

  static T* allocate(std::size_t num)
  {
  	void* ptr = nullptr;
  	helper* x;
  	x = helper::getinstance();
  	if(x == nullptr)
    	throw std::bad_alloc();

  	ptr = x->allocate(num*sizeof(T));
  	if (!ptr)
    	throw std::bad_alloc();

  	return reinterpret_cast<T*>(ptr);
  }
  static void deallocate(T* p, std::size_t num)
  {
  //free BO call here
    helper* x;
    x = helper::getinstance();
    if(x == nullptr)
        throw std::bad_alloc();

    x->deallocate(p);
  }
};

}


#endif
