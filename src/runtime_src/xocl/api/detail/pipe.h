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

#ifndef xocl_api_pmd_detail_pipe_h_
#define xocl_api_pmd_detail_pipe_h_

#include "CL/cl.h"
#include "xocl/core/pipe.h"

namespace xocl { namespace pmd { namespace detail {

namespace pipe {

void
validOrError(const cl_pipe);

void
validOrError(const cl_pipe pipe,const cl_command_queue command_queue);

} // pipe

 }}} // pmd,detail,xocl

#endif


