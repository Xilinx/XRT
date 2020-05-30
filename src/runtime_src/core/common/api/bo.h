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

#ifndef _XRT_COMMON_BO_H_
#define _XRT_COMMON_BO_H_

// This file defines implementation extensions to the XRT BO APIs.
#include "core/include/experimental/xrt_bo.h"

namespace xrt_core { namespace bo {

/**
 * address() - Get physical device address of BO
 *
 * @handle:        Buffer handle
 * Return:         Device address of BO
 */
uint64_t
address(const xrt::bo& bo);
uint64_t
address(xrtBufferHandle handle);
  
}} // namespace bo, xrt_core

#endif
