// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include <memory>

#include "aie_buffer_object.h"
#include "core/common/system.h"
#include "core/edge/user/shim.h"

namespace zynqaie {
  using buffer_state = xrt::aie::device::buffer_state;
  aie_buffer_object::aie_buffer_object(xrt_core::device* device, const xrt::uuid uuid, const char* buffer_name, zynqaie::hwctx_object* hwctx)
    : name{buffer_name}
  {
    if (hwctx) {
      m_aie_array = hwctx->get_aie_array_shared();
    }
    else {
      auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());
      if (drv->isAieRegistered())
        m_aie_array = drv->get_aie_array_shared();
    }

    if (!m_aie_array)
      throw xrt_core::error(-EINVAL, "Aie Array is not registered" );

    auto found_gmio = m_aie_array->find_gmio(name);
    auto found_ebuf = m_aie_array->find_external_buffer(name);

    if (!found_gmio && !found_ebuf)
      throw xrt_core::error(-EINVAL, "GMIO/External buffer is not present with name " + name );

    if (found_gmio && found_ebuf)
      throw xrt_core::error(-EINVAL, "Ambiguous port name '" + name + "'.Both GMIO and External Buffer exists with this name");
  }

  std::string
  aie_buffer_object::get_name() const
  {
    return name;
  }

  void
  aie_buffer_object::sync(std::vector<xrt::bo>& bos, xclBOSyncDirection dir, size_t size, size_t offset) const
  {
    return m_aie_array->sync_bo(bos, name.c_str(), dir, size, offset);
  }

  void
  aie_buffer_object::async(std::vector<xrt::bo>& bos, xclBOSyncDirection dir, size_t size, size_t offset)
  {
    std::lock_guard<std::mutex> lock(mtx);
    if (m_state == buffer_state::running)
      throw xrt_core::error(-EINVAL, "Asynchronous operation is already initiated. Multiple 'async' calls are not supported");

    bd_info = m_aie_array->sync_bo_nb(bos, name.c_str(), dir, size, offset);
    m_state = buffer_state::running;
  }

  buffer_state
  aie_buffer_object::async_status()
  {
    if (m_state != buffer_state::running)
      throw xrt_core::error(-EINVAL, "Asynchronous operation is not initiated.");

    if (m_state != buffer_state::completed && m_aie_array->async_status(name, bd_info.first, bd_info.second))
      m_state = buffer_state::completed;

    return m_state;
  }

  void
  aie_buffer_object::wait()
  {
    std::lock_guard<std::mutex> lock(mtx);
    if (m_state != buffer_state::running)
      throw xrt_core::error(-EINVAL, "Asynchronous operation is not initiated. Please call 'wait' after 'async' call");

    m_aie_array->wait_gmio(name);
    m_state = buffer_state::completed;
  }
}
