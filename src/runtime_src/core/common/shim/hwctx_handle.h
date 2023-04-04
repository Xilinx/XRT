// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_HWCTX_HANDLE_H
#define XRT_CORE_HWCTX_HANDLE_H

#include "core/common/cuidx_type.h"
#include "core/common/error.h"
#include "core/common/shim/hwqueue_handle.h"

#include "xrt/xrt_hw_context.h"

#include <memory>

namespace xrt_core {

// class hwctx_handle - shim base class for hardware context
//
// Shim level implementation derives off this class to support
// hardware context.
class hwctx_handle
{
public:
  using qos_type = xrt::hw_context::cfg_param_type;
  using access_mode = xrt::hw_context::access_mode;
  using slot_id = uint32_t;

  // Destruction must destroy the underlying hardware context
  virtual ~hwctx_handle()
  {}

  // Update QoS of an existing hwardware context.  This is in response
  // to experimental user facing xrt::hw_context::update_qos()
  virtual void
  update_qos(const qos_type&)
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

  // Update access mode for platforms that care.  This is used
  // for Alveo mailbox where CUs are changed to be exclusive mode
  virtual void
  update_access_mode(access_mode)
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

  // The slotidx is used to encode buffer objects flags for legacy
  // shims and host applications that do not use context specific
  // xrt::bo construction.
  virtual slot_id
  get_slotidx() const = 0;

  // Create a hardware queue.  The return value is allowed to be
  // nullptr if the shim does not support hardware queues.
  virtual std::unique_ptr<hwqueue_handle>
  create_hw_queue() = 0;

  // Context specific buffer allocation
  virtual std::unique_ptr<buffer_handle>
  alloc_bo(void* userptr, size_t size, unsigned int flags) = 0;

  // Context specific buffer allocation
  virtual std::unique_ptr<buffer_handle>
  alloc_bo(size_t size, unsigned int flags) = 0;

  // Legacy XRT may require special handling when opening a context on
  // a compute unit.  Ideally, the hardware context itself should
  // manage the compute unit and XRT should not have to open and close
  // context on compute units.
  virtual cuidx_type
  open_cu_context(const std::string& cuname) = 0;

  // Legacy XRT may require special handling when closing a context on
  // a compute unit.  Ideally, the hardware context itself should
  // manage the compute unit and XRT should not have to open and close
  // context on compute units.
  virtual void
  close_cu_context(cuidx_type cuidx) = 0;

  // Execution of command objects when the shim does not support
  // hardware queues
  virtual void
  exec_buf(buffer_handle* cmd) = 0;
};

} // xrt_core
#endif
