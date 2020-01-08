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
#include "xocl/core/param.h"
#include "xocl/core/error.h"
#include "xocl/core/kernel.h"
#include "xocl/core/program.h"
#include "xocl/core/context.h"
#include "xocl/core/device.h"
#include "detail/program.h"
#include "plugin/xdp/profile.h"
#include <CL/opencl.h>

#ifdef _WIN32
# pragma warning ( disable : 4267 )
#endif

namespace xocl {

static void
validOrError(cl_program         program,
             cl_program_info    param_name,
             size_t             param_value_size,
             void *             param_value,
             size_t *           param_value_size_ret)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_VALUE if param_name is not valid, or if size in bytes
  // specified by param_value_size is < size of return type as
  // described in the table above and param_value is not NULL.

  // CL_INVALID_PROGRAM if program is not a valid program object.
  detail::program::validOrError(program);

  // CL_INVALID_PROGRAM_EXECUTABLE if param_name is
  // CL_PROGRAM_NUM_KERNELS or CL_PROGRAM_KERNEL_NAMES and a
  // successful program executable has not been built for at least one
  // device in the list of devices associated with program.

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
}

static cl_int
clGetProgramInfo(cl_program         program,
                 cl_program_info    param_name,
                 size_t             param_value_size,
                 void *             param_value,
                 size_t *           param_value_size_ret)
{
  validOrError(program,param_name,param_value_size,param_value,param_value_size_ret);

  xocl::param_buffer buffer { param_value, param_value_size, param_value_size_ret };

  switch(param_name){
    case CL_PROGRAM_REFERENCE_COUNT:
      buffer.as<cl_uint>() = xocl(program)->count();
      break;
    case CL_PROGRAM_CONTEXT:
      buffer.as<cl_context>() = xocl(program)->get_context();
      break;
    case CL_PROGRAM_NUM_DEVICES:
      buffer.as<cl_uint>() = xocl(program)->num_devices();
      break;
    case CL_PROGRAM_DEVICES:
      buffer.as<cl_device_id>() = xocl(program)->get_device_range();
      break;
    case CL_PROGRAM_SOURCE:
      buffer.as<char>() = xocl(program)->get_source();
      break;
    case CL_PROGRAM_BINARY_SIZES:
      buffer.as<size_t>() = xocl::get_range(xocl(program)->get_binary_sizes());
      break;
    case CL_PROGRAM_BINARIES:
      //param_value points to a an array of n pointers allocated by the caller, sized by
      //a prior call with CL_PROGRAM_BINARY_SIZES.  Skip device binary for entry with nullptr.
      for (auto device : xocl(program)->get_device_range()) {
        auto buf = buffer.as_array<unsigned char*>(1); // unsigned char**
        auto xclbin = xocl(program)->get_binary(device);
        auto binary_data = xclbin.binary_data();
        auto binary = binary_data.first;
        auto sz = binary_data.second - binary_data.first;
        if (buf && *buf && binary && sz) {
          // this absolutely sucks, there is no guarantee user has allocated
          // enough memory for the pointers and there is no way to check
          std::memcpy(buf[0],binary,sz);
        }
      }
      break;
    case CL_PROGRAM_NUM_KERNELS:
      buffer.as<size_t>() = xocl(program)->get_num_kernels();
      break;
    case CL_PROGRAM_KERNEL_NAMES:
      {
        std::string str;
        for (auto nm : xocl(program)->get_kernel_names())
          str.append(str.empty()?0:1,';').append(nm);
        buffer.as<char>() = str;
      }
      break;
    case CL_PROGRAM_BUFFERS_XILINX:
      //Xilinx Host Accessible Program Scope Globals vendor extension
      //external progvar
      //return semicolon separated list of host accessible program scope globals
      {
        std::string str;
        for (auto& pvar : xocl(program)->get_progvar_names()) {
            //demangle __xcl_gv prefix
            std::string demangled = pvar.substr(9);
            str.append(demangled).append(1,';');
          }
        if (!str.empty())
          str.pop_back();
        buffer.as<char>() = str;
      }
      break;
    default:
      throw xocl::error(CL_INVALID_VALUE,"clGetProgramInfo invalid param_name");
      break;
  }

  return(CL_SUCCESS);
}

} // xocl

CL_API_ENTRY cl_int CL_API_CALL
clGetProgramInfo(cl_program         program,
                 cl_program_info    param_name,
                 size_t             param_value_size,
                 void *             param_value,
                 size_t *           param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clGetProgramInfo
      (program,param_name,param_value_size,param_value,param_value_size_ret);
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
