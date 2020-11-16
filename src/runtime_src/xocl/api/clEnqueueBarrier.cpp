/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

// Copyright 2017-2020 Xilinx, Inc. All rights reserved.

#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#include "xocl/config.h"
#include "xocl/core/command_queue.h"
#include "detail/command_queue.h"
#include "plugin/xdp/profile.h"
#include "plugin/xdp/lop.h"
#include <CL/opencl.h>

namespace xocl {

static void
validOrError(cl_command_queue command_queue)
{
  if (!config::api_checks())
    return;

  detail::command_queue::validOrError(command_queue);
}

static cl_int
clEnqueueBarrier(cl_command_queue command_queue )
{
  validOrError(command_queue);
  xocl(command_queue)->wait();
  return CL_SUCCESS;
}

} // xocl


cl_int
clEnqueueBarrier(cl_command_queue command_queue)
{
  try {
    PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    LOP_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    return xocl::clEnqueueBarrier(command_queue);
  }
  catch (const xrt_xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}
