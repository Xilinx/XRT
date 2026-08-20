// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef XRT_CORE_GRAPH_HANDLE_H
#define XRT_CORE_GRAPH_HANDLE_H

#include <cstddef>
#include <cstdint>

namespace xrt_core {

// class graph_handle - shim base class for AIE graph objects
//
// Shim level implementations derive off this class to support
// opaque graph objects where implementation details are platform
// specific.  The xrt::graph API dispatches graph operations through
// this virtual interface.
class graph_handle
{
public:
  virtual ~graph_handle() {}

  // Reset graph by disabling tiles and enabling tile reset
  virtual void
  reset_graph() = 0;

  // Get graph timestamp in AIE cycles
  virtual uint64_t
  get_timestamp() = 0;

  // Start graph execution for the given number of iterations
  virtual void
  run_graph(int iterations) = 0;

  // Wait for graph run to complete; timeout in milliseconds
  virtual int
  wait_graph_done(int timeout) = 0;

  // Wait for the given number of AIE cycles since last run, then suspend
  virtual void
  wait_graph(uint64_t cycle) = 0;

  // Suspend a running graph
  virtual void
  suspend_graph() = 0;

  // Resume a suspended graph
  virtual void
  resume_graph() = 0;

  // Wait for the given number of AIE cycles since last run, then terminate
  virtual void
  end_graph(uint64_t cycle) = 0;

  // Update a graph run-time parameter (RTP) port; blocks on tile locks
  virtual void
  update_graph_rtp(const char* port, const char* buffer, size_t size) = 0;

  // Read a graph run-time parameter (RTP) port; blocks on tile locks
  virtual void
  read_graph_rtp(const char* port, char* buffer, size_t size) = 0;

  // Non-blocking RTP update.  Returns 0 on success, EAGAIN if lock unavailable
  virtual int
  update_graph_rtp_nb(const char* port, const char* buffer, size_t size) = 0;

  // Non-blocking RTP read.  Returns 0 on success, EAGAIN if lock unavailable
  virtual int
  read_graph_rtp_nb(const char* port, char* buffer, size_t size) = 0;
};

} // xrt_core
#endif
