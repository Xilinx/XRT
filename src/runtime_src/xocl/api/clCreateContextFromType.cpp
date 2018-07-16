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
#include "plugin/xdp/profile.h"

#include "xrt/util/memory.h"

namespace xocl {

static cl_device_type
getDeviceType(cl_device_id device)
{
  cl_device_type type = CL_DEVICE_TYPE_DEFAULT;
  xocl::api::clGetDeviceInfo(device,CL_DEVICE_TYPE,sizeof(cl_device_type),&type,nullptr);
  return type;
}

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
validOrError(const cl_context_properties* properties,
             cl_device_type               device_type,
             void (CL_CALLBACK *          pfn_notify )(const char *, const void *, size_t, void *),
             void *                       user_data ,
             cl_int *                     errcode_ret )
{
  if (!config::api_checks())
    return;

  // CL_INVALID_PLATFORM if properties is NULL and no platform could
  // be selected or if platform value specified in properties is not a
  // valid platform.
  detail::platform::validOrError(getPlatform(properties));

  // CL_INVALID_PROPERTY if context property name in properties is
  // not a supported property name, or if the value specified for a
  // supported property name is not valid, or if the same property
  // name is specified more than once.
  detail::context::validOrError(properties);

  // CL_INVALID_VALUE if pfn_notify is NULL but user_data is not NULL
  if (user_data && !pfn_notify)
    throw error(CL_INVALID_VALUE,"user data but no callback");
  
  // CL_INVALID_VALUE if pfn_notify is NULL but user_data is not NULL
  detail::device::validOrError(device_type);
}

static cl_context
clCreateContextFromType(const cl_context_properties* properties,
                        cl_device_type               device_type,
                        void (CL_CALLBACK *          pfn_notify )(const char *, const void *, size_t, void *),
                        void *                       user_data ,
                        cl_int *                     errcode_ret )
{
  validOrError(properties,device_type,pfn_notify,user_data,errcode_ret);

  auto platform = getPlatform(properties);

  // collect the devices
  std::vector<cl_device_id> devices;
  for (auto device : xocl(platform)->get_device_range()) {
    auto dtype = getDeviceType(device);

    bool validdevice=
      (device_type & CL_DEVICE_TYPE_CPU & dtype) ||
      (device_type & CL_DEVICE_TYPE_GPU & dtype) ||
      (device_type & CL_DEVICE_TYPE_ACCELERATOR & dtype) ||
      (device_type & CL_DEVICE_TYPE_CUSTOM & dtype);

    //todo cl_device_type_default
    validdevice = validdevice || (device_type==CL_DEVICE_TYPE_ALL);

    if(validdevice) {
      devices.push_back(device);
      if (!device->lock())
        throw xocl::error(CL_DEVICE_NOT_AVAILABLE,"device unavailable");
    }
  }

  if (devices.empty())
    throw xocl::error(CL_DEVICE_NOT_FOUND,"No devices found");

  // allocate the context, use unique_ptr until we are done throwing or returning
  auto notify = (pfn_notify 
    ? [pfn_notify,user_data](const char* s) { 
        pfn_notify(s,nullptr,0,user_data);
      }
    : xocl::context::notify_action());

  auto context = xrt::make_unique<xocl::context>(properties,devices.size(),&devices[0],notify);

  xocl::assign(errcode_ret,CL_SUCCESS);
  return (context.release());
}

} // xocl

CL_API_ENTRY cl_context CL_API_CALL
clCreateContextFromType(const cl_context_properties *  properties,
                        cl_device_type           device_type,
                        void (CL_CALLBACK *      pfn_notify )(const char *, const void *, size_t, void *),
                        void *                   user_data ,
                        cl_int *                 errcode_ret ) CL_API_SUFFIX__VERSION_1_0
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clCreateContextFromType
      (properties,device_type,pfn_notify,user_data,errcode_ret);
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


