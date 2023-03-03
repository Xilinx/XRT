// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_HWQUEUE_HANDLE_H
#define XRT_CORE_HWQUEUE_HANDLE_H

#include "buffer_handle.h"
#include "fence_handle.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace xrt_core {

// class hwqueue_handle - shim base class for hardware queue
//
// Shim level implementation derives off this class to support
// hardware queue.
class hwqueue_handle
{
public:
  // Destruction must destroy the underlying hardware queue
  virtual ~hwqueue_handle()
  {}

  // Submit command for execution
  virtual void
  submit_command(buffer_handle* cmd) = 0;

  // Wait for command completion.
  //
  // @cmd        Handle to command to wait for
  // @timeout_ms Timout in ms, 0 implies infinite wait.
  // @return     0 indicates timeout, anything else indicates completion
  //
  // If cmd buffer handle is nullptr, then this function is supposed to wait
  // until any command completes execution.
  virtual int
  wait_command(buffer_handle* cmd, uint32_t timeout_ms) const = 0;

  // Enqueue a command
  //
  // @cmd      Handle to command to enqueue
  // @return   Fence handle with ownership passed to caller
  virtual std::unique_ptr<fence_handle>
  enqueue_command(buffer_handle*)
  {
    throw std::runtime_error("not supported");
  }

  // Enqueue a command along with its dependencies.
  //
  // @cmd      Handle to command to enqueue
  // @waits    List of fence handles that must be signaled prior to execution
  // @return   Fence handle with ownership passed to caller
  virtual std::unique_ptr<fence_handle>
  enqueue_command(buffer_handle*, const std::vector<fence_handle*>&)
  {
    throw std::runtime_error("not supported");
  }

  // Import a fence handle that has been exported from another process
  // or device.  The imported handle is converted into a fence_handle
  // with ownership passed to caller.  The imported fence handle can be
  // used as a dependency with enqueue.
  virtual std::unique_ptr<fence_handle>
  import(fence_handle::export_handle)
  {
    throw std::runtime_error("not supported");
  }
};

} // xrt_core
#endif
