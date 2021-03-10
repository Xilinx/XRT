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

#ifndef xocl_api_detail_platform_h_
#define xocl_api_detail_platform_h_

#include "xocl/config.h"
#include "CL/cl.h"

namespace xocl { namespace detail {

namespace platform {

void
validOrError(const cl_platform_id platform);

void
validOrError(cl_uint num_entries, const cl_platform_id* platforms);

}

}} // detail,xocl

#endif
