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

#include "xocl/core/context.h"
#include "xocl/core/object.h"
#include "xocl/core/param.h"
#include "xocl/core/error.h"
#include "xocl/core/event.h"
#include "xocl/config.h"
#include "detail/event.h"
#include "plugin/xdp/profile.h"

namespace xocl {

static void
validOrError(const cl_event event) 
{
  if(!config::api_checks())
    return;
 detail::event::validOrError(event); 
}

static cl_int
clGetEventInfo(cl_event          event ,
               cl_event_info     param_name ,
               size_t            param_value_size ,
               void *            param_value ,
               size_t *          param_value_size_ret )
{
  validOrError(event);

  xocl::param_buffer buffer { param_value, param_value_size, param_value_size_ret };

  switch(param_name){
    case CL_EVENT_COMMAND_QUEUE:
      if(xocl::xocl(event)->get_command_type()==CL_COMMAND_USER)
        buffer.as<cl_command_queue>() = nullptr;
      else
        buffer.as<cl_command_queue>() = xocl::xocl(event)->get_command_queue();
      break;
    case CL_EVENT_CONTEXT:
      buffer.as<cl_context>() = xocl::xocl(event)->get_context();
      break;
    case CL_EVENT_COMMAND_TYPE:
      buffer.as<cl_command_type>() = xocl::xocl(event)->get_command_type();
      break;
     case CL_EVENT_COMMAND_EXECUTION_STATUS:
       buffer.as<cl_int>() = xocl::xocl(event)->get_status();
      break;
     case CL_EVENT_REFERENCE_COUNT:
       buffer.as<cl_uint>() = xocl::xocl(event)->count();
      break;
     default:
      return CL_INVALID_VALUE;
      break;
  }     
  return CL_SUCCESS;
}

}

cl_int 
clGetEventInfo(cl_event          event ,
               cl_event_info     param_name ,
               size_t            param_value_size ,
               void *            param_value ,
               size_t *          param_value_size_ret ) 
{
  try {
    PROFILE_LOG_FUNCTION_CALL
    return xocl::
      clGetEventInfo
      (event,param_name,param_value_size,param_value,param_value_size_ret);
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
  return CL_SUCCESS;
}




