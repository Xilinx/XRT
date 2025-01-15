// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _ZYNQ_GRAPH_OBJECT_H_
#define _ZYNQ_GRAPH_OBJECT_H_

#include "core/edge/user/aie/aie.h"
#include "core/edge/user/aie/common_layer/adf_api_config.h"
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
  class hwctx_object;
  class graph_object : public xrt_core::graph_handle
  {
    ZYNQ::shim* m_shim;
    enum class graph_state : unsigned short
    {
      stop = 0,
      reset = 1,
      running = 2,
      suspend = 3,
      end = 4,
    };
    int id;
    graph_state state;
    std::string name;
    xrt::graph::access_mode access_mode;
    zynqaie::hwctx_object* m_hwctx = nullptr;

    /**
     * This is the pointer to the AIE array where the AIE part of
     * the graph resides. The Aie is an obect that holds the whole
     * AIE resources, configurations etc.
     */
    std::shared_ptr<aie_array> m_aie_array;

    /**
     * This is the collections of tiles that this graph uses.
     * A tile is represented by a pair of number <col, row>
     * It represents the tile position in the AIE array.
     */
    adf::graph_config graph_config;
    std::shared_ptr<adf::graph_api> graph_api_obj;
    /* This is the collections of rtps that are used. */
    std::unordered_map<std::string, adf::rtp_config> rtps;

  public:
    graph_object(ZYNQ::shim* shim, const xrt::uuid& uuid , const char* name,
                 xrt::graph::access_mode am, zynqaie::hwctx_object* hwctx = nullptr);

    ~graph_object();

    std::string
    getname() const;

    unsigned short
    getstatus() const;

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
#endif  //_ZYNQ_GRAPH_OBJECT_H_
