// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_device_h
#define xrthip_device_h

#ifdef _WIN32
#undef max
#endif

#include "core/common/api/handle.h"
#include "xrt/xrt_device.h"

#include <limits>
#include <vector>

namespace xrt::core::hip {

// device_handle - opaque device handle
using device_handle = uint32_t;

// forward declaration
class context;

class device
{
  uint32_t m_device_id{std::numeric_limits<uint32_t>::max()};
  xrt::device m_xrt_device;
  unsigned int m_flags;
  std::weak_ptr<context> pri_ctx;

public:
  device() = default;

  explicit
  device(uint32_t device_id);

  [[nodiscard]]
  const xrt::device&
  get_xrt_device() const
  {
    return m_xrt_device;
  }

  [[nodiscard]]
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

  void
  set_pri_ctx(std::shared_ptr<context> ctx)
  {
    pri_ctx = ctx;
  }

  std::shared_ptr<context>
  get_pri_ctx()
  {
    return pri_ctx.lock(); // may return nullptr
  }
};

// Global map of devices
extern xrt_core::handle_map<device_handle, std::shared_ptr<device>> device_cache;
} // xrt::core::hip

#endif
