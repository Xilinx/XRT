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

// Copyright 2016 Xilinx, Inc. All rights reserved.

#include "xocl/config.h"
#include "xocl/core/param.h"
#include "xocl/core/error.h"
#include "xocl/core/object.h"
#include "xocl/core/device.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/context.h"
#include "detail/command_queue.h"

#include "plugin/xdp/profile_v2.h"

namespace xocl {

static void
validOrError(const cl_command_queue command_queue)
{
  if(!config::api_checks())
    return;
  detail::command_queue::validOrError(command_queue);
}

cl_int
clGetCommandQueueInfo(cl_command_queue       command_queue ,
                      cl_command_queue_info  param_name ,
                      size_t                 param_value_size ,
                      void *                 param_value ,
                      size_t *               param_value_size_ret )
{
  validOrError(command_queue);

  xocl::param_buffer buffer { param_value, param_value_size, param_value_size_ret };

  switch(param_name){
  case CL_QUEUE_CONTEXT:
    buffer.as<cl_context>() = xocl::xocl(command_queue)->get_context();
    break;
  case CL_QUEUE_DEVICE:
    buffer.as<cl_device_id>() = xocl::xocl(command_queue)->get_device();
    break;
  case CL_QUEUE_REFERENCE_COUNT:
    buffer.as<cl_uint>() = xocl::xocl(command_queue)->count();
    break;
  case CL_QUEUE_PROPERTIES:
    buffer.as<cl_command_queue_properties>() = xocl::xocl(command_queue)->get_properties();
    break;
  default:
    return CL_INVALID_VALUE;
    break;
  }

  return CL_SUCCESS;
}

} // xocl


cl_int
clGetCommandQueueInfo(cl_command_queue       command_queue ,
                      cl_command_queue_info  param_name ,
                      size_t                 param_value_size ,
                      void *                 param_value ,
                      size_t *               param_value_size_ret )
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::
      clGetCommandQueueInfo
      (command_queue,param_name,param_value_size,param_value,param_value_size_ret);
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
