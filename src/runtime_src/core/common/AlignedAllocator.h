/*
 * Copyright (C) 2019, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) APIs
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


#ifndef ALINGED_ALLOCATOR_H
#define ALINGED_ALLOCATOR_H

#include "core/common/memalign.h"


namespace xrt_core {

  // Memory alignment for DDR and AXI-MM trace access
  template <typename T> class AlignedAllocator {
      void *mBuffer;
      size_t mCount;
  public:
      T *getBuffer() {
          return (T *)mBuffer;
      }

      size_t size() const {
          return mCount * sizeof(T);
      }

      AlignedAllocator(size_t alignment, size_t count) : mBuffer(0), mCount(count) {
        if (xrt_core::posix_memalign(&mBuffer, alignment, count * sizeof(T))) {
              mBuffer = 0;
          }
      }
      ~AlignedAllocator() {
        if (mBuffer) {
          xrt_core::aligned_ptr_deleter pDel;
          pDel(mBuffer);
        }
      }
  };
}
#endif
