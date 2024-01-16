// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-204 Advanced Micro Device, Inc. All rights reserved.

#include "device.h"

namespace xrt::core::hip {
// Implementation
xrt_core::handle_map<device_handle, std::shared_ptr<device>> device_cache;

device::
device(uint32_t _device_id)
{
  m_xrt_device = xrt::device{_device_id};
  m_device_id = _device_id;
}
}
