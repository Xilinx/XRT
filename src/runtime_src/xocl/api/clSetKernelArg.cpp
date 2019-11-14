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
#include "plugin/xdp/profile.h"
#include <CL/opencl.h>
#include <cstdlib>

#ifdef _WIN32
# pragma warning ( disable : 4996 )
#endif

namespace {

inline bool
xcl_conformancecollect()
{
  static bool val = getenv("XCL_CONFORMANCECOLLECT") != nullptr;
  return val;
}

}

namespace xocl {

static void
validOrError(cl_kernel    kernel,
             cl_uint      arg_index,
             size_t       arg_size,
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

  // CL_INVALID_MEM_OBJECT for an argument declared to be a memory
  // object when the specified arg_value is not a valid memory object.
  // checked in core/kernel::set_arg

  // CL_INVALID_MEM_OBJECT for an argument declared to be a
  // multi-sample image, multisample image array, multi-sample depth
  // image or a multi-sample depth image array and the argument value
  // specified in arg_value does not follow the rules described above
  // for a depth memory object or memory array object
  // argument. (Applies if the cl_khr_gl_msaa_sharing extension is
  // supported.)
  // checked in core/kernel::set_arg

  // CL_INVALID_SAMPLER for an argument declared to be of type
  // sampler_t when the specified arg_value is not a valid sampler
  // object.
  // checked in core/kernel::set_arg

  // CL_INVALID_DEVICE_QUEUE for an argument declared to be of type
  // queue_t when the specified arg_value is not a valid device queue
  // object.
  // checked in core/kernel::set_arg

  // CL_INVALID_ARG_SIZE if arg_size does not match the size of the
  // data type for an argument that is not a memory object or if the
  // argument is a memory object and arg_size != sizeof(cl_mem) or if
  // arg_size is zero and the argument is declared with the local
  // qualifier or if the argument is a sampler and arg_size !=
  // sizeof(cl_sampler).
  // checked in core/kernel::set_arg

  // CL_INVALID_ARG_VALUE if the argument is an image declared with
  // the read_only qualifier and arg_value refers to an image object
  // created with cl_mem_flags of CL_MEM_WRITE or if the image
  // argument is declared with the write_only qualifier and arg_value
  // refers to an image object created with cl_mem_flags of
  // CL_MEM_READ.
  // checked in core/kernel::set_arg

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
}

static cl_int
clSetKernelArg(cl_kernel    kernel,
               cl_uint      arg_index,
               size_t       arg_size,
               const void * arg_value)
{
  validOrError(kernel,arg_index,arg_size,arg_value);

  // XCL_CONFORMANCECOLLECT mode, not sure why return here?
  if (xcl_conformancecollect())
    return CL_SUCCESS;

  // May throw out-of-range
  xocl(kernel)->set_argument(arg_index,arg_size,arg_value);

  return CL_SUCCESS;
}

namespace api {

cl_int
clSetKernelArg(cl_kernel    kernel,
               cl_uint      arg_index,
               size_t       arg_size,
               const void * arg_value)
{
  return ::xocl::clSetKernelArg(kernel,arg_index,arg_size,arg_value);
}

} // api

} // xocl

cl_int
clSetKernelArg(cl_kernel    kernel,
               cl_uint      arg_index,
               size_t       arg_size,
               const void * arg_value)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clSetKernelArg(kernel,arg_index,arg_size,arg_value);
  }
  catch (const xocl::error& ex) {
    std::string msg = ex.what();
    msg += "\nERROR: clSetKernelArg() for kernel \"" + xocl::xocl(kernel)->get_name() + "\", argument index " + std::to_string(arg_index) + ".";
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
