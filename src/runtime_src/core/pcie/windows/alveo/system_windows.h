// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef SYSTEM_WINDOWS_ALVEO_H
#define SYSTEM_WINDOWS_ALVEO_H

#include "pcie/common/system_pcie.h"

namespace xrt_core {

class system_windows : public system_pcie
{
public:
  void
  get_driver_info(boost::property_tree::ptree &pt);

  std::pair<device::id_type, device::id_type>
  get_total_devices(bool is_user) const;

  std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
  get_bdf_info(device::id_type id, bool is_user) const;

  void
  scan_devices(bool verbose, bool json) const;

  std::shared_ptr<device>
  get_userpf_device(device::id_type id) const;

  std::shared_ptr<device>
  get_userpf_device(device::handle_type device_handle, device::id_type id) const;

  std::shared_ptr<device>
  get_mgmtpf_device(device::id_type id) const;

  void
  program_plp(const device* dev, const std::vector<char> &buffer, bool force) const;
};

} // host,xrt_core

#endif
