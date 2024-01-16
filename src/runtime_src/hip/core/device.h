// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_device_h
#define xrthip_device_h

#include "core/common/api/handle.h"
#include "xrt/xrt_device.h"

#include <vector>

namespace xrt::core::hip {
/**
 * typedef device_handle - opaque device handle
 */
typedef uint32_t device_handle;

// forward declaration
class context;

class device
{
  xrt::device m_xrt_device;
  uint32_t m_device_id = UINT32_MAX;
  unsigned int m_flags;

public:
  explicit
  device(uint32_t _device_id);

  const xrt::device&
  get_xrt_device() const
  {
    return m_xrt_device;
  }

  uint32_t
  get_device_id() const
  {
    return m_device_id;
  }

  void
  set_flags(unsigned int flags)
  {
    m_flags = flags;
  }
};

// Global map of devices
extern xrt_core::handle_map<device_handle, std::shared_ptr<device>> device_cache;
} // xrt::core::hip

#endif
