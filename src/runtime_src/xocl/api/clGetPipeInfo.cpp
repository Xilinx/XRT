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

#include "xocl/core/memory.h"
#include "xocl/core/object.h"
#include "xocl/core/param.h"
#include "xocl/core/error.h"
#include "xoclProfile.h"
#include "xrt/config.h"

namespace xocl {

static void 
validOrError(cl_mem pipe) 
{
  if(!config::api_checks())
    return;

  if(!pipe)
    throw error(CL_INVALID_MEM_OBJECT); 
}

static cl_int
clGetPipeInfo(cl_mem           pipe,
              cl_mem_info      param_name, 
              size_t           param_value_size,
              void *           param_value,
              size_t *         param_value_size_ret )
{
  validOrError(pipe);

  xocl::param_buffer buffer { param_value, param_value_size, param_value_size_ret };

  switch(param_name){
  case CL_PIPE_PACKET_SIZE:
    buffer.as<cl_uint>() = xocl::xocl(pipe)->get_pipe_packet_size();
    break;
  case CL_PIPE_MAX_PACKETS:
    buffer.as<cl_uint>() = xocl::xocl(pipe)->get_pipe_max_packets();
    break;
  default:
    return CL_INVALID_VALUE;
    break;
  }

  return CL_SUCCESS;
}

} // xocl

cl_int 
clGetPipeInfo(cl_mem           pipe,
              cl_mem_info      param_name, 
              size_t           param_value_size,
              void *           param_value,
              size_t *         param_value_size_ret ) 
{
  try {
    PROFILE_LOG_FUNCTION_CALL
    return xocl::
      clGetPipeInfo
      (pipe,param_name,param_value_size,param_value,param_value_size_ret);
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



