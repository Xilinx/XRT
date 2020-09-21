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

#ifndef PCIE_SYSTEM_SWEMU_LINUX_H
#define PCIE_SYSTEM_SWEMU_LINUX_H

#include "pcie/common/system_pcie.h"

namespace xrt_core { namespace swemu {

class system : public system_pcie
{
public:
  system();
  
  std::pair<device::id_type, device::id_type>
  get_total_devices(bool is_user) const;

  std::shared_ptr<xrt_core::device>
  get_userpf_device(device::id_type id) const;

  std::shared_ptr<xrt_core::device>
  get_userpf_device(device::handle_type device_handle, device::id_type id) const;

  std::shared_ptr<xrt_core::device>
  get_mgmtpf_device(device::id_type id) const;

  void
  program_plp(std::shared_ptr<device> dev, std::vector<char> buffer) const;
};

/**
 * get_userpf_device
 * Force singleton initialization from static linking
 * with libxrt_core.
 */ 
std::shared_ptr<device>
get_userpf_device(device::handle_type device_handle, device::id_type id);

}} // swemu, xrtcore

#endif
