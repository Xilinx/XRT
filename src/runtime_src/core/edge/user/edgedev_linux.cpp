// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#include "edgedev_linux.h"

namespace xrt_core {
  namespace edge {

    std::shared_ptr<device>
    edgedev_linux::create_device(device::handle_type handle, device::id_type id) const
    {
      return (!handle) ? std::shared_ptr<xrt_core::device_linux>(new xrt_core::device_linux(nullptr, id, false)) : //mgmt
                         std::shared_ptr<xrt_core::device_linux>(new xrt_core::device_linux(handle, id, true)); //zocl
    }

    device::handle_type
    edgedev_linux::create_shim(device::id_type id) const
    {
      return xclOpen(id, nullptr, XCL_QUIET);
    }
  }
} // namespace xrt_core :: pci