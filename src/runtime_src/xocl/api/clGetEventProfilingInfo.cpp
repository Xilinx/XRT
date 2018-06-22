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

#include <CL/opencl.h>
#include "xocl/core/param.h"
#include "xocl/core/error.h"
#include "xocl/core/event.h"
#include "xocl/config.h"
#include "detail/event.h"
#include "profile.h"


namespace xocl {

XRT_UNUSED
static void 
validOrError(const cl_event event)   
{
  if(!config::api_checks())
    return;

  detail::event::validOrError(event); 

  auto command_queue = xocl::xocl(event)->get_command_queue();
  if( (!((xocl::xocl(command_queue)->get_properties()) & CL_QUEUE_PROFILING_ENABLE)) ||
      (xocl::xocl(event)->get_status()!=CL_COMPLETE) ||
      (xocl::xocl(event)->get_command_type()==CL_COMMAND_USER)
      ) {
    throw error(CL_PROFILING_INFO_NOT_AVAILABLE);
  }
}
  
static cl_int
clGetEventProfilingInfo(cl_event             event ,
                        cl_profiling_info    param_name ,
                        size_t               param_value_size ,
                        void *               param_value ,
                        size_t *             param_value_size_ret )
{

  //validOrError(event);
  
  xocl::param_buffer buffer { param_value, param_value_size, param_value_size_ret };

  switch(param_name) {
  case CL_PROFILING_COMMAND_QUEUED:
    buffer.as<cl_ulong>() = xocl::xocl(event)->time_queued();
    break;
  case CL_PROFILING_COMMAND_SUBMIT:
    buffer.as<cl_ulong>() = xocl::xocl(event)->time_submit();
    break;
  case CL_PROFILING_COMMAND_START:
    buffer.as<cl_ulong>() = xocl::xocl(event)->time_start();
    break;
  case CL_PROFILING_COMMAND_END:
    buffer.as<cl_ulong>() = xocl::xocl(event)->time_end();
    break;
  default:
    return CL_INVALID_VALUE;
    break;
  }     

  return CL_SUCCESS;
}

} //xocl

cl_int 
clGetEventProfilingInfo(cl_event             event ,
                        cl_profiling_info    param_name ,
                        size_t               param_value_size ,
                        void *               param_value ,
                        size_t *             param_value_size_ret ) 
{
  try {
    PROFILE_LOG_FUNCTION_CALL
    return xocl::
      clGetEventProfilingInfo
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
}



