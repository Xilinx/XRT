// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#include "hwctx_object.h"
#ifdef XRT_ENABLE_AIE
#include "core/edge/user/aie/graph_object.h"
#endif
#include "core/edge/user/shim.h"
#include "core/edge/user/aie/graph_api.h"

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
  {
#ifdef XRT_ENABLE_AIE
    auto device{xrt_core::get_userpf_device(m_shim)};
    m_aie_array = std::make_unique<Aie>(device, this);
#endif
  }

#ifdef XRT_ENABLE_AIE

  zynqaie::Aie*
  hwctx_object::get_aie_array_from_hwctx()
  {
    return m_aie_array.get();
  }

  bool
  hwctx_object::is_aie_registered()
  {
    return (nullptr != m_aie_array);
  }

#endif

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
    m_shim->xclExecBuf(cmd->get_xcl_handle());
  }

  std::unique_ptr<xrt_core::graph_handle>
  hwctx_object::open_graph_handle(const char* name, xrt::graph::access_mode am)
  {
#ifdef XRT_ENABLE_AIE    
    return std::make_unique<graph_object>(m_shim, m_uuid, name, am, this);
#else
    return nullptr;
#endif
  }

#ifdef XRT_ENABLE_AIE

int
hwctx_object::
start_profiling(int option, const char* port1Name, const char* port2Name, uint32_t value)
{
  try {
    return graph_api::start_profiling(m_shim, option, port1Name, port2Name, value, m_aie_array.get());
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (...) {
    xrt_core::send_exception_message("unknown exeception in START PROFILING");
  }
  return -1;
}

uint64_t
hwctx_object::
read_profiling(int phdl)
{
  try {
    return graph_api::read_profiling(m_shim, phdl, m_aie_array.get());
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (...) {
    xrt_core::send_exception_message("unknown exeception in READ PROFILING");
  }
  return -1;
}

void
hwctx_object::
stop_profiling(int phdl)
{
  try {
    graph_api::stop_profiling(m_shim, phdl, m_aie_array.get());
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (...) {
    xrt_core::send_exception_message("unknown exeception in STOP PROFILING");
  }
}

#endif

}
