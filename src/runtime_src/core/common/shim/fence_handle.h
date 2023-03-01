// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_FENCE_HANDLE_H
#define XRT_CORE_FENCE_HANDLE_H

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
  enum class flags { tbd };

#ifdef _WIN32
  using export_handle = void*;
#else
  using export_handle = int;
#endif

public:
  // Destruction must destroy the underlying hardware queue
  virtual ~fence_handle()
  {}

  // Export a fence for use with another process or device
  // An exported fence can be imported by a hwqueue.
  virtual export_handle
  export_fence() = 0;

  // Wait for a fence to be signaled
  // This is a blocking call
  virtual void
  wait(uint32_t timeout_ms) const = 0;
};

} // xrt_core
#endif
