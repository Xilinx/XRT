/**
 * Copyright (C) 2019 Xilinx, Inc
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
#define XRT_CORE_COMMON_SOURCE
#include "core/common/system.h"

namespace xrt_core {

system* system_child_ctor(); // foward declaration

system&
instance()
{
  static system* singleton = system_child_ctor();
  return *singleton;
}

void
get_xrt_info(boost::property_tree::ptree &pt)
{
  instance().get_xrt_info(pt);
}

void
get_os_info(boost::property_tree::ptree& pt)
{
  instance().get_os_info(pt);
}

void
get_devices(boost::property_tree::ptree& pt)
{
  instance().get_devices(pt);
}

void
scan_devices(bool verbose, bool json)
{
  instance().scan_devices(verbose, json);
}

std::shared_ptr<device>
get_userpf_device(device::id_type id)
{
  return instance().get_userpf_device(id);
}

std::shared_ptr<device>
get_mgmtpf_device(device::id_type id)
{
  return instance().get_mgmtpf_device(id);
}

} // xrt_core
