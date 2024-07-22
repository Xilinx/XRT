// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef XRT_CORE_GRAPH_HANDLE_H
#define XRT_CORE_GRAPH_HANDLE_H

namespace xrt_core {
class graph_handle
{
public:
  virtual ~graph_handle() {}

  virtual void
  reset_graph() = 0;

  virtual uint64_t
  get_timestamp() = 0;

  virtual void
  run_graph(int iterations) = 0;

  virtual int
  wait_graph_done(int timeout) = 0;

  virtual void
  wait_graph(uint64_t cycle) = 0;

  virtual void
  suspend_graph() = 0;

  virtual void
  resume_graph() = 0;

  virtual void
  end_graph(uint64_t cycle) = 0;

  virtual void
  update_graph_rtp(const char* port, const char* buffer, size_t size) = 0;

  virtual void
  read_graph_rtp(const char* port, char* buffer, size_t size) = 0;
};

} // xrt_core
#endif
