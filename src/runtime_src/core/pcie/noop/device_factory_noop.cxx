// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include "device_factory_noop.h"

namespace xrt_core::noop {

std::shared_ptr<xrt_core::device>
device_factory_noop::
create_device(device::handle_type handle, device::id_type id) const
{
  return (!handle) ? std::shared_ptr<xrt_core::noop::device>(new xrt_core::noop::device(nullptr, id, false)) :
    std::shared_ptr<xrt_core::noop::device>(new xrt_core::noop::device(handle, id, true));
}

device::handle_type
device_factory_noop::
create_shim(device::id_type id) const
{
  return xclOpen(id, nullptr, XCL_QUIET);
}

} // xrt_core::noop