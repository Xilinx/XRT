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

#ifndef _XRT_COMMON_XCLBIN_INT_H_
#define _XRT_COMMON_XCLBIN_INT_H_

// This file defines implementation extensions to the XRT XCLBIN APIs.
#include "core/include/experimental/xrt_xclbin.h"

namespace xrt_core {
namespace xclbin_int {

/**
 * is_valid_or_error() - Returns the validity of the xrtXclbinHandle handle.
 *
 * @handle:        Xclbin handle
 *
 * Throws if @handle is invalid
 */
void
is_valid_or_error(xrtXclbinHandle handle);

/**
 * get_xclbin_data() - Returns the data of the xrtXclbinHandle handle.
 *
 * @handle:        Xclbin handle
 * Return:         Data of the @handle
 *
 * Throws if @handle is invalid.
 */
const std::vector<char>&
get_xclbin_data(xrtXclbinHandle handle);

} //xclbin_int
}; // xrt_core

#endif
