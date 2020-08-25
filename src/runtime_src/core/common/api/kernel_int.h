/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
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

#ifndef _XRT_COMMON_KERNEL_INT_H_
#define _XRT_COMMON_KERNEL_INT_H_

// This file defines implementation extensions to the XRT Kernel APIs.
#include "core/include/experimental/xrt_kernel.h"

namespace xrt_core { namespace kernel_int {

// Provide access to kdma command based BO copy Used by xrt::bo::copy.
// Arguably this should implemented by by shim->copy_bo, but must wait
// until execbuf()/execwait() can handle multithreading with one device .
void
copy_bo_with_kdma(const std::shared_ptr<xrt_core::device>& core_device,
                  size_t sz,
                  xclBufferHandle dst_bo, size_t dst_offset,
                  xclBufferHandle src_bo, size_t src_offset);

}} // device_int, xrt_core

#endif
