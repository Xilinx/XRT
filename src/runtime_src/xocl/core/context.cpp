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

#include "context.h"
#include "device.h"
#include "platform.h"
#include <iostream>

namespace xocl {

context::
context(const cl_context_properties* properties
        ,size_t num_devices, const cl_device_id* devices
        ,const notify_action& notify)
  : m_props(properties), m_notify(notify)
{
  static unsigned int uid_count = 0;
  m_uid = uid_count++;

  XOCL_DEBUG(std::cout,"xocl::context::context(",m_uid,")\n");

  // Ensure devices are available for current process
  for (auto device : xocl::get_range(devices, devices + num_devices))
    if (!xocl(device)->lock())
      throw error(CL_DEVICE_NOT_AVAILABLE,"device unavailable");

  std::transform(devices,devices+num_devices
                 ,std::back_inserter(m_devices)
                 ,[](cl_device_id dev) {
                   return xocl::xocl(dev);
                 });
}

context::
~context()
{
  XOCL_DEBUG(std::cout,"xocl::context::~context(",m_uid,")\n");
  try {
    for (auto device : m_devices)
      device->unlock();
  }
  catch (const std::exception& ex) {
    XOCL_PRINTF("Unexpected exception in context dtor '%s'\n",ex.what());
  }
}

platform*
context::
get_platform() const
{
  return get_global_platform();
}

device*
context::
get_single_active_device() const
{
  auto device = get_device_if_one();
  return (device && device->is_active())
    ? device
    : nullptr;
}

} // xocl
