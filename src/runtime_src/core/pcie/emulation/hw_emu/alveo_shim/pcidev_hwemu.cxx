// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidev_hwemu.h"

namespace xrt_core { namespace pci {
	
  std::shared_ptr<device> 
  pcidev_hwemu::create_device(device::handle_type handle, device::id_type id) const
  {
	  return std::shared_ptr<xrt_core::hwemu::device>(new xrt_core::hwemu::device(handle, id, !m_is_mgmt));
  }

  device::handle_type
  pcidev_hwemu::create_shim(device::id_type id) const
  {
	  return xclOpen(id, nullptr, XCL_QUIET);
  }

} } // namespace xrt_core :: pci