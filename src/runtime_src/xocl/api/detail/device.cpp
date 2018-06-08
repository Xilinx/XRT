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

#include "device.h"
#include "xocl/core/device.h"
#include "xocl/core/context.h"
#include "xocl/core/program.h"
#include "xocl/core/platform.h"
#include "xocl/core/error.h"

namespace xocl { namespace detail {

namespace device {

void 
validOrError(cl_device_id device)
{
  if (!device)
    throw error(CL_INVALID_DEVICE,"device is nullptr");
}

void
validOrError(const cl_device_type device_type)
{
  auto valid = CL_DEVICE_TYPE_ALL | CL_DEVICE_TYPE_DEFAULT
    | CL_DEVICE_TYPE_CPU| CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_ACCELERATOR
    | CL_DEVICE_TYPE_CUSTOM;
  if (! (device_type & valid) ) 
    throw error(CL_INVALID_DEVICE_TYPE);
}

void
validOrError(const cl_program program, cl_device_id device)
{
  if (!xocl(program)->has_device(xocl(device)))
    throw error(CL_INVALID_DEVICE,"device not in program");
}

void
validOrError(cl_uint num_devices, const cl_device_id* device_list)
{
  if (!num_devices && !device_list)
    return ;

  if (!num_devices && device_list)
    throw error(CL_INVALID_VALUE,"number of devices is 0");
  if (num_devices && !device_list)
    throw error(CL_INVALID_VALUE,"device_list is nullptr");
}

void
validOrError(cl_program program, cl_uint num_devices, const cl_device_id* device_list)
{
  validOrError(num_devices,device_list);
  for (auto device : get_range(device_list,device_list+num_devices)) {
    if (!device)
      throw error(CL_INVALID_DEVICE,"device is nullptr");
    if (!xocl(program)->has_device(xocl(device)))
      throw error(CL_INVALID_DEVICE,"device not in program");
  }
}

void
validOrError(cl_context context, cl_uint num_devices, const cl_device_id* device_list)
{
  validOrError(num_devices,device_list);
  for (auto device : get_range(device_list,device_list+num_devices)) {
    if (!device)
      throw error(CL_INVALID_DEVICE,"device is nullptr");
    if (!xocl(context)->has_device(xocl(device)))
      throw error(CL_INVALID_DEVICE,"device not in context");
  }
}

void
validOrError(cl_platform_id platform, cl_uint num_devices, const cl_device_id* device_list)
{
  validOrError(num_devices,device_list);
  for (auto device : get_range(device_list,device_list+num_devices)) {
    if (!device)
      throw error(CL_INVALID_DEVICE,"device is nullptr");

    // Skip platform check for subdevices.
    if (xocl(device)->is_sub_device())
      continue;
    
    if (platform && !xocl(platform)->has_device(xocl(device)))
      throw error(CL_INVALID_DEVICE,"device not in platform");
  }
}

} // device

}} // detail,xocl



