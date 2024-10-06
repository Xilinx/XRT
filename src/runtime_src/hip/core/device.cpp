// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

#include "device.h"

namespace xrt::core::hip {
// Implementation
//we should override clang-tidy warning by adding NOLINT since device_cache is non-const parameter
xrt_core::handle_map<device_handle, std::unique_ptr<device>> device_cache; //NOLINT

device::
device(uint32_t device_id)
  : m_device_id{device_id}
  , m_xrt_device{device_id}
  , m_flags{0}
{}
}
