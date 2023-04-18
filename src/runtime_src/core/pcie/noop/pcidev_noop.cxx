// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidev_noop.h"

namespace xrt_core { namespace pci {
	
  std::shared_ptr<device> 
  pcidev_noop::create_device(device::handle_type handle, device::id_type id) const
  {
    return (!handle) ? std::shared_ptr<xrt_core::noop::device>(new xrt_core::noop::device(nullptr, id, false)) :
                       std::shared_ptr<xrt_core::noop::device>(new xrt_core::noop::device(handle, id, true));
  }

  device::handle_type
  pcidev_noop::create_shim(device::id_type id) const
  {
	  return xclOpen(id, nullptr, XCL_QUIET);
  }

} } // namespace xrt_core :: pci