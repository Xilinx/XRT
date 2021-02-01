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
#include "detail/kernel.h"
#include "plugin/xdp/profile_v2.h"
#include <CL/opencl.h>
#include <cstdlib>

#ifdef _WIN32
# pragma warning ( disable : 4996 )
#endif

namespace xocl {

static void
validOrError(cl_kernel    kernel,
             cl_uint      arg_index,
             const void * arg_value)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_KERNEL if kernel is not a valid kernel object.
  detail::kernel::validOrError(kernel);

  // CL_INVALID_ARG_INDEX if arg_index is not a valid argument index.
  // checked in core/kernel::set_arg

  // CL_INVALID_ARG_VALUE if arg_value specified is not a valid value.
  // checked in core/kernel::set_arg

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
}

static cl_int
clSetKernelArgSVMPointer(cl_kernel    kernel,
               cl_uint      arg_index,
               const void * arg_value)
{
  validOrError(kernel,arg_index,arg_value);

  // XCL_CONFORMANCECOLLECT mode, not sure why return here?
  if (getenv("XCL_CONFORMANCECOLLECT"))
    return CL_SUCCESS;

  // May throw out-of-range
  xocl(kernel)->set_svm_argument(arg_index, sizeof(void *), arg_value);

  return CL_SUCCESS;
}

} // xocl

cl_int
clSetKernelArgSVMPointer(cl_kernel    kernel,
               cl_uint      arg_index,
               const void * arg_value)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clSetKernelArgSVMPointer(kernel, arg_index, arg_value);
  }
  catch (const xocl::error& ex) {
    std::string msg = ex.what();
    msg += "\nERROR: clSetKernelArgSVMPointer() for kernel \"" + xocl::xocl(kernel)->get_name() + "\", argument index " + std::to_string(arg_index) + ".\n";
    xocl::send_exception_message(msg.c_str());
    return ex.get_code();
  }
  catch (const std::out_of_range&) {
    std::string msg = "bad kernel argument index " + std::to_string(arg_index);
    xocl::send_exception_message(msg.c_str());
    return CL_INVALID_ARG_INDEX;
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_RESOURCES;
  }
}
