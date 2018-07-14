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

#include "xocl/core/param.h"
#include "xocl/core/error.h"
#include "xocl/core/device.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/context.h"
#include "detail/command_queue.h"

#include "xocl/config.h"
#include "xoclProfile.h"


namespace xocl 
{

static void
validOrError(const cl_context context) 
{
  if(!config::api_checks())
   return;

 //older runtime checks against a global list.
 if(!context)
   throw error(CL_INVALID_CONTEXT); 
}
 
static cl_int
clGetContextInfo(cl_context         context,
                 cl_context_info    param_name,
                 size_t             param_value_size,
                 void *             param_value,
                 size_t *           param_value_size_ret)
{
  validOrError(context);

  xocl::param_buffer buffer { param_value, param_value_size, param_value_size_ret };
  auto xcontext = xocl::xocl(context);

  switch(param_name){
  case CL_CONTEXT_REFERENCE_COUNT:
    buffer.as<cl_uint>() = xcontext->count();
    break;
  case CL_CONTEXT_NUM_DEVICES:
    buffer.as<cl_uint>() = xcontext->num_devices();
    break;
  case CL_CONTEXT_DEVICES:
    buffer.as<cl_device_id>() = xcontext->get_device_range();
    break;
  case CL_CONTEXT_PROPERTIES:
    for (auto prop : xcontext->get_properties()) {
      buffer.as<cl_context_properties>() = prop.get_key();
      buffer.as<cl_context_properties>() = prop.get_value();
    }
    // null terminate
    buffer.as<cl_context_properties>() = static_cast<cl_context_properties>(0);
    break;
  default:
    return CL_INVALID_VALUE;
    break;
  }

  return CL_SUCCESS;
}

} //xocl

cl_int 
clGetContextInfo(cl_context         context,
                 cl_context_info    param_name,
                 size_t             param_value_size,
                 void *             param_value,
                 size_t *           param_value_size_ret) 
{
  try {
    PROFILE_LOG_FUNCTION_CALL
    return xocl::
      clGetContextInfo
      (context,param_name,param_value_size,param_value,param_value_size_ret);
  }
  catch (const xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
  return CL_SUCCESS;
}



