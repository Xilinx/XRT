// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_stream_h
#define xrthip_stream_h

#include "context.h"

#include <list>

namespace xrt::core::hip {

// forward declarations
class event;
class command;

class stream
{
  std::shared_ptr<context> m_ctx;
  unsigned int m_flags;
  bool m_null;

  std::list<std::shared_ptr<command>> m_cmd_queue;
  std::mutex m_cmd_lock;
  event* m_top_event{nullptr};

public:
  stream() = default;
  stream(std::shared_ptr<context> ctx, unsigned int flags, bool is_null = false);

  ~stream();

  inline bool
  is_null() const
  {
    return m_null;
  }

  unsigned int
  flags() const
  {
    return m_flags;
  }

  void
  enqueue(std::shared_ptr<command>&& cmd);

  std::shared_ptr<command>
  dequeue();

  bool
  erase_cmd(std::shared_ptr<command> cmd);

  void
  enqueue_event(std::shared_ptr<event>&& ev);

  void
  synchronize_streams();

  void
  await_completion();

  void
  synchronize();

  void
  record_top_event(event* ev);
};

// Global map of streams
extern xrt_core::handle_map<stream_handle, std::shared_ptr<stream>> stream_cache;

std::shared_ptr<stream>
get_stream(hipStream_t stream);
} // xrt::core::hip

#endif

