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
#include "xocl/core/range.h"
#include "xocl/core/error.h"

#include "detail/context.h"
#include "detail/program.h"
#include "detail/device.h"

#include "api.h"
#include "plugin/xdp/profile.h"

namespace xocl {

static void
validOrError(cl_context            context ,
             cl_uint               num_devices ,
             const cl_device_id *  device_list ,
             const char *          options ,
             cl_uint               num_input_programs ,
             const cl_program*     input_programs ,
             void (CL_CALLBACK *   pfn_notify )(cl_program program , void *  user_data ),
             void *                user_data ,
             cl_int *              errcode_ret)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_CONTEXT if context is not a valid context.
  detail::context::validOrError(context);

  // CL_INVALID_VALUE if device_list is NULL and num_devices is
  // greater than zero, or if device_list is not NULL and num_devices
  // is zero.
  //
  // CL_INVALID_DEVICE if OpenCL devices listed in device_list are not
  // in the list of devices associated with context.
  detail::device::validOrError(context,num_devices,device_list);

  // CL_INVALID_VALUE if num_input_programs is zero and input_programs
  // is NULL or if num_input_programs is zero and input_programs is
  // not NULL or if num_input_programs is not zero and input_programs
  // is NULL.
  if (!num_input_programs || !input_programs)
    throw error(CL_INVALID_VALUE,"num_input_programs==0 or input_programs==nullptr");

  // CL_INVALID_PROGRAM if programs specified in input_programs are
  // not valid program objects.
  std::for_each(input_programs,input_programs+num_input_programs,
                [](cl_program p){detail::program::validOrError(p);});

  // CL_INVALID_VALUE if pfn_notify is NULL but user_data is not NULL.
  if (user_data && !pfn_notify)
    throw error(CL_INVALID_VALUE,"user data but no callback");

  // CL_INVALID_LINKER_OPTIONS if the linker options specified by
  // options are invalid

  // CL_INVALID_OPERATION if the compilation or build of a program
  // executable for any of the devices listed in device_list by a
  // previous call to clCompileProgram or clBuildProgram for program
  // has not completed.

  // CL_INVALID_OPERATION if the rules for devices containing compiled
  // binaries or libraries as described in input_programs argument
  // above are not followed.

  // CL_INVALID_OPERATION if the one or more of the programs specified
  // in input_programs requires independent forward progress of
  // sub-groups but one or more of the devices listed in device_list
  // does not return CL_TRUE for the
  // CL_DEVICE_SUBGROUP_INDEPENDENT_FORWARD_PROGRESS query.

  // CL_LINKER_NOT_AVAILABLE if a linker is not available
  // i.e. CL_DEVICE_LINKER_AVAILABLE specified in the table of allowed
  // values for param_name for clGetDeviceInfo is set to CL_FALSE.
  std::for_each(device_list,device_list+num_devices,[](cl_device_id device) {
      cl_bool val = false;
      api::clGetDeviceInfo(device,CL_DEVICE_LINKER_AVAILABLE,sizeof(cl_bool),&val,nullptr);
      if (!val)
        throw error(CL_LINKER_NOT_AVAILABLE,"linker not available for device");
    });
  
  // CL_LINK_PROGRAM_FAILURE if there is a failure to link the
  // compiled binaries and/or libraries.

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
} 

static cl_program 
clLinkProgram(cl_context            context ,
              cl_uint               num_devices ,
              const cl_device_id *  device_list ,
              const char *          options ,
              cl_uint               num_input_programs ,
              const cl_program*     input_programs ,
              void (CL_CALLBACK *   pfn_notify )(cl_program program , void *  user_data ),
              void *                user_data ,
              cl_int *              errcode_ret  )
{
  validOrError(context,num_devices,device_list,options,
               num_input_programs,input_programs,
               pfn_notify,user_data,errcode_ret);

  xocl::assign(errcode_ret,CL_SUCCESS);
  return nullptr;
}

} // xocl

cl_program
clLinkProgram(cl_context            context ,
              cl_uint               num_devices ,
              const cl_device_id *  device_list ,
              const char *          options ,
              cl_uint               num_input_programs ,
              const cl_program*     input_programs ,
              void (CL_CALLBACK *   pfn_notify )(cl_program program , void *  user_data ),
              void *                user_data ,
              cl_int *              errcode_ret)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clLinkProgram
      (context, num_devices, device_list,options,num_input_programs,
       input_programs,pfn_notify,user_data,errcode_ret);
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


