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

#define CL_USE_DEPRECATED_OPENCL_1_0_APIS

#include "xocl/config.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/device.h"

#include "detail/command_queue.h"

#include "api.h"
#include "plugin/xdp/profile_v2.h"

#include <CL/opencl.h>

#ifdef _WIN32
# pragma warning ( disable : 4245 )
#endif

namespace xocl {

static void
validOrError(cl_command_queue command_queue,
             cl_command_queue_properties properties,
             cl_bool enable,
             cl_command_queue_properties *old_properties)
{
  if (!config::api_checks())
    return;

  detail::command_queue::validOrError(command_queue);
  detail::command_queue::validOrError(xocl(command_queue)->get_device(),properties);
}

static cl_int
clSetCommandQueueProperty(cl_command_queue command_queue,
                          cl_command_queue_properties properties,
                          cl_bool enable,
                          cl_command_queue_properties *old_properties)
{
  validOrError(command_queue,properties,enable,old_properties);

  if (old_properties)
    *old_properties = xocl(command_queue)->get_properties();

  //CL_QUEUE_PROFILING_ENABLE
  if (properties & CL_QUEUE_PROFILING_ENABLE) {
    if (enable)
      xocl(command_queue)->get_properties() |= CL_QUEUE_PROFILING_ENABLE;
    else
      xocl(command_queue)->get_properties() &= (~CL_QUEUE_PROFILING_ENABLE);
  }

  //CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE
  if(properties & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE) {
    // block until all previousls queued commands in command_queue have completed
    // prevent new commands from being enqueued until properties are changed
    auto lk = xocl::xocl(command_queue)->wait_and_lock();
    if (enable)
      xocl(command_queue)->get_properties() |= CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    else
      xocl(command_queue)->get_properties() &= (~CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
  }

  return CL_SUCCESS;
}

} // xocl

cl_int
clSetCommandQueueProperty(cl_command_queue command_queue,
                          cl_command_queue_properties properties,
                          cl_bool enable,
                          cl_command_queue_properties *old_properties)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clSetCommandQueueProperty
      (command_queue,properties,enable,old_properties);
  }
  catch (const xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}
