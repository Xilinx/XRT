// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include <memory>

#include "aie_buffer_object.h"
#include "core/common/system.h"
#include "core/edge/user/shim.h"

namespace zynqaie {
  aie_buffer_object::aie_buffer_object(xrt_core::device* device ,const xrt::uuid uuid, const char* buffer_name, zynqaie::hwctx_object* hwctx)
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
}
