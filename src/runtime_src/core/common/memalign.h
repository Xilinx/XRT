/**
 * Copyright (C) 2019 Xilinx, Inc
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

#ifndef xrtcore_memalign_h_
#define xrtcore_memalign_h_

#include <cstdlib>
#include <memory>
#include <stdexcept>

namespace xrt_core {

inline int
posix_memalign(void **memptr, size_t alignment, size_t size)
{
#if defined(__linux__)
  return ::posix_memalign(memptr,alignment,size);
#elif defined(_WINDOWS)
  // this is not good, _aligned_malloc requires _aligned_free
  // power of 2
  if (!alignment || (alignment & (alignment - 1)))
    return EINVAL;

  auto save = errno;
  auto ptr = _aligned_malloc(size,alignment);
  if (!ptr) {
    errno = save;
    return ENOMEM;
  }

  *memptr = ptr;
  return 0;
#endif
}

namespace detail {

template <typename MyType>
struct aligned_ptr_deleter
{
#if defined(_WINDOWS)
  void operator() (MyType* ptr)  { _aligned_free(ptr); }
#else
  void operator() (MyType* ptr)  { free(ptr); }
#endif
};

template <typename MyType>
using aligned_ptr_t = std::unique_ptr<MyType, aligned_ptr_deleter<MyType>>;

// aligned_alloc - Allocated managed aligned memory
//
// Allocates size bytes of uninitialized storage whose alignment is
// specified by align.  The allocated memory is managed by a
// unique_ptr to ensure proper freeing of the memory upon destruction.
template <typename MyType>
inline aligned_ptr_t<MyType>
aligned_alloc(size_t align, size_t size)
{
  // power of 2
  if (!align || (align & (align - 1)))
    throw std::runtime_error("xrt_core::aligned_alloc requires power of 2 for alignment");

#if defined(_WINDOWS)
  return aligned_ptr_t<MyType>(reinterpret_cast<MyType*>(_aligned_malloc(size, align)));
#else
  return aligned_ptr_t<MyType>(reinterpret_cast<MyType*>(::aligned_alloc(align, size)));
#endif
}

} // detail

// This type is used in legacy interfaces
using aligned_ptr_type = detail::aligned_ptr_t<void>;

// Expose templated detail type for convenience in use as data member
template <typename MyType>
using aligned_ptr_t = detail::aligned_ptr_t<MyType>;

// Untyped aligned memory allocation  
inline aligned_ptr_t<void>
aligned_alloc(size_t align, size_t size)
{
  return detail::aligned_alloc<void>(align, size);
}

// Typed aligned memory allocation  
template <typename MyType>
inline aligned_ptr_t<MyType>
aligned_alloc(size_t align)
{
  return detail::aligned_alloc<MyType>(align, sizeof(MyType));
}

} // xrt_core

#endif
