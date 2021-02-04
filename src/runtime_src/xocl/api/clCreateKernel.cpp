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
#include "xocl/core/platform.h"
#include "xocl/core/kernel.h"
#include "xocl/core/program.h"
#include "xocl/core/device.h"
#include "xocl/core/error.h"

#include "detail/program.h"
#include "api.h"
#include "plugin/xdp/profile_v2.h"

#include <CL/opencl.h>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <fstream>

namespace bfs = boost::filesystem;

#ifdef _WIN32
# pragma warning ( disable : 4996 4189 4505 )
#endif

namespace {

XRT_UNUSED static size_t
getDeviceMaxWorkGroupSize(cl_device_id device)
{
  static size_t size = 0;
  if (size)
    return size;
  xocl::api::clGetDeviceInfo(device,CL_DEVICE_MAX_WORK_GROUP_SIZE,sizeof(size_t),&size,nullptr);
  return size;
}

}

namespace xocl {

static void
validOrError(cl_program      program,
             const char *    kernel_name,
             cl_int *        errcode_ret)
{
  if( !xocl::config::api_checks())
    return;

  // CL_INVALID_PROGRAM if program is not a valid program object.
  detail::program::validOrError(program);

  // CL_INVALID_VALUE if kernel_name is NULL.
  if (!kernel_name)
    throw xocl::error(CL_INVALID_VALUE,"kernel_name == nullptr");

  // CL_INVALID_PROGRAM_EXECUTABLE if there is no successfully built
  // executable for program.
  detail::program::validExecutableOrError(program);

  // CL_INVALID_KERNEL_NAME if kernel_name is not found in program.
  if (!xocl(program)->has_kernel(kernel_name))
    throw xocl::error(CL_INVALID_KERNEL_NAME,"kernel '" + std::string(kernel_name) + "' not found");

  // CL_INVALID_KERNEL_DEFINITION if the function definition for
  // __kernel function given by kernel_name such as the number of
  // arguments, the argument types are not the same for all devices
  // for which the program executable has been built.

}

static cl_kernel
clCreateKernel(cl_program      program,
               const char *    kernel_name,
               cl_int *        errcode_ret)
{
  validOrError(program,kernel_name,errcode_ret);

  auto pkernel = xocl::xocl(program)->create_kernel(kernel_name);
  xocl::assign(errcode_ret,CL_SUCCESS);
  return pkernel.release();
}

namespace api {

cl_kernel
clCreateKernel(cl_program      program,
               const char *    kernel_name,
               cl_int *        errcode_ret)
{
  return ::xocl::clCreateKernel(program,kernel_name,errcode_ret);
}

} // api

} // xocl

cl_kernel
clCreateKernel(cl_program      program,
               const char *    kernel_name,
               cl_int *        errcode_ret)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clCreateKernel(program,kernel_name,errcode_ret);
  }
  catch (const xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,ex.get_code());
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,CL_OUT_OF_HOST_MEMORY);
  }
  return nullptr;
}
