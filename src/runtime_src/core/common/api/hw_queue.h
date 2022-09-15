// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_COMMON_API_HW_QUEUE_H
#define XRT_COMMON_API_HW_QUEUE_H

#include "core/common/config.h"
#include "xrt/detail/pimpl.h"

#include <chrono>
#include <condition_variable>

namespace xrt {
class hw_context;
}

namespace xrt_core {

class command;
class device;

// class hw_queue - internal representation of hw queue for scheduling
//
// Constructed from within xrt::kernel
// Used for command submission and completion.
class hw_queue_impl;
class hw_queue : public xrt::detail::pimpl<hw_queue_impl>
{
public:
  // Default queue without any implementation, use for assignment
  hw_queue() = default;

  // Construct from hwctx
  hw_queue(const xrt::hw_context& hwctx);

  // Legacy construction for internal support of command execution
  // that is not tied to kernel execution, .e.g for copy_bo_with_kdma
  XRT_CORE_COMMON_EXPORT
  hw_queue(const xrt_core::device* device);

  // Start a command and manage its execution by monitoring
  // for command completion.
  void
  managed_start(xrt_core::command* cmd);

  // Start a command with explicit completion control from
  // application.
  XRT_CORE_COMMON_EXPORT
  void
  unmanaged_start(xrt_core::command* cmd);

  // Wait for command completion.  Supports both managed and unmanaged
  // commands.
  XRT_CORE_COMMON_EXPORT
  void
  wait(const xrt_core::command* cmd) const;

  // Wait for command completion with timeout.  Supports both managed
  // and unmanaged commands.
  std::cv_status
  wait(const xrt_core::command* cmd, const std::chrono::milliseconds& timeout_ms) const;

  // Wait for one call to exec_wait to return either from
  // some command completing or from a timeout.
  XRT_CORE_COMMON_EXPORT
  static std::cv_status
  exec_wait(const xrt_core::device* device, const std::chrono::milliseconds& timeout_ms);

  // Internal API to synchronize static global destruction.
  // Used by OpenCL implementation.
  XRT_CORE_COMMON_EXPORT
  static void
  stop();
};

} // namespace xrt_core

#endif
