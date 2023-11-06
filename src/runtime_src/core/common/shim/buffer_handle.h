// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_BUFFER_HANDLE_H
#define XRT_CORE_BUFFER_HANDLE_H

#include "core/common/shim/shared_handle.h"
#include "core/common/usage_metrics.h"
#include "xrt.h"

#include <cstdint>
#include <cstddef>
#include <memory>

namespace xrt_core {

// class buffer_handle - shim base class for buffer objects
//
// Shim level implementation derive off this class to support
// opaque buffer objects where implementation details are
// platform specific
class buffer_handle
{
public:
  // map_type - determines how a buffer is mapped
  enum class map_type { read, write };

  // direction - direction of sync operation
  enum class direction
  {
    host2device = XCL_BO_SYNC_BO_TO_DEVICE,
    device2host = XCL_BO_SYNC_BO_FROM_DEVICE,
  };

  // properties - buffer details
  struct properties
  {
    uint64_t flags;
    uint64_t size;
    uint64_t paddr;
  };

public:
  XCL_DRIVER_DLLESPEC
  buffer_handle();

  virtual ~buffer_handle()
  {}

  // Export buffer for use with another process or device
  // An exported buffer can be imported by another device
  // or hardware context.
  virtual std::unique_ptr<shared_handle>
  share() const = 0;

  // Map a buffer for read or write. Subject to be replaced
  // by scoped embedded object.
  virtual void*
  map(map_type) = 0;

  // Unmap a previously mapped buffer, potentially replace
  // by scoped embedded object destructor
  virtual void
  unmap(void* addr) = 0;

  // Sync a buffer to or from device
  virtual void
  sync(direction, size_t size, size_t offset) = 0;

  // Copy size bytes from src buffer at src offset into this
  // buffer at dst offset
  virtual void
  copy(const buffer_handle* src, size_t size, size_t dst_offset, size_t src_offset) = 0;

  // Get properties of this buffer
  virtual properties
  get_properties() const = 0;

  // While keeping xcl APIs alive
  virtual xclBufferHandle
  get_xcl_handle() const
  {
    return XRT_NULL_BO;
  }

  std::shared_ptr<usage_metrics::base_logger>
  get_usage_logger()
  {
    return m_usage_logger;
  }

private:
  std::shared_ptr<usage_metrics::base_logger> m_usage_logger; 
};

} // xrt_core

#endif
