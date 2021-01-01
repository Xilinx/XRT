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

#include "xocl/config.h"
#include "xocl/core/param.h"
#include "xocl/core/error.h"
#include "xocl/core/kernel.h"

#include "detail/kernel.h"

#include "plugin/xdp/profile.h"
#include "plugin/xdp/lop.h"

#include <CL/opencl.h>

namespace xocl {

static void
validOrError(cl_kernel          kernel ,
             cl_uint            arg_indx ,
             cl_kernel_arg_info param_name ,
             size_t             param_value_size ,
             void *             param_value ,
             size_t *           param_value_size_ret)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_KERNEL if kernel is not a valid kernel object.
  detail::kernel::validOrError(kernel);

  // CL_INVALID_ARG_INDEX if arg_indx is not a valid argument index.
  if (arg_indx >= xocl::xocl(kernel)->get_indexed_xargument_range().size())
    throw xocl::error(CL_INVALID_ARG_INDEX,"clGetKernelArgInfo: invalid arg idx (" + std::to_string(arg_indx) + ")\n");

  // CL_INVALID_VALUE if param_name is not valid, or if size in bytes
  // specified by param_value_size is < size of return type as
  // described in the table above and param_value is not NULL

  // CL_KERNEL_ARG_INFO_NOT_AVAILABLE if the argument information is
  // not available for kernel.

}

static cl_int
clGetKernelArgInfo(cl_kernel          kernel ,
                   cl_uint            arg_indx ,
                   cl_kernel_arg_info param_name ,
                   size_t             param_value_size ,
                   void *             param_value ,
                   size_t *           param_value_size_ret)
{
  validOrError(kernel,arg_indx,param_name,param_value_size,param_value,param_value_size_ret);

  xocl::param_buffer buffer { param_value, param_value_size, param_value_size_ret };
  const xrt_core::xclbin::kernel_argument* arginfo = xocl::xocl(kernel)->get_arg_info(arg_indx);

  switch(param_name) {
    case CL_KERNEL_ARG_ADDRESS_QUALIFIER:
      buffer.as<cl_kernel_arg_address_qualifier>() = static_cast<cl_kernel_arg_address_qualifier>(arginfo->type);
      break;
    case CL_KERNEL_ARG_ACCESS_QUALIFIER:
      buffer.as<cl_kernel_arg_access_qualifier>() = 0;
      break;
    case CL_KERNEL_ARG_TYPE_NAME:
      buffer.as<char>() = arginfo->hosttype;
      break;
    case CL_KERNEL_ARG_NAME:
      buffer.as<char>() = arginfo->name;
      break;
    case CL_KERNEL_ARG_OFFSET:
      buffer.as<size_t>() = arginfo->offset;
      break;
    default:
      throw error(CL_INVALID_VALUE,"clGetKernelArgInfo: invalid param_name");
      break;
  }

  return CL_SUCCESS;
}

} // xocl

CL_API_ENTRY cl_int CL_API_CALL
clGetKernelArgInfo(cl_kernel        kernel ,
                   cl_uint          arg_indx ,
                   cl_kernel_arg_info   param_name ,
                   size_t           param_value_size ,
                   void *           param_value ,
                   size_t *         param_value_size_ret ) CL_API_SUFFIX__VERSION_1_2
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::
      clGetKernelArgInfo
      (kernel,arg_indx,param_name,param_value_size,param_value,param_value_size_ret);
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
