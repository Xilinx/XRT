// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#include "graph_object.h"
#include "core/edge/user/aie/graph.h"
#include "core/common/system.h"
#include <memory>

namespace zynqaie {
  graph_object::graph_object(ZYNQ::shim* shim, const xrt::uuid& uuid , const char* name,
                  xrt::graph::access_mode am, const zynqaie::hwctx_object* hwctx)
    : m_shim{shim}
  {
    auto device{xrt_core::get_userpf_device(m_shim)};
    m_graphInstance = std::make_unique<zynqaie::graph_instance>(device, name, am, hwctx,uuid);
  }

  void
  graph_object::reset_graph()
  {
    try {
      m_graphInstance->reset();
    }
    catch (const std::exception& e) {
      xrt_core::send_exception_message(std::string("fail to reset graph: ") + e.what());
    }
    catch (...) {
      xrt_core::send_exception_message(std::string("fail to reset graph"));
    }
  }

  uint64_t
  graph_object::get_timestamp()
  {
    try {
      m_graphInstance->get_timestamp();
    }
    catch (const std::exception& e) {
      xrt_core::send_exception_message(std::string("fail to get graph timestamp: ") + e.what());
    }
    catch (...) {
      xrt_core::send_exception_message(std::string("fail to get graph timestamp"));
    }
    return -1;
  }

  void
  graph_object::run_graph(int iterations)
  {
    try {
      if (iterations == 0)
        m_graphInstance->run();
      else
        m_graphInstance->run(iterations);
    }
    catch (const std::exception& e) {
      xrt_core::send_exception_message(std::string("fail to run graph: ") + e.what());
    }
    catch (...) {
      xrt_core::send_exception_message(std::string("fail to run graph"));
    }
  }

  int
  graph_object::wait_graph_done(int timeout)
  {
    try {
      m_graphInstance->wait_done(timeout);
      return 0;
    }
    catch (const std::exception& e) {
      xrt_core::send_exception_message(std::string("fail to wait graph done: ") + e.what());
    }
    catch (...) {
      xrt_core::send_exception_message(std::string("fail to wait graph done"));
    }
    return -1;
  }

  void
  graph_object::wait_graph(uint64_t cycle)
  {
    try {
      if (cycle == 0)
        m_graphInstance->wait();
      else
        m_graphInstance->wait(cycle);
    }
    catch (const std::exception& e) {
      xrt_core::send_exception_message(std::string("fail to wait graph: ") + e.what());
    }
    catch (...) {
      xrt_core::send_exception_message(std::string("fail to wait graph"));
    }
  }

  void
  graph_object::suspend_graph()
  {
    try {
      m_graphInstance->suspend();
    }
    catch (const std::exception& e) {
      xrt_core::send_exception_message(std::string("fail to suspend graph: ") + e.what());
    }
    catch (...) {
      xrt_core::send_exception_message(std::string("fail to suspend graph"));
    }
  }

  void
  graph_object::resume_graph()
  {
    try {
      m_graphInstance->resume();
    }
    catch (const std::exception& e) {
      xrt_core::send_exception_message(std::string("fail to resume graph: ") + e.what());
    }
    catch (...) {
      xrt_core::send_exception_message(std::string("fail to resume graph"));
    }
  }

  void
  graph_object::end_graph(uint64_t cycle)
  {
    try {
      if (cycle == 0)
        m_graphInstance->end();
      else
        m_graphInstance->end(cycle);
    }
    catch (const std::exception& e) {
      xrt_core::send_exception_message(std::string("fail to end graph: ") + e.what());
    }
    catch (...) {
      xrt_core::send_exception_message(std::string("fail to end graph"));
    }
  }

  void
  graph_object::update_graph_rtp(const char* port, const char* buffer, size_t size)
  {
    try {
      m_graphInstance->update_rtp(port, buffer, size);
    }
    catch (const std::exception& e) {
      xrt_core::send_exception_message(std::string("fail to update graph rtp: ") + e.what());
    }
    catch (...) {
      xrt_core::send_exception_message(std::string("fail to update graph rtp"));
    }
  }

  void
  graph_object::read_graph_rtp(const char* port, char* buffer, size_t size)
  {
    try {
      m_graphInstance->read_rtp(port, buffer, size);
    }
    catch (const std::exception& e) {
      xrt_core::send_exception_message(std::string("fail to read graph rtp: ") + e.what());
    }
    catch (...) {
      xrt_core::send_exception_message(std::string("fail to read graph rtp"));
    }
  }
}
