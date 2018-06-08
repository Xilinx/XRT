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
#include "xocl/core/program.h"
#include "xocl/core/device.h"
#include "xocl/core/param.h"
#include "xocl/core/error.h"
#include "xocl/xclbin/xclbin.h"
#include "detail/program.h"
#include "detail/device.h"
#include "profile.h"

namespace xocl {

static void
validOrError(cl_program            program,
             cl_device_id          device,
             cl_program_build_info param_name,
             size_t                param_value_size,
             void *                param_value,
             size_t *              param_value_size_ret)
{
  if(!config::api_checks())
    return;

  detail::program::validOrError(program);
  detail::device::validOrError(program,device);
}

static cl_int 
clGetProgramBuildInfo(cl_program            program,
                      cl_device_id          device,
                      cl_program_build_info param_name,
                      size_t                param_value_size,
                      void *                param_value,
                      size_t *              param_value_size_ret)
{
  validOrError(program,device,param_name,param_value_size,param_value,param_value_size_ret);

  xocl::param_buffer buffer { param_value, param_value_size, param_value_size_ret };

  switch(param_name) {
  case CL_PROGRAM_BUILD_STATUS:
    buffer.as<cl_build_status>() = xocl::xocl(program)->get_build_status(xocl(device));
    break;
  case CL_PROGRAM_BUILD_OPTIONS:
    buffer.as<char>() = xocl::xocl(program)->get_build_options(xocl(device));
    break;
  case CL_PROGRAM_BUILD_LOG:
    buffer.as<char>() = xocl::xocl(program)->get_build_log(xocl(device));
    break;
  case CL_PROGRAM_TARGET_TYPE:
    {
      auto type = CL_PROGRAM_TARGET_TYPE_NONE;
      auto target = xocl::xocl(program)->get_target();
      if (target==xocl::xclbin::target_type::bin)
        type = CL_PROGRAM_TARGET_TYPE_HW;
      else if (target==xocl::xclbin::target_type::csim)
        type = CL_PROGRAM_TARGET_TYPE_SW_EMU;
      else if (target==xocl::xclbin::target_type::hwem)
        type = CL_PROGRAM_TARGET_TYPE_HW_EMU;
      buffer.as<cl_program_target_type>() = type;
    }
    break;
  case CL_PROGRAM_BINARY_TYPE:
    // Not curently used
    buffer.as<cl_program_binary_type>() = CL_PROGRAM_BINARY_TYPE_NONE;
    break;
  default:
    return CL_INVALID_VALUE;
    break;
  }

  return CL_SUCCESS;
}

} // api_impl

CL_API_ENTRY cl_int CL_API_CALL
clGetProgramBuildInfo(cl_program            program,
                      cl_device_id          device,
                      cl_program_build_info param_name,
                      size_t                param_value_size,
                      void *                param_value,
                      size_t *              param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{

  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clGetProgramBuildInfo
      (program, device,param_name, param_value_size, param_value, param_value_size_ret);
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


