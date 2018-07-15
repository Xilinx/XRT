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

// Copyright 2017 Xilinx, Inc. All rights reserved.

#include <CL/opencl.h>
#include "xocl/core/pipe.h"
#include "xocl/api/detail/pipe.h"
#include "xocl/core/error.h"

namespace xocl {

static void
validOrError(cl_command_queue command_queue, 
             cl_pipe          pipe,
             rte_mbuf*        buf)
{
  if (!config::api_checks())
    return;

  pmd::detail::pipe::validOrError(pipe,command_queue);
}

static cl_int
clReleasePipeBuffer(cl_command_queue command_queue,
                    cl_pipe          pipe,
                    rte_mbuf*        buf)
{
  validOrError(command_queue,pipe,buf);
  if (xocl::xocl(pipe)->release())
    delete xocl::xocl(pipe);

  return CL_SUCCESS;
}

} // xocl              

int
clReleasePipeBuffer(cl_command_queue command_queue,
                    cl_pipe          pipe,
                    rte_mbuf*        buf)
{
  try {
    return xocl::clReleasePipeBuffer(command_queue,pipe,buf);
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}


