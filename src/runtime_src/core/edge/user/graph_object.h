// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#ifndef _ZYNQ_GRAPH_OBJECT_H_
#define _ZYNQ_GRAPH_OBJECT_H_

#include "core/common/message.h"
#include "core/common/shim/shared_handle.h"
#include "core/common/shim/graph_handle.h"
#include "core/include/xrt/xrt_uuid.h"
#include "xrt/xrt_graph.h"


#include <memory>

namespace ZYNQ {
	class shim;
}
namespace zynqaie {
  // Shim handle for graph object
  class graph_instance;
  class hwctx_object;
  class graph_object : public xrt_core::graph_handle
  {
    ZYNQ::shim* m_shim;
    std::unique_ptr<zynqaie::graph_instance> m_graphInstance;

  public:
    graph_object(ZYNQ::shim* shim, const xrt::uuid& uuid , const char* name,
                    xrt::graph::access_mode am, const zynqaie::hwctx_object* hwctx = nullptr);

    void
    reset_graph() override;

    uint64_t
    get_timestamp() override;

    void
    run_graph(int iterations) override;

    int
    wait_graph_done(int timeout) override;

    void
    wait_graph(uint64_t cycle) override;

    void
    suspend_graph() override;

    void
    resume_graph() override;

    void
    end_graph(uint64_t cycle) override;

    void
    update_graph_rtp(const char* port, const char* buffer, size_t size) override;

    void
    read_graph_rtp(const char* port, char* buffer, size_t size) override;
  }; // graph_object
}
#endif
