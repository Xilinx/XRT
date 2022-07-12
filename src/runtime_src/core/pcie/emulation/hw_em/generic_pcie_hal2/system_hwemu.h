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

#ifndef PCIE_SYSTEM_HWEMU_LINUX_H
#define PCIE_SYSTEM_HWEMU_LINUX_H

#include "pcie/common/system_pcie.h"

namespace xrt_core { namespace hwemu {

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
  program_plp(const device* dev, const std::vector<char> &buffer) const;

  void
  mem_read(const device* dev, long long addr, long long size, const std::string& output_file) const;

  void
  mem_write(const device* device, long long addr, long long size, unsigned int pattern) const;
};

/**
 * get_userpf_device
 * Force singleton initialization from static linking
 * with libxrt_core.
 */
std::shared_ptr<device>
get_userpf_device(device::handle_type device_handle, device::id_type id);

}} // hwemu, xrtcore

#endif
