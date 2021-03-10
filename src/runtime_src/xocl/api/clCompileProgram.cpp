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
#include "xocl/core/program.h"
#include "xocl/core/error.h"
#include "detail/program.h"
#include "detail/device.h"
#include "api.h"
#include "plugin/xdp/profile_v2.h"
#include <CL/cl.h>

namespace xocl {

static void
validOrError(cl_program             program ,
             cl_uint                num_devices ,
             const cl_device_id *   device_list ,
             const char *           options ,
             cl_uint                num_input_headers ,
             const cl_program *     input_headers,
             const char **          header_include_names ,
             void (CL_CALLBACK *    pfn_notify)(cl_program program, void *  user_data ),
             void *                 user_data )
{
  if(!config::api_checks())
    return;

  detail::program::validOrError(program);
  detail::device::validOrError(program,num_devices,device_list);

  if (num_input_headers==0 && (header_include_names || input_headers))
    throw error(CL_INVALID_VALUE,"clCompileProgram");
  if (num_input_headers && (!header_include_names || !input_headers))
    throw error(CL_INVALID_VALUE,"clCompileProgram");

  if(!pfn_notify && user_data)
    throw error(CL_INVALID_VALUE,"clCompileProgram");

  if (xocl(program)->get_creation_type() == program::creation_type::source) {
    for (auto device : get_range(device_list,device_list+num_devices)) {
      cl_bool compiler_available = false;
      api::clGetDeviceInfo(device,CL_DEVICE_COMPILER_AVAILABLE,sizeof(cl_bool),&compiler_available,nullptr);
      if (!compiler_available)
        throw xocl::error(CL_COMPILER_NOT_AVAILABLE,"clCompileProgram");
    }
  }

  if (xocl(program)->get_num_kernels()!=0)
    throw xocl::error(CL_INVALID_OPERATION,"clCompileProgram: program already has kernels");

  if (xocl(program)->get_creation_type() != xocl::program::creation_type::source)
    throw xocl::error(CL_INVALID_OPERATION,"clCompileProgram: program not created from source");
}

static cl_int
clCompileProgram(cl_program             program ,
                 cl_uint                num_devices ,
                 const cl_device_id *   device_list ,
                 const char *           options ,
                 cl_uint                num_input_headers ,
                 const cl_program *     input_headers,
                 const char **          header_include_names ,
                 void (CL_CALLBACK *    pfn_notify)(cl_program program, void *  user_data ),
                 void *                 user_data )
{
  validOrError
    (program,num_devices,device_list
     ,options,num_input_headers,input_headers,header_include_names
     ,pfn_notify,user_data);

  return CL_SUCCESS;
}

} // xocl

cl_int
clCompileProgram(cl_program            program ,
                 cl_uint               num_devices ,
                 const cl_device_id *  device_list ,
                 const char *          options ,
                 cl_uint               num_input_headers ,
                 const cl_program *    input_headers,
                 const char **         header_include_names ,
                 void (CL_CALLBACK *   pfn_notify)(cl_program program, void *  user_data ),
                 void *                user_data )
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clCompileProgram
      (program, num_devices, device_list, options, num_input_headers, input_headers,
       header_include_names, pfn_notify, user_data);
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
