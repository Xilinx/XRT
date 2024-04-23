// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Device, Inc. All rights reserved.

#include "device.h"

namespace xrt::core::hip {
// Implementation
xrt_core::handle_map<device_handle, std::shared_ptr<device>> device_cache;

device::
device(uint32_t device_id)
  : m_device_id{device_id}
  , m_xrt_device{device_id}
  , m_flags{0}
{}
}
