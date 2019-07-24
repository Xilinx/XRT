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

#include "xocl/core/kernel.h"
#include "xocl/core/param.h"
#include "xocl/core/error.h"
#include "detail/kernel.h"
#include "api.h"
#include "plugin/xdp/profile.h"

namespace xocl {

static cl_device_type
getDeviceType(cl_device_id device)
{
  cl_device_type type = CL_DEVICE_TYPE_DEFAULT;
  xocl::api::clGetDeviceInfo(device,CL_DEVICE_TYPE,sizeof(cl_device_type),&type,nullptr);
  return type;
}

static void
validOrError(cl_kernel                 kernel,
             cl_device_id              device,
             cl_kernel_work_group_info param_name,
             size_t                    param_value_size,
             void *                    param_value,
             size_t *                  param_value_size_ret)
{
  if(!config::api_checks())
    return;

  detail::kernel::validOrError(kernel);
  detail::kernel::validOrError(device,kernel);

  // CL_INVALID_VALUE if param_name is CL_KERNEL_GLOBAL_WORK_SIZE and
  // device is not a custom device and kernel is not a built-in
  // kernel.
  if(param_name==CL_KERNEL_GLOBAL_WORK_SIZE &&
     (device && getDeviceType(device)!=CL_DEVICE_TYPE_CUSTOM) &&
     (!xocl(kernel)->is_built_in())
    )
    throw error(CL_INVALID_VALUE);

}

static cl_int
clGetKernelWorkGroupInfo(cl_kernel                 kernel,
                         cl_device_id              device,
                         cl_kernel_work_group_info param_name,
                         size_t                    param_value_size,
                         void *                    param_value,
                         size_t *                  param_value_size_ret)
{
  validOrError(kernel,device,param_name,param_value_size,param_value,param_value_size_ret);

  xocl::param_buffer buffer { param_value, param_value_size, param_value_size_ret };

  switch(param_name) {
  case CL_KERNEL_GLOBAL_WORK_SIZE:
    throw error(-20,"Not implemented");
    break;
  case CL_KERNEL_WORK_GROUP_SIZE:
    buffer.as<size_t>() = xocl::xocl(kernel)->get_wg_size();
    break;
  case CL_KERNEL_COMPILE_WORK_GROUP_SIZE:
    buffer.as<size_t>() = xocl::xocl(kernel)->get_compile_wg_size_range();
    break;
  case CL_KERNEL_LOCAL_MEM_SIZE:
    buffer.as<cl_ulong>() = 0;
    break;
  case CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE:
    throw error(-20,"Not implemented");
    break;
  case CL_KERNEL_PRIVATE_MEM_SIZE:
    buffer.as<cl_ulong>() = 0;
    break;
  default:
    throw error(CL_INVALID_VALUE);
    break;
  }

  return CL_SUCCESS;
}

} // xocl


cl_int
clGetKernelWorkGroupInfo(cl_kernel                 kernel,
                         cl_device_id              device,
                         cl_kernel_work_group_info param_name,
                         size_t                    param_value_size,
                         void *                    param_value,
                         size_t *                  param_value_size_ret)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::
      clGetKernelWorkGroupInfo
      (kernel, device, param_name, param_value_size, param_value, param_value_size_ret);
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
