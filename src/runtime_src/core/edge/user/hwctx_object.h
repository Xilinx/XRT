// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#ifndef _ZYNQ_HWCTX_OBJECT_H_
#define _ZYNQ_HWCTX_OBJECT_H_

#include "core/common/shim/hwctx_handle.h"
#include "core/common/shim/shared_handle.h"
#include "core/common/shim/graph_handle.h"

// Shim handle for hardware context Even as hw_emu does not
// support hardware context, it still must implement a shim
// hardware context handle representing the default slot
namespace ZYNQ {
  class shim;
}

namespace zynqaie {
  class hwctx_object : public xrt_core::hwctx_handle
  {
    ZYNQ::shim* m_shim;
    xrt::uuid m_uuid;
    slot_id m_slotidx;
    xrt::hw_context::access_mode m_mode;

  public:
    hwctx_object(ZYNQ::shim* shim, slot_id slotidx, xrt::uuid uuid, xrt::hw_context::access_mode mode);

    void
    update_access_mode(access_mode mode) override
    {
      m_mode = mode;
    }

    slot_id
    get_slotidx() const override
    {
      return m_slotidx;
    }

    xrt::hw_context::access_mode
    get_mode() const
    {
      return m_mode;
    }

    xrt::uuid
    get_xclbin_uuid() const
    {
      return m_uuid;
    }

    xrt_core::hwqueue_handle*
    get_hw_queue() override
    {
      return nullptr;
    }

    std::unique_ptr<xrt_core::buffer_handle>
    alloc_bo(void* userptr, size_t size, uint64_t flags) override;

    std::unique_ptr<xrt_core::buffer_handle>
    alloc_bo(size_t size, uint64_t flags) override;

    xrt_core::cuidx_type
    open_cu_context(const std::string& cuname) override;

    void
    close_cu_context(xrt_core::cuidx_type cuidx) override;

    void
    exec_buf(xrt_core::buffer_handle* cmd) override;

    std::unique_ptr<xrt_core::graph_handle>
    open_graph_handle(const char* name, xrt::graph::access_mode am) override;
  }; // class hwctx_object
}
#endif
