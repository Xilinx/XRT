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

#ifndef xrt_util_aligned_allocator_h_
#define xrt_util_aligned_allocator_h_

#include <cstddef>
#include <cstdlib>
#include <new>

namespace xrt {

/**
 * Aligned allocator for use with std containers
 *
 * std::vector<int,xrt::aligned_allocator<int,4096>> vec;
 * auto data = vec.data();
 * assert((data % 4096)==0);
 */
template <typename T, std::size_t Align>
struct aligned_allocator
{
  using value_type = T;

  template <typename U>
  struct rebind 
  {
    using other = aligned_allocator<U,Align>;
  };

  aligned_allocator() = default;

  template <typename U> 
  aligned_allocator(const aligned_allocator<U,Align>&) {}

  T* allocate(std::size_t num)
  {
    void* ptr = nullptr;
    if (posix_memalign(&ptr,Align,num*sizeof(T)))
      throw std::bad_alloc();
    return reinterpret_cast<T*>(ptr);
  }
  void deallocate(T* p, std::size_t num)
  {
    free(p);
  }
};

}

#endif


