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

#ifndef xrtcore_xstdlib_h_
#define xrtcore_xstdlib_h_
#include <cstdlib>

namespace xrt_core { 

inline int
posix_memalign(void **memptr, size_t alignment, size_t size)
{
#if defined(__GNUC__)
  return ::posix_memalign(memptr,alignment,size);
#elif defined(_WINDOWS)
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

} // xrt_core

#endif
