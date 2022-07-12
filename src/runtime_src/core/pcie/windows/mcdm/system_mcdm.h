// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Xilinx, Inc. All rights reserved.
#ifndef CORE_PCIE_WINDOWS_SYSTEM_MCDM_H
#define CORE_PCIE_WINDOWS_SYSTEM_MCDM_H

#include "pcie/common/system_pcie.h"

namespace xrt_core {

class system_mcdm : public system_pcie
{
public:
  std::pair<device::id_type, device::id_type>
  get_total_devices(bool is_user) const;

  std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
  get_bdf_info(device::id_type id, bool is_user) const;

  std::shared_ptr<device>
  get_userpf_device(device::id_type id) const;

  std::shared_ptr<device>
  get_userpf_device(device::handle_type device_handle, device::id_type id) const;

  std::shared_ptr<device>
  get_mgmtpf_device(device::id_type id) const;
};

} // xrt_core

#endif
