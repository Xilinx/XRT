// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2026 Advanced Micro Devices, Inc. All rights reserved.

#include "device.h"

namespace xrt::core::hip {

device::
device(uint32_t device_id)
  : m_device_id{device_id}
  , m_xrt_device{device_id}
  , m_flags{0}
{}
}
