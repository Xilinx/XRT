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

#ifndef EDGE_SYSTEM_LINUX_H
#define EDGE_SYSTEM_LINUX_H

#include "core/edge/common/system_edge.h"

namespace xrt_core {

class system_linux : public system_edge
{
public:
  void
  get_xrt_info(boost::property_tree::ptree &pt);

  void
  get_os_info(boost::property_tree::ptree &pt);

  std::pair<device::id_type, device::id_type>
  get_total_devices(bool is_user) const;

  void
  scan_devices(bool verbose, bool json) const;

  std::shared_ptr<device>
  get_userpf_device(device::id_type id) const;

  std::shared_ptr<device>
  get_userpf_device(device::handle_type hdl, device::id_type) const;

  std::shared_ptr<device>
  get_mgmtpf_device(device::id_type id) const;

  void
  program_plp(const device* dev, const std::vector<char> &buffer) const;
};

namespace edge_linux {

/**
 * get_userpf_device
 * Force singleton initialization from static linking
 * with libxrt_core.
 */
std::shared_ptr<device>
get_userpf_device(device::handle_type device_handle, device::id_type id);

} // edge_linux

} // xrt_core

#endif /* EDGE_SYSTEM_LINUX_H */
