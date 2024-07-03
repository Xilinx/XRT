// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#include "hwctx_object.h"
#include "graph_object.h"
#include "shim.h"

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

  std::unique_ptr<xrt_core::buffer_handle>
  hwctx_object::alloc_bo(void* userptr, size_t size, uint64_t flags)
  {
    // The hwctx is embedded in the flags, use regular shim path
    return m_shim->xclAllocUserPtrBO(userptr, size, xcl_bo_flags{flags}.flags);
  }

  std::unique_ptr<xrt_core::buffer_handle>
  hwctx_object::alloc_bo(size_t size, uint64_t flags)
  {
    // The hwctx is embedded in the flags, use regular shim path
    return m_shim->xclAllocBO(size, xcl_bo_flags{flags}.flags);
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
    m_shim->xclExecBuf(cmd->get_xcl_handle());
  }

  std::unique_ptr<xrt_core::graph_handle>
  hwctx_object::open_graph_handle(const char* name, xrt::graph::access_mode am)
  {
    return std::make_unique<graph_object>(m_shim, m_uuid, name, am, this);
  }
}
