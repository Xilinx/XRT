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

#ifndef xocl_api_detail_context_h_
#define xocl_api_detail_context_h_

#include "xocl/config.h"
#include "CL/cl.h"
#include <vector>

namespace xocl { namespace detail {

namespace context {

void
validOrError(const cl_context context);

void
validOrError(const cl_context_properties* properties);

void
validOrError(const cl_context context,const std::vector<cl_mem>& mem_objects);

}

}} // detail,xocl

#endif
