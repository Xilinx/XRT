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
#include "xocl/core/device.h"
#include "api.h"

#include "detail/program.h"
#include "detail/device.h"
#include "plugin/xdp/appdebug.h"
#include "plugin/xdp/profile.h"

#ifdef _WIN32
# pragma warning ( disable : 4996 )
#endif

namespace xocl {

static void
validOrError(cl_program            program ,
             cl_uint               num_devices ,
             const cl_device_id *  device_list ,
             const char *          options ,
             void                  (CL_CALLBACK *   pfn_notify)(cl_program program, void *  user_data ),
             void *                user_data )
{
  if (!config::api_checks())
    return;

  detail::program::validOrError(program);
  detail::device::validOrError(program,num_devices,device_list);

  // CL_INVALID_VALUE if pfn_notify is NULL but user_data is not NULL
  if(pfn_notify==NULL && user_data)
    throw xocl::error(CL_INVALID_VALUE,"pfn_notify==nullptr && user_data != nullptr");

  // CL_INVALID_BINARY if program is created with
  // clCreateProgramWithBinary and devices listed in device_list do
  // not have a valid program binary loaded
  // todo

  // CL_INVALID_BUILD_OPTIONS if the build options specified by
  // options are invalid
  // todo

  // CL_INVALID_OPERATION if the build of a program executable for any
  // of the devices listed in device_list by a previous call to
  // clBuildProgram for program has not completed.
  // todo

  // CL_COMPILER_NOT_AVAILABLE if program is created with
  // clCreateProgramWithSource and a compiler is not available
  // i.e. CL_DEVICE_COMPILER_AVAILABLE specified in the table of
  // OpenCL Device Queries for clGetDeviceInfo is set to CL_FALSE
  auto creation_type = xocl(program)->get_creation_type();
  if(creation_type==xocl::program::creation_type::source) {
    for (auto device : xocl::get_range(device_list,device_list+num_devices)) {
      cl_bool compiler_available = false;
      api::clGetDeviceInfo(device,CL_DEVICE_COMPILER_AVAILABLE,sizeof(cl_bool),&compiler_available,nullptr);
      if (!compiler_available)
        throw error(CL_COMPILER_NOT_AVAILABLE,"clBuildProgram: no compiler");
    }
  }

  // CL_BUILD_PROGRAM_FAILURE if there is a failure to build the
  // program executable. This error will be returned if clBuildProgram
  // does not return until the build has completed.
  // todo

  // CL_INVALID_OPERATION if program was not created with
  // clCreateProgramWithSource, clCreateProgramWithIL, or
  // clCreateProgramWithBinary.
  if (creation_type!=xocl::program::creation_type::source && creation_type!=xocl::program::creation_type::binary)
    throw xocl::error(CL_INVALID_OPERATION,"clBuildProgram: program not from source or binary");

  // CL_INVALID_OPERATION if the program requires independent forward
  // progress of sub-groups but one or more of the devices listed in
  // device_list does not return CL_TRUE for the
  // CL_DEVICE_SUBGROUP_INDEPENDENT_FORWARD_PROGRESS query.
  // todo

}

static cl_int
clBuildProgram(cl_program            program ,
               cl_uint               num_devices ,
               const cl_device_id *  device_list ,
               const char *          options ,
               void (CL_CALLBACK *   pfn_notify)(cl_program program, void *  user_data ),
               void *                user_data )
{
  validOrError(program,num_devices,device_list,options,pfn_notify,user_data);

  // If device_list is a NULL value, the prorgam executable is built
  // for all devices associated with program
  std::vector<xocl::device*> idevice_list;
  if(device_list == nullptr){
    for (auto d : xocl(program)->get_device_range())
      idevice_list.push_back(d);
  }
  else {
    std::transform(device_list,device_list+num_devices,std::back_inserter(idevice_list)
                   ,[](cl_device_id dev) {
                     return xocl(dev);
                   });
  }

  if (xocl(program)->get_creation_type() == xocl::program::creation_type::source) {
    if (std::getenv("XCL_CONFORMANCECOLLECT"))
      xocl(program)->build(idevice_list,options);
  }

  //async build registered callback
  //always operating in synchronous mode
  if(pfn_notify)
    pfn_notify(program,user_data);

  return CL_SUCCESS;
}

} // xocl

cl_int
clBuildProgram(cl_program program ,
    cl_uint               num_devices ,
    const cl_device_id *  device_list ,
    const char *          options ,
    void (CL_CALLBACK *   pfn_notify)(cl_program program, void *  user_data ),
    void *                user_data )
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clBuildProgram
      (program,num_devices,device_list,options,pfn_notify,user_data);
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
