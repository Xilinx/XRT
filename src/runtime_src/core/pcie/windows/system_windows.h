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

#ifndef SYSTEM_WINDOWS_H
#define SYSTEM_WINDOWS_H

#include "pcie/common/system_pcie.h"

namespace xrt_core {

class system_windows : public system_pcie
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
  get_userpf_device(device::handle_type device_handle, device::id_type id) const;

  std::shared_ptr<device>
  get_mgmtpf_device(device::id_type id) const;

  void
  program_plp(std::shared_ptr<device> dev, const std::vector<char> &buffer) const;
};

} // host,xrt_core

#endif
