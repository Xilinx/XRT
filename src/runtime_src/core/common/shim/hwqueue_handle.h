// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_HWQUEUE_HANDLE_H
#define XRT_CORE_HWQUEUE_HANDLE_H

#include "xrt.h"

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
  submit_command(xrt_buffer_handle cmd) = 0;

  // Wait for command completion.
  //
  // @cmd        Handle to command to wait for
  // @timeout_ms Timout in ms, 0 implies infinite wait.
  // @return     0 indicates timeout, anything else indicates completion
  //
  // If cmd buffer handle is nullptr, then this function is supposed to wait
  // until any command completes execution.
  virtual int
  wait_command(xrt_buffer_handle cmd, uint32_t timeout_ms) const = 0;
};

} // xrt_core
#endif
