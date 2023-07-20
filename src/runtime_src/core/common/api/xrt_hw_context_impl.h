// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrt_hw_context_impl_h_
#define xrt_hw_context_impl_h_

#include "core/include/xrt/xrt_hw_context.h"
#include "hw_context_int.h"

#include "core/common/device.h"
#include "core/common/shim/hwctx_handle.h"

namespace xrt {

class hw_context_impl : public std::enable_shared_from_this<hw_context_impl>
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
  std::shared_ptr<hw_context_impl> get_shared_ptr()
  {
    return shared_from_this();
  }

  public:
  hw_context_impl(std::shared_ptr<xrt_core::device> device, const xrt::uuid& xclbin_id, const cfg_param_type& cfg_param);
  hw_context_impl(std::shared_ptr<xrt_core::device> device, const xrt::uuid& xclbin_id, access_mode mode);
  ~hw_context_impl();

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

}

#endif