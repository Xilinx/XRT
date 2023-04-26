// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#include "edgedev_swemu.h"

namespace xrt_core {
  namespace edge {

    std::shared_ptr<device>
    edgedev_swemu::create_device(device::handle_type handle, device::id_type id) const
    {
      return (!handle) ? std::shared_ptr<xrt_core::swemu::device>(new xrt_core::swemu::device(nullptr, id, false)) : //mgmt
                         std::shared_ptr<xrt_core::swemu::device>(new xrt_core::swemu::device(handle, id, true)); //zocl
    }

    device::handle_type
    edgedev_swemu::create_shim(device::id_type id) const
    {
      return xclOpen(id, nullptr, XCL_QUIET);
    }
  }
} // namespace xrt_core :: edge
