/**
 * Copyright (C) 2020 Xilinx, Inc
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

#define XCL_DRIVER_DLL_EXPORT

#include "device_edge.h"

namespace xrt_core {

device_edge::
device_edge(id_type device_id, bool user)
    : device(device_id), m_userpf(user)
{
  if (m_userpf)
    m_handle = xclOpen(device_id, nullptr, XCL_QUIET);
}

device_edge::
~device_edge()
{
  if (m_userpf && m_handle)
    xclClose(m_handle);
}

xclDeviceHandle
device_edge::
get_device_handle() const
{
  return m_handle;
}

void
device_edge::
get_info(boost::property_tree::ptree& pt) const
{
}

} // xrt_core
