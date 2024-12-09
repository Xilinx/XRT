// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_API_SOURCE         // exporting xrt_hwcontext.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_xclbin.h
#define XRT_CORE_COMMON_SOURCE // in same dll as coreutil
#include "core/include/xrt/experimental/xrt_fence.h"
#include "core/common/api/fence_int.h"
#include "core/common/shim/fence_handle.h"
#include "core/common/shim/shared_handle.h"
#include "core/common/device.h"

#include <chrono>
#include <type_traits>

namespace xrt {

// class fence_impl - implementation of fence object
//
// Mostly a wrapper around a fence_handle, but also contains
// a shared_handle for exporting the fence.
class fence_impl
{
  static_assert(std::is_same<xrt::fence::export_handle, xrt_core::shared_handle::export_handle>::value,
                "xrt::fence::export_handle must be same as xrt_core::shared_handle::export_handle");

  std::unique_ptr<xrt_core::fence_handle> m_handle;
  std::unique_ptr<xrt_core::shared_handle> m_shared_handle;
  xrt::fence::access_mode m_access = xrt::fence::access_mode::local;

public:
  fence_impl(xrt_core::device* device, fence::access_mode access)
    : m_handle(device->create_fence(access))
    , m_access(access)
  {}

  explicit fence_impl(std::unique_ptr<xrt_core::fence_handle> fhdl)
    : m_handle(std::move(fhdl))
  {}

  fence_impl(xrt_core::device* device, pid_type pid, fence::export_handle ehdl)
    : m_handle(device->import_fence(pid.pid, ehdl))
    , m_access(xrt::fence::access_mode::process)
  {}

  fence_impl(const fence_impl& other)
    : m_handle(other.m_handle->clone())
    , m_access(other.m_access)
  {}

  ~fence_impl() = default;
  
  fence_impl() = delete;
  fence_impl(fence_impl&&) = delete;
  fence_impl& operator=(const fence_impl&) = delete;
  fence_impl& operator=(fence_impl&&) = delete;

  fence::export_handle
  export_fence()
  {
    if (!m_shared_handle)
      m_shared_handle = m_handle->share();

    return m_shared_handle->get_export_handle();
  }

  std::cv_status
  wait(std::chrono::milliseconds timeout)
  {
    m_handle->wait(static_cast<uint32_t>(timeout.count()));
    return std::cv_status::no_timeout; // TBD
  }

  [[nodiscard]] xrt_core::fence_handle*
  get_fence_handle() const
  {
    return m_handle.get();
  }

  [[nodiscard]] xrt::fence::access_mode
  get_access_mode() const 
  {
    return m_access;
  }

  [[nodiscard]] uint64_t
  get_next_state() const
  {
    return m_handle->get_next_state();
  }
};

} // xrt

namespace {
}

namespace xrt_core::fence_int {

xrt_core::fence_handle*
get_fence_handle(const xrt::fence& fence)
{
  auto handle = fence.get_handle();
  return handle->get_fence_handle();
}

xrt::fence::access_mode
get_access_mode(const xrt::fence& fence)
{
  auto handle = fence.get_handle();
  return handle->get_access_mode();
}

} // xrt_core::fence_int

////////////////////////////////////////////////////////////////
// xrt_fence C++ API implmentations (xrt_fence.h)
////////////////////////////////////////////////////////////////
namespace xrt {

fence::
fence(const xrt::device& device, fence::access_mode access)
  : detail::pimpl<fence_impl>(std::make_shared<fence_impl>(device.get_handle().get(), access))
{}

fence::
fence(std::unique_ptr<xrt_core::fence_handle> fhdl)
  : detail::pimpl<fence_impl>(std::make_shared<fence_impl>(std::move(fhdl)))
{}

fence::
fence(const xrt::device& device, pid_type pid, fence::export_handle ehdl)
  : detail::pimpl<fence_impl>(std::make_shared<fence_impl>(device.get_handle().get(), pid, ehdl))
{}

fence::
fence(const fence& other)
  : detail::pimpl<fence_impl>(std::make_shared<fence_impl>(*other.get_handle()))
{}

fence::
fence(fence&& other) noexcept
  : detail::pimpl<fence_impl>(std::move(other))
{}

fence::export_handle
fence::
export_fence()
{
  return handle->export_fence();
}

std::cv_status
fence::
wait(const std::chrono::milliseconds& timeout)
{
  return handle->wait(timeout);
}

fence::access_mode
fence::
get_access_mode() const
{
  return handle->get_access_mode();
}
  
uint64_t
fence::
get_next_state() const
{
  return handle->get_fence_handle()->get_next_state();
}
  
} // namespace xrt
