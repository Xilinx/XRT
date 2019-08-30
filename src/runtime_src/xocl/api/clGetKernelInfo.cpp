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
#include "xocl/config.h"
#include "xocl/core/param.h"
#include "xocl/core/error.h"
#include "xocl/core/kernel.h"
#include "xocl/core/program.h"
#include "xocl/core/context.h"
#include "xocl/core/compute_unit.h"
#include "xocl/xclbin/xclbin.h"

#include "detail/kernel.h"

#include "plugin/xdp/profile.h"

namespace xocl {

static void
validOrError(cl_kernel        kernel,
             cl_kernel_info   param_name,
             size_t           param_value_size,
             void *           param_value,
             size_t *         param_value_size_ret )
{
  if (!config::api_checks())
    return;

  // CL_INVALID_VALUE if param_name is not valid, or if size in bytes
  // specified by param_value_size is < size of return type as
  // described in the table above and param_value is not NULL.

  // CL_INVALID_KERNEL if kernel is not a valid kernel object.
  detail::kernel::validOrError(kernel);

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
}

static cl_int
clGetKernelInfo(cl_kernel        kernel,
                cl_kernel_info   param_name,
                size_t           param_value_size,
                void *           param_value,
                size_t *         param_value_size_ret )
{
  validOrError(kernel,param_name,param_value_size,param_value,param_value_size_ret);

  xocl::param_buffer buffer { param_value, param_value_size, param_value_size_ret };

  switch(param_name){
    case CL_KERNEL_FUNCTION_NAME:
      buffer.as<char>() = xocl(kernel)->get_name();
      break;
    case CL_KERNEL_NUM_ARGS:
      buffer.as<cl_uint>() = xocl(kernel)->get_indexed_argument_range().size();
      break;
    case CL_KERNEL_REFERENCE_COUNT:
      buffer.as<cl_uint>() = xocl(kernel)->count();
      break;
    case CL_KERNEL_CONTEXT:
      buffer.as<cl_context>() = xocl(kernel)->get_program()->get_context();
      break;
    case CL_KERNEL_PROGRAM:
      buffer.as<cl_program>() = xocl(kernel)->get_program();
      break;
    case CL_KERNEL_ATTRIBUTES:
      buffer.as<char>() = xocl(kernel)->get_attributes();
      break;
    case CL_KERNEL_COMPUTE_UNIT_COUNT:
      buffer.as<cl_uint>() = xocl(kernel)->get_cus().size();
      break;
    case CL_KERNEL_INSTANCE_BASE_ADDRESS: 
      for (auto cu : xocl(kernel)->get_cus())
        buffer.as<size_t>() = cu->get_base_addr();
      break;
    default:
      throw error(CL_INVALID_VALUE,"clGetKernelInfo invalud param name");
      break;
  }

  return CL_SUCCESS;
}

} // xocl

extern CL_API_ENTRY cl_int CL_API_CALL
clGetKernelInfo(cl_kernel        kernel,
                cl_kernel_info   param_name,
                size_t           param_value_size,
                void *           param_value,
                size_t *         param_value_size_ret ) CL_API_SUFFIX__VERSION_1_0
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clGetKernelInfo
      (kernel,param_name,param_value_size,param_value,param_value_size_ret);
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



