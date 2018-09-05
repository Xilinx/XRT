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

#include "xocl/core/program.h"
#include "xocl/core/context.h"
#include "xocl/core/device.h"
#include "xocl/core/range.h"
#include "xocl/core/error.h"
#include "xrt/util/memory.h"

#include "detail/context.h"
#include "detail/device.h"
#include "plugin/xdp/profile.h"

namespace xocl {

static void
validOrError(cl_context          context ,
               cl_uint             num_devices ,
               const cl_device_id* device_list ,
               const char *        kernel_names)
{
  if(!config::api_checks())
    return;

  detail::context::validOrError(context);
  detail::device::validOrError(context,num_devices,device_list);
}

static cl_program 
clCreateProgramWithBuiltInKernels(cl_context          context ,
                                  cl_uint             num_devices ,
                                  const cl_device_id* device_list ,
                                  const char *        kernel_names ,
                                  cl_int *            errcode_ret )
{
  validOrError(context,num_devices,device_list,kernel_names);
  auto program = xrt::make_unique<xocl::program>(xocl::xocl(context));
  for (auto d : xocl::get_range(device_list,device_list+num_devices)) {
    program->add_device(xocl(d));
  }
  throw xocl::error(CL_INVALID_PROGRAM,"clCreateProgramWithBuiltInKernels is not supported");

#if 0
  xocl::assign(errcode_ret,CL_SUCCESS);
  return program.release();
#endif
}

} // api_impl

cl_program
clCreateProgramWithBuiltInKernels(cl_context          context ,
                                  cl_uint             num_devices ,
                                  const cl_device_id* device_list ,
                                  const char *        kernel_names ,
                                  cl_int *            errcode_ret )
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clCreateProgramWithBuiltInKernels
      (context, num_devices, device_list, kernel_names, errcode_ret);
  }
  catch (const xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    if (errcode_ret)
      *errcode_ret = ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    if (errcode_ret)
      *errcode_ret = CL_OUT_OF_HOST_MEMORY;
  }

  return nullptr;
}



