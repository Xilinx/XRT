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

  // Poll for command completion
  //
  // @cmd    Handle to command to poll for
  // @return 0 indicates pending, anything else may indicate completion
  //
  // Note that a return value different from 0 does not guarantee that
  // the command has completed.  The command state still has to be checked
  // in order to determine completion. For shim implementations where
  // command state is live, this function does not have to be implemented.
  virtual int
  poll_command(buffer_handle*) const
  {
    return 1;
  }

  // Wait for command completion.
  //
  // @cmd        Handle to command to wait for
  // @timeout_ms Timout in ms, 0 implies infinite wait.
  // @return     0 indicates timeout, anything else indicates completion
  virtual int
  wait_command(buffer_handle* cmd, uint32_t timeout_ms) const = 0;

  // Submit wait on a fence.  The fence prevents the hardware queue from
  // proceeding until the fence is signaled.
  virtual void
  submit_wait(const fence_handle*)
  {
    throw std::runtime_error("not supported");
  }

  // Submit list of waits.  The fences prevents the hardware queue from
  // proceeding until they are all signaled.
  virtual void
  submit_wait(const std::vector<fence_handle*>&)
  {
    throw std::runtime_error("not supported");
  }

  // Submit signal on a fence.  The fence is signaled when the hardware
  // queue reaches this point.
  virtual void
  submit_signal(const fence_handle*)
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
