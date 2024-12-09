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

// (c) Copyright 2017 Xilinx, Inc. All rights reserved.
#include "xocl/config.h"
#include "xocl/core/range.h"
#include "xocl/core/error.h"
#include "xocl/core/device.h"
#include "xocl/core/program.h"
#include "xocl/core/kernel.h"
#include "xocl/core/context.h"

#include "detail/context.h"
#include "detail/device.h"

#include "core/include/xrt/detail/xclbin.h"
#include "core/include/xrt/experimental/xclbin_util.h"

#include <exception>
#include <string>
#include <algorithm>

#include "plugin/xdp/profile_v2.h"

#include <CL/opencl.h>

#ifdef _WIN32
# pragma warning ( disable : 4996 )
#endif

namespace {

static void
loadProgramBinary(xocl::program* program, xocl::device* device)
{
  device->load_program(program);
}

} //namespace

namespace xocl {

static void
validOrError(cl_context                      context ,
             cl_uint                         num_devices ,
             const cl_device_id *            device_list ,
             const size_t *                  lengths ,
             const unsigned char **          binaries ,
             cl_int *                        binary_status ,
             cl_int *                        errcode_ret )
{
  if (!config::api_checks())
    return;

  // CL_INVALID_CONTEXT if context is not a valid context.
  detail::context::validOrError(context);

  // CL_INVALID_VALUE if device_list is NULL or num_devices is zero.
  // CL_INVALID_DEVICE if OpenCL devices listed in device_list are not
  // in the list of devices associated with context.
  detail::device::validOrError(context,num_devices,device_list);

  // CL_INVALID_VALUE if lengths or binaries are NULL or if any entry
  // in lengths[i] or binaries[i] is NULL.
  if ((lengths==nullptr) || (binaries==nullptr))
    throw xocl::error(CL_INVALID_VALUE,"CL_INVALID_VALUE lengths or binaries are nullptr");
  if (std::any_of(lengths,lengths+num_devices,[](size_t sz){return sz==0;}))
    throw xocl::error(CL_INVALID_VALUE,"CL_INVALID_VALUE an entry in lengths is zero");
  if (std::any_of(binaries,binaries+num_devices,[](const unsigned char* b){return b==nullptr;}))
    throw xocl::error(CL_INVALID_VALUE,"CL_INVALID_VALUE an entry in binaries is nullptr");

  // CL_INVALID_BINARY if an invalid program binary was encountered
  // for any device. binary_status will return specific status for
  // each device.


  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.
  // xlnx: if a device is already programmed with different xclbin
  // then it's unavailable

  // If any one device is already programmed, then all should be
  // programmed and the binaries must match the ones passed here.
  // Or none of current devices are programmed.
  auto program = xocl(device_list[0])->get_program();
  for (unsigned int idx = 0; idx < num_devices; ++idx) {
    auto device = xocl(device_list[idx]);
    
    if (idx > 0 && device->get_program() != program)
      throw xocl::error(CL_INVALID_VALUE,"Device '" + device->get_bdf() + "' is already programmed");

    if (!program)
      continue;
    
    // Compare program uuid against this binary, must match
    auto uuid = program->get_xclbin_uuid(device);
    auto binary = binaries[idx];   // guaranteed not nullptr 
    xuid_t xuuid;
    xclbin_uuid(binary, xuuid);

    if (uuid != xuuid)
      throw xocl::error(CL_OUT_OF_RESOURCES,
                        "device '" + device->get_bdf() +"' programmed with different xclbin");
  }

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.

  // Xilinx restriction
  // Only one binary per device
  std::vector<cl_device_id> devices(device_list,device_list+num_devices);
  std::sort(devices.begin(),devices.end());
  auto itr = std::adjacent_find(devices.begin(),devices.end());
  if (itr!=devices.end())
    throw xocl::error(CL_INVALID_VALUE,"Xilinx restriction more than one binary per device");
}

static cl_program
clCreateProgramWithBinary(cl_context                      context ,
                          cl_uint                         num_devices ,
                          const cl_device_id *            device_list ,
                          const size_t *                  lengths ,
                          const unsigned char **          binaries ,
                          cl_int *                        binary_status,
                          cl_int *                        errcode_ret )
{
  validOrError(context,num_devices,device_list,lengths,binaries,binary_status,errcode_ret);

  // validOrError guarantees that if any one device is already programmed
  // then all are programmed and with the same program object. Further
  // every device is guaranteed to be programmed with the binary passed
  // to this function.  Alas, just return the program.
  if (auto program = xocl(device_list[0])->get_program()) {
    program->retain();
    xocl::assign(errcode_ret,CL_SUCCESS);
    return program;
  }

  // Initialize binary_status
  if (binary_status)
    std::fill(binary_status,binary_status+num_devices,CL_INVALID_VALUE);

  // Construct program object
  auto program = std::make_unique<xocl::program>(xocl::xocl(context),num_devices,device_list,binaries,lengths);

  // Assign binaries to all devices in the list
  size_t idx = 0;
  for (auto device : xocl::get_range(device_list,device_list+num_devices)) {
    try {
      if (xocl(device)->is_active())
        xocl::profile::flush_device(xocl(device)->get_xdevice()) ;
      loadProgramBinary(program.get(),xocl(device));
      xocl::profile::update_device(xocl(device)->get_xdevice()) ;
      if (binary_status)
        xocl::assign(&binary_status[idx++],CL_SUCCESS);
    }
    catch (const xocl::error&) {
      if (binary_status)
        xocl::assign(&binary_status[idx],CL_INVALID_BINARY);
      throw;
    }
  }

  xocl::assign(errcode_ret,CL_SUCCESS);

  return program.release();
}

namespace api {

cl_program
clCreateProgramWithBinary(cl_context                      context ,
                          cl_uint                         num_devices ,
                          const cl_device_id *            device_list ,
                          const size_t *                  lengths ,
                          const unsigned char **          binaries ,
                          cl_int *                        binary_status ,
                          cl_int *                        errcode_ret )
{
  return xocl::clCreateProgramWithBinary
    (context,num_devices,device_list,lengths,binaries,binary_status,errcode_ret);
}

} // api

} // xocl

cl_program
clCreateProgramWithBinary(cl_context                      context ,
                          cl_uint                         num_devices ,
                          const cl_device_id *            device_list ,
                          const size_t *                  lengths ,
                          const unsigned char **          binaries ,
                          cl_int *                        binary_status ,
                          cl_int *                        errcode_ret )
{

  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clCreateProgramWithBinary
      (context,num_devices,device_list,lengths,binaries,binary_status,errcode_ret);
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
