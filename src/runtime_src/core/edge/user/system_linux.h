/**
 * Copyright (C) 2020 Xilinx, Inc
 * Copyright (C) 2022-2025 Advanced Micro Devices, Inc. - All rights reserved
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


namespace xrt_core::edge {

class dev;
class drv;

}

namespace xrt_core {

class system_linux : public system_edge
{
public:
  system_linux();

  void
  get_driver_info(boost::property_tree::ptree &pt);

  std::pair<device::id_type, device::id_type>
  get_total_devices(bool is_user) const;

  std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
  get_bdf_info(device::id_type id, bool is_user) const override;

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

  std::shared_ptr<edge::dev>
  get_edge_dev(unsigned index) const;

private:
  std::vector<std::shared_ptr<edge::dev>> dev_list;
};

namespace edge_linux {

/**
 * get_userpf_device
 * Force singleton initialization from static linking
 * with libxrt_core.
 */
std::shared_ptr<device>
get_userpf_device(device::handle_type device_handle, device::id_type id);

void
register_driver(std::shared_ptr<xrt_core::edge::drv> driver);

std::shared_ptr<edge::dev>
get_dev(unsigned index);

} // edge_linux

} // xrt_core

#endif /* EDGE_SYSTEM_LINUX_H */
