// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// This file implements XRT xclbin APIs as declared in
// core/include/experimental/xrt_queue.h
#define XRT_API_SOURCE         // exporting xrt_hwcontext.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_xclbin.h
#define XRT_CORE_COMMON_SOURCE // in same dll as coreutil
#include "core/include/experimental/xrt_hw_context.h"
#include "hw_context_int.h"

#include "core/common/device.h"
#include "core/include/xrt_mem.h"

#include <limits>

namespace xrt {

// class hw_context_impl - insulated implemention of an xrt::hw_context
//
class hw_context_impl
{
  using qos_type = xrt::hw_context::qos_type;
  using access_mode = xrt::hw_context::access_mode;

  // Managed context handle with exceptional handling for flows
  // or applications that use load_xclbin (legacy Alveo)
  struct ctx_handle
  {
    const xrt_core::device* m_device;
    xcl_hwctx_handle m_hdl;
    bool m_destroy_context = true;

    ctx_handle(const xrt_core::device* device, const xrt::uuid& uuid, const qos_type& qos, access_mode mode)
      : m_device(device)
    {
      try {
        // Throws if (1) load_xclbin was called by application, or
        // or (2) create_hw_context is not supported. (2) => (1) so
        // in both cases simply lookup the slot into which the xclbin
        // has been loaded.
        m_hdl = m_device->create_hw_context(uuid, qos, mode);
      }
      catch (const xrt_core::ishim::not_supported_error&) {
        // xclbin must have been loaded, get the slot id for the xclbin
        // Without a hw_context, it is an error if multiple slots have
        // loaded same xclbin
        auto slots = m_device->get_slots(uuid);
        if (slots.empty())
          throw std::runtime_error("Unexpected error, xclbin has not been loaded");
        if (slots.size() > 1)
          throw std::runtime_error("Unexpected error, multi-slot xclbin requires hw_context");

        m_hdl = slots.back();
        m_destroy_context = false;
      }
    }

    ~ctx_handle()
    {
      if (m_destroy_context)
        m_device->destroy_hw_context(m_hdl);
    }
  };

  std::shared_ptr<xrt_core::device> m_core_device;
  xrt::xclbin m_xclbin;
  qos_type m_qos;
  access_mode m_mode;
  ctx_handle m_ctx_handle;

public:
  hw_context_impl(std::shared_ptr<xrt_core::device> device, const xrt::uuid& xclbin_id, const qos_type& qos)
    : m_core_device(std::move(device))
    , m_xclbin(m_core_device->get_xclbin(xclbin_id))
    , m_qos(qos)
    , m_mode(xrt::hw_context::access_mode::shared)
    , m_ctx_handle{m_core_device.get(), xclbin_id, m_qos, m_mode}
  {
  }

  hw_context_impl(std::shared_ptr<xrt_core::device> device, const xrt::uuid& xclbin_id, access_mode mode)
    : m_core_device(std::move(device))
    , m_xclbin(m_core_device->get_xclbin(xclbin_id))
    , m_mode(mode)
    , m_ctx_handle{m_core_device.get(), xclbin_id, m_qos, m_mode}
  {}

void
  set_exclusive()
  {
    m_mode = xrt::hw_context::access_mode::exclusive;
  }

  const std::shared_ptr<xrt_core::device>&
  get_core_device() const
  {
    return m_core_device;
  }

  xrt::uuid
  get_uuid() const
  {
    return m_xclbin.get_uuid();
  }

  xrt::xclbin
  get_xclbin() const
  {
    return m_xclbin;
  }

  access_mode
  get_mode() const
  {
    return m_mode;
  }

  xcl_hwctx_handle
  get_xcl_handle() const
  {
    return m_ctx_handle.m_hdl;
  }
};

} // xrt

////////////////////////////////////////////////////////////////
// xrt_hw_context implementation of extension APIs not exposed to end-user
////////////////////////////////////////////////////////////////
namespace xrt_core { namespace hw_context_int {

std::shared_ptr<xrt_core::device>
get_core_device(const xrt::hw_context& hwctx)
{
  return hwctx.get_handle()->get_core_device();
}

xrt_core::device*
get_core_device_raw(const xrt::hw_context& hwctx)
{
  return hwctx.get_handle()->get_core_device().get();
}

void
set_exclusive(xrt::hw_context& hwctx)
{
  hwctx.get_handle()->set_exclusive();
}

}} // hw_context_int, xrt_core

////////////////////////////////////////////////////////////////
// xrt_hwcontext C++ API implmentations (xrt_hw_context.h)
////////////////////////////////////////////////////////////////
namespace xrt {

hw_context::
hw_context(const xrt::device& device, const xrt::uuid& xclbin_id, const xrt::hw_context::qos_type& qos)
  : detail::pimpl<hw_context_impl>(std::make_shared<hw_context_impl>(device.get_handle(), xclbin_id, qos))
{}


hw_context::
hw_context(const xrt::device& device, const xrt::uuid& xclbin_id, access_mode mode)
  : detail::pimpl<hw_context_impl>(std::make_shared<hw_context_impl>(device.get_handle(), xclbin_id, mode))
{}

xrt::device
hw_context::
get_device() const
{
  return xrt::device{get_handle()->get_core_device()};
}

xrt::uuid
hw_context::
get_xclbin_uuid() const
{
  return get_handle()->get_uuid();
}

xrt::xclbin
hw_context::
get_xclbin() const
{
  return get_handle()->get_xclbin();
}

hw_context::access_mode
hw_context::
get_mode() const
{
  return get_handle()->get_mode();
}

hw_context::
operator xcl_hwctx_handle() const
{
  return get_handle()->get_xcl_handle();
}

} // xrt
