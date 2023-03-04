// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#include "xrt/detail/pimpl.h"
#include "core/common/shim/fence_handle.h"

namespace xrt_core {

// class fence - internal representation of a managed fence_handle
class fence : public xrt::detail::pimpl<fence_handle>
{
public:
  // Default empty fence
  fence() = default;

  // Construct from fence_handle.  The ownership is transferred
  // and managed by this fence object.
  fence(std::unique_ptr<fence_handle> hdl)
    : pimpl(std::move(hdl))
  {}

  // Wait for the fence to be signaled.  Once signaled, a fence
  // remains signaled until it is deleted.
  void
  wait(uint32_t timeout_ms) const
  {
    get_handle()->wait(timeout_ms);
  }

  // Get the underlying fence handle as a cast.
  // This simplies conversion of container of fence to
  // container of underlying handles.
  operator fence_handle* () const
  {
    return get_handle().get();
  }
};

}
