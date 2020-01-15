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
#include "xocl/config.h"
#include "xocl/core/kernel.h"
#include "xocl/core/program.h"
#include "xocl/core/error.h"

#include "detail/program.h"
#include "api.h"
#include "plugin/xdp/profile.h"
#include <CL/opencl.h>

namespace xocl {

static void
validOrError(cl_program      program ,
             cl_uint         num_kernels ,
             cl_kernel *     kernels ,
             cl_uint *       num_kernels_ret )
{
  if (!config::api_checks())
    return;

  // CL_INVALID_PROGRAM if program is not a valid program object.
  detail::program::validOrError(program);

  // CL_INVALID_PROGRAM_EXECUTABLE if there is no successfully built
  // executable for any device in program.
  detail::program::validExecutableOrError(program);

  // CL_INVALID_VALUE if kernels is not NULL and num_kernels is less
  // than the number of kernels in program.
  if (kernels && (xocl::xocl(program)->get_num_kernels()>num_kernels))
    throw xocl::error(CL_INVALID_VALUE,"num_kernels less than number of kernels in program");

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
}

static cl_int
clCreateKernelsInProgram(cl_program      program ,
                         cl_uint         num_kernels ,
                         cl_kernel *     kernels ,
                         cl_uint *       num_kernels_ret )
{
  validOrError(program,num_kernels,kernels,num_kernels_ret);

  //find collect and copy cl_decls
  cl_uint idx = 0;
  for (auto& kernel_name : xocl(program)->get_kernel_names()) {
    if (kernels)
      kernels[idx] = api::clCreateKernel(program,kernel_name.c_str(),nullptr);
    ++idx;
  }
  xocl::assign(num_kernels_ret,idx);

  return CL_SUCCESS;
}

} // xocl

cl_int
clCreateKernelsInProgram(cl_program      program ,
                         cl_uint         num_kernels ,
                         cl_kernel *     kernels ,
                         cl_uint *       num_kernels_ret )
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clCreateKernelsInProgram
      (program,num_kernels,kernels,num_kernels_ret);
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
