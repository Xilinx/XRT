// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_BUFFER_HANDLE_H
#define XRT_CORE_BUFFER_HANDLE_H

#include "core/common/shim/shared_handle.h"
#include "xrt.h"
#include "xrt/xrt_bo.h"
#include "core/common/error.h"

#include <cstdint>
#include <cstddef>
#include <map>
#include <memory>

namespace xrt_core {
class hwctx_handle; // forward declaration

// class buffer_handle - shim base class for buffer objects
//
// Shim level implementation derive off this class to support
// opaque buffer objects where implementation details are
// platform specific
class buffer_handle
{
public:

  using bo_direction = xclBOSyncDirection;
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
    uint64_t flags;  // flags of bo
    uint64_t size;   // size of bo
    uint64_t paddr;  // physical address
    uint64_t kmhdl;  // kernel mode handle
  };

public:
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

  // Indicates to SHIM/driver that bh is going to be used by this
  // BO. With offset and size, it can also support using portion of bh
  // (sub-BO).  For now, this is only used when set_arg() is called
  // upon an exec buf BO where pos is the arg index.
  virtual void
  bind_at(size_t /*pos*/, const buffer_handle* /*bh*/, size_t /*offset*/, size_t /*size*/)
  {}

  // Reverse of bind_at, this function indiciates to shim that the execbo
  // (on which the function is called) is no longer using the BOs that were
  // bound to it.
  virtual void
  reset()
  {}

  virtual void
  sync_aie_bo(xrt::bo&, const char *, bo_direction, size_t, size_t)
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

  virtual void
  sync_aie_bo_nb(xrt::bo&, const char *, bo_direction, size_t, size_t)
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

  // Configures the buffer to be used as debug/dtrace/log bo using
  // the flag that is used to create this bo. This call creates metadata
  // using map of column/uc index and buffer sizes and passes the info to driver.
  virtual void
  config(xrt_core::hwctx_handle* /*ctx*/, const std::map<uint32_t, size_t>& /*buf_sizes*/)
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

  // Unconfigure the buffer configured earlier
  // If this call is not made explicitly the derived buffer_handle class
  // destoryer should handle the unconfiguring part.
  virtual void
  unconfig(xrt_core::hwctx_handle*)
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }
};

} // xrt_core

#endif
