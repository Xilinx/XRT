// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_BUFFER_HANDLE_H
#define XRT_CORE_BUFFER_HANDLE_H

#include "core/common/shim/shared_handle.h"
#include "xrt.h"

#include <cstdint>
#include <cstddef>
#include <memory>

namespace xrt_core {

class buffer_handle
{
public:
  enum class map_type { read, write };

  enum class direction
  {
    host2device = XCL_BO_SYNC_BO_TO_DEVICE,
    device2host = XCL_BO_SYNC_BO_FROM_DEVICE,
  };

  struct properties
  {
    uint32_t flags;
    uint64_t size;
    uint64_t paddr;
  };

public:
  virtual ~buffer_handle()
  {}

  // Export buffer for use with another process or device
  // An exported buffer can be imported by another device
  // or hardware context.
  virtual std::unique_ptr<shared_handle>
  share() const = 0;

  virtual void*
  map(map_type) = 0;

  virtual void
  unmap(void* addr) = 0;

  virtual void
  sync(direction, size_t size, size_t offset) = 0;

  virtual void
  copy(const buffer_handle* src, size_t size, size_t dst_offset, size_t src_offset) = 0;

  virtual properties
  get_properties() const = 0;
};

} // xrt_core

#endif
