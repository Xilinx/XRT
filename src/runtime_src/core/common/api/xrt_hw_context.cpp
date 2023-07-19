// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

// This file implements XRT xclbin APIs as declared in
// core/include/experimental/xrt_queue.h
#define XRT_API_SOURCE         // exporting xrt_hwcontext.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_xclbin.h
#define XRT_CORE_COMMON_SOURCE // in same dll as coreutil
#include "core/include/xrt/xrt_hw_context.h"
#include "hw_context_int.h"

#include "core/common/device.h"
#include "core/common/shim/hwctx_handle.h"
#include <limits>
#include <memory>

namespace xdp::aie::profile {
  __declspec(dllexport) std::function<void (void*)> update_hw_context_cb;
  __declspec(dllexport) std::function<void (void*)> end_poll_cb;
}


namespace xrt {

// class hw_context_impl - insulated implemention of an xrt::hw_context
//
class hw_context_impl
{
  using cfg_param_type = xrt::hw_context::cfg_param_type;
  using qos_type = cfg_param_type;
  using access_mode = xrt::hw_context::access_mode;

  std::shared_ptr<xrt_core::device> m_core_device;
  xrt::xclbin m_xclbin;
  cfg_param_type m_cfg_param;
  access_mode m_mode;
  std::unique_ptr<xrt_core::hwctx_handle> m_hdl;

public:
  hw_context_impl(std::shared_ptr<xrt_core::device> device, const xrt::uuid& xclbin_id, const cfg_param_type& cfg_param)
    : m_core_device(std::move(device))
    , m_xclbin(m_core_device->get_xclbin(xclbin_id))
    , m_cfg_param(cfg_param)
    , m_mode(xrt::hw_context::access_mode::shared)
    , m_hdl{m_core_device->create_hw_context(xclbin_id, m_cfg_param, m_mode)}
  {
  }

  hw_context_impl(std::shared_ptr<xrt_core::device> device, const xrt::uuid& xclbin_id, access_mode mode)
    : m_core_device{std::move(device)}
    , m_xclbin{m_core_device->get_xclbin(xclbin_id)}
    , m_mode{mode}
    , m_hdl{m_core_device->create_hw_context(xclbin_id, m_cfg_param, m_mode)}
  {}

  void
  update_qos(const qos_type& qos)
  {
    m_hdl->update_qos(qos);
  }

  void
  set_exclusive()
  {
    m_mode = xrt::hw_context::access_mode::exclusive;
    m_hdl->update_access_mode(m_mode);
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

  xrt_core::hwctx_handle*
  get_hwctx_handle()
  {
    return m_hdl.get();
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
hw_context(const xrt::device& device, const xrt::uuid& xclbin_id, const xrt::hw_context::cfg_param_type& cfg_param)
  : detail::pimpl<hw_context_impl>(std::make_shared<hw_context_impl>(device.get_handle(), xclbin_id, cfg_param))
{     
  if (xdp::aie::profile::update_hw_context_cb != nullptr) 
    xdp::aie::profile::update_hw_context_cb(this);
}


hw_context::
hw_context(const xrt::device& device, const xrt::uuid& xclbin_id, access_mode mode)
  : detail::pimpl<hw_context_impl>(std::make_shared<hw_context_impl>(device.get_handle(), xclbin_id, mode))
{
  if (xdp::aie::profile::update_hw_context_cb != nullptr) 
    xdp::aie::profile::update_hw_context_cb(this);
}

hw_context::~hw_context() 
{
  std::cout << "Deleting the HW CONTEXT" << std::endl;
  // if (xdp::aie::profile::end_poll_cb != nullptr) 
  //   xdp::aie::profile::end_poll_cb(this);
}

void
hw_context::
update_qos(const qos_type& qos)
{
  get_handle()->update_qos(qos);
}

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
operator xrt_core::hwctx_handle* () const
{
  return get_handle()->get_hwctx_handle();
}

} // xrt
