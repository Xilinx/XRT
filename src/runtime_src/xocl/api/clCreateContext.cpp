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

#include "xocl/core/platform.h"
#include "xocl/core/device.h"
#include "xocl/core/context.h"
#include "xocl/core/property.h"
#include "detail/platform.h"
#include "detail/device.h"
#include "detail/context.h"

#include "api.h"
#include "xoclProfile.h"

#include "xrt/util/memory.h"

namespace xocl {

static cl_platform_id
getPlatform(const cl_context_properties* properties)
{
  if (!properties) {
    cl_uint num_platforms = 0;
    cl_platform_id platform = nullptr;
    api::clGetPlatformIDs(1,&platform,&num_platforms);
    return num_platforms==0 ? nullptr : platform;
  }

  xocl::property_list<cl_context_properties> context_properties(properties);
  return context_properties.get_value_as<cl_platform_id>(CL_CONTEXT_PLATFORM);
}

static void
validOrError(const cl_context_properties * properties,
             cl_uint                       num_devices,
             const cl_device_id *          devices,
             void (CL_CALLBACK *  pfn_notify )(const char *, const void *, size_t, void *),
             void *user_data,
             cl_int *errcode_ret)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_PLATFORM if properties is NULL and no platform could
  // be selected or if platform value specified in properties is not a
  // valid platform.
  detail::platform::validOrError(getPlatform(properties));

  // CL_INVALID_PROPERTY if context property name in properties is not
  // a supported property name, if the value specified for a supported
  // property name is not valid, or if the same property name is
  // specified more than once.
  detail::context::validOrError(properties);

  // CL_INVALID_VALUE if devices is NULL; if num_devices is equal to
  // zero; or if pfn_notify is NULL but user_data is not NULL
  if (!devices)
    throw error(CL_INVALID_VALUE,"device is nullptr");
  if (user_data && !pfn_notify)
    throw error(CL_INVALID_VALUE,"user data but no callback");

  // CL_INVALID_DEVICE if devices contains an invalid device
  detail::device::validOrError(get_global_platform(),num_devices,devices);

  // CL_DEVICE_NOT_AVAILABLE if a device in devices is currently not
  // available even though the device was returned by clGetDeviceIDs
  // checked in api
}

static cl_context 
clCreateContext(const cl_context_properties * properties,
                cl_uint                       num_devices,
                const cl_device_id *          devices,
                void (CL_CALLBACK *  pfn_notify )(const char *, const void *, size_t, void *),
                void *user_data,
                cl_int *errcode_ret)
{
  validOrError(properties,num_devices,devices,pfn_notify,user_data,errcode_ret);

  // Duplicate devices are ignored
  std::vector<cl_device_id> vdevices (devices,devices+num_devices);
  std::sort(vdevices.begin(),vdevices.end());
  vdevices.erase(std::unique(vdevices.begin(),vdevices.end()),vdevices.end());

  // Ensure devices are available for current process
  for (auto device : vdevices) {
    if (!xocl(device)->lock())
      throw error(CL_DEVICE_NOT_AVAILABLE,"device unavailable");
  }

  //allocate context
  //openccl1.2-rev11.pdf P55
  //8  CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the
  //OpenCL implementation on the device.
  auto notify = (pfn_notify 
    ? [pfn_notify,user_data](const char* s) { 
        pfn_notify(s,nullptr,0,user_data);
      }
    : xocl::context::notify_action());

  auto context = xrt::make_unique<xocl::context>(properties,vdevices.size(),&vdevices[0],notify);
  xocl::assign(errcode_ret,CL_SUCCESS);
  return context.release();
}

} // xocl

cl_context
clCreateContext(const cl_context_properties * properties,
                cl_uint                       num_devices,
                const cl_device_id *          device_list,
                void (CL_CALLBACK *  pfn_notify )(const char *, const void *, size_t, void *),
                void *user_data,
                cl_int *errcode_ret)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clCreateContext
      (properties,num_devices,device_list,pfn_notify, user_data, errcode_ret);
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


