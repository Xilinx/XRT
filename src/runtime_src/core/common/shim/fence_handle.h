// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_FENCE_HANDLE_H
#define XRT_CORE_FENCE_HANDLE_H

#include "xrt/experimental/xrt_fence.h"

#include "core/common/shim/shared_handle.h"
#include "core/common/error.h"

#include <cstdint>
#include <memory>

namespace xrt_core {

// class fence_handle - shim base class for fence
//
// Shim level implementation derives off this class to support
// fence synchronization objects
//
// A fence is associated with a command submission to a hw queue.  It
// is signaled upon command completion and remains signaled until the
// fence is deleted.
class fence_handle
{
  // Flags that indicate what kind of fence to create
public:
  using export_handle = shared_handle::export_handle;
  using access_mode = xrt::fence::access_mode;

public:
  // Destruction must destroy the underlying hardware queue
  virtual ~fence_handle()
  {}

  // Make a deep copy of a fence handle.   There is no shared
  // state between the original and the clone.
  virtual std::unique_ptr<fence_handle>
  clone() const = 0;

  // Export a fence for use with another process or device
  // An exported fence can be imported by a hwqueue.
  virtual std::unique_ptr<shared_handle>
  share() const = 0;

  // Wait for a fence to be signaled
  // This is a blocking call
  virtual void
  wait(uint32_t timeout_ms) const = 0;

  // Signal a fence from host side
  virtual void
  signal() const
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

  // Return the next state of the fence.  The next state is the value
  // that will be used when the fence is signaled or waited on.  The
  // next state is incremented when the fence is signaled or waited on.
  // Implementation detail to facilitate debugging.
  virtual uint64_t
  get_next_state() const = 0;
};

} // xrt_core
#endif
