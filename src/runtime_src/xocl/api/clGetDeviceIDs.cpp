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
#include "xocl/core/device.h"
#include "detail/platform.h"
#include "detail/device.h"
#include "api.h"
#include "plugin/xdp/profile.h"
#include <CL/cl.h>

namespace {

static cl_device_type
getDeviceType(cl_device_id device)
{
  cl_device_type type = CL_DEVICE_TYPE_DEFAULT;
  xocl::api::clGetDeviceInfo(device,CL_DEVICE_TYPE,sizeof(cl_device_type),&type,nullptr);
  return type;
}

} // namespace

namespace xocl {

static void
validOrError(cl_platform_id platform,
             cl_device_type device_type,
             cl_uint        num_entries,
             cl_device_id * devices,
             cl_uint *      num_devices)
{
  if (!config::api_checks())
    return;

  detail::platform::validOrError(platform);
  detail::device::validOrError(device_type);
  detail::device::validOrError(num_entries,devices);
}

static cl_int clGetDeviceIDs(cl_platform_id platform,
                             cl_device_type device_type,
                             cl_uint        num_entries,
                             cl_device_id * devices,
                             cl_uint *      num_devices)
{
  if (!platform)
    platform = xocl::get_global_platform();
  validOrError(platform,device_type,num_entries,devices,num_devices);


  auto xplatform = xocl::xocl(platform);

  cl_uint num_devices_idx = 0;  // running count of matching devices

  //find matching devices
  if (device_type==CL_DEVICE_TYPE_DEFAULT) {
    //return first non-custom device
    for (auto checkagainst_device : xplatform->get_device_range()) {
      if (getDeviceType(checkagainst_device)!=CL_DEVICE_TYPE_CUSTOM) {
        if(num_devices_idx<num_entries && devices)
          devices[num_devices_idx]=checkagainst_device;
        ++num_devices_idx;
      }
    }
  }

  else if (device_type==CL_DEVICE_TYPE_ALL) {
    //return all non-custom devices
    for (auto checkagainst_device : xplatform->get_device_range()) {
      if(getDeviceType(checkagainst_device)!=CL_DEVICE_TYPE_CUSTOM) {
        if(num_devices_idx<num_entries && devices)
          devices[num_devices_idx]=checkagainst_device;
        ++num_devices_idx;
      }
    }
  }

  else if (device_type==CL_DEVICE_TYPE_CPU
           || device_type==CL_DEVICE_TYPE_GPU
           || device_type==CL_DEVICE_TYPE_ACCELERATOR) {
    for (auto checkagainst_device : xplatform->get_device_range()) {
      if(getDeviceType(checkagainst_device)==device_type) {
        if(num_devices_idx<=num_entries && devices)
          devices[num_devices_idx]=checkagainst_device;
        ++num_devices_idx;
      }
    }
  }

  xocl::assign(num_devices,num_devices_idx);

  if(num_devices_idx==0)
    throw xocl::error(CL_DEVICE_NOT_FOUND,"clGetDeviceIDs");

  return CL_SUCCESS;
}

} // xocl

cl_int
clGetDeviceIDs(cl_platform_id   platform,
               cl_device_type   device_type,
               cl_uint          num_entries,
               cl_device_id *   devices,
               cl_uint *        num_devices)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::
      clGetDeviceIDs(platform,device_type, num_entries, devices, num_devices);
  }
  catch (const xocl::error& ex) {
    // suppress messages if icd loader is asking for CPU or GPU
    if (ex.get_code()!=static_cast<unsigned int>(CL_DEVICE_NOT_FOUND)
        || (device_type!=CL_DEVICE_TYPE_CPU && device_type!=CL_DEVICE_TYPE_GPU))
      xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}
