// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#include "hwctx_object.h"
#include "core/edge/user/aie/profile_object.h"
#ifdef XRT_ENABLE_AIE
#include "core/edge/user/aie/graph_object.h"
#include "core/edge/user/aie/aie_buffer_object.h"
#include "core/edge/user/aie/aie.h"
#endif
#include "core/edge/user/shim.h"

namespace zynqaie {
  // Shim handle for hardware context Even as hw_emu does not
  // support hardware context, it still must implement a shim
  // hardware context handle representing the default slot
  //
  //
  hwctx_object::hwctx_object(ZYNQ::shim* shim, slot_id slotidx, xrt::uuid uuid, xrt::hw_context::access_mode mode)
	  : m_shim(shim)
	  , m_uuid(std::move(uuid))
	  , m_slotidx(slotidx)
	  , m_mode(mode)
  {}

  hwctx_object::~hwctx_object()
  {
    try {
      m_shim->destroy_hw_context(m_slotidx);
    }
    catch (const std::exception& ex) {
      xrt_core::send_exception_message(ex.what());
    }
  }

#ifdef XRT_ENABLE_AIE
  std::shared_ptr<Aie>
  hwctx_object::get_aie_array_shared()
  {
    return m_aie_array;
  }
    
  aied*
  hwctx_object::getAied()
  {
    return m_aied.get();
  }
#endif
  
  void 
  hwctx_object::initAie()
  {
#ifdef XRT_ENABLE_AIE
    auto device{xrt_core::get_userpf_device(m_shim)};
    auto data = device->get_axlf_section(AIE_METADATA, m_uuid);
    if (data.first && data.second)
      m_aie_array = std::make_shared<Aie>(device, this);

    m_aied = std::make_unique<zynqaie::aied>(device.get());
#endif
  }

  std::unique_ptr<xrt_core::buffer_handle>
  hwctx_object::alloc_bo(void* userptr, size_t size, uint64_t flags)
  {
    // The hwctx is embedded in the flags, use regular shim path
    return m_shim->xclAllocUserPtrBO(userptr, size, xcl_bo_flags{flags}.flags, this);
  }

  std::unique_ptr<xrt_core::buffer_handle>
  hwctx_object::alloc_bo(size_t size, uint64_t flags)
  {
    // The hwctx is embedded in the flags, use regular shim path
    return m_shim->xclAllocBO(size, xcl_bo_flags{flags}.flags, this);
  }

  xrt_core::cuidx_type
  hwctx_object::open_cu_context(const std::string& cuname)
  {
    return m_shim->open_cu_context(this, cuname);
  }

  void
  hwctx_object::close_cu_context(xrt_core::cuidx_type cuidx)
  {
    m_shim->close_cu_context(this, cuidx);
  }

  void
  hwctx_object::exec_buf(xrt_core::buffer_handle* cmd)
  {
    m_shim->hwctx_exec_buf(this, cmd->get_xcl_handle());
  }

  std::unique_ptr<xrt_core::graph_handle>
  hwctx_object::open_graph_handle(const char* name, xrt::graph::access_mode am)
  {
#ifdef XRT_ENABLE_AIE    
    return std::make_unique<graph_object>(m_shim, m_uuid, name, am, this);
#else
    throw xrt_core::error(std::errc::not_supported, __func__);
#endif
  }

  std::unique_ptr<xrt_core::profile_handle>
  hwctx_object::open_profile_handle()
  {
#ifdef XRT_ENABLE_AIE    
    return std::make_unique<profile_object>(m_shim, m_aie_array);
#else
    throw xrt_core::error(std::errc::not_supported, __func__);
#endif
  }

  std::unique_ptr<xrt_core::aie_buffer_handle>
  hwctx_object::open_aie_buffer_handle(const char* name)
  {
#ifdef XRT_ENABLE_AIE
    auto device{xrt_core::get_userpf_device(m_shim)};
    return std::make_unique<aie_buffer_object>(device.get(),m_uuid,name,this);
#else
    throw xrt_core::error(std::errc::not_supported, __func__);
#endif
  }

}
