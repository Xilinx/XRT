// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_stream_h
#define xrthip_stream_h

#include "context.h"

#include <condition_variable>
#include <queue>

namespace xrt::core::hip {

// forward declarations
class event;
class command;

class stream
{
  std::shared_ptr<context> m_ctx;
  unsigned int m_flags;
  bool null;

  std::queue<std::shared_ptr<command>> cmd_queue;
  std::mutex m;
  std::condition_variable cv;
  event* top_event{nullptr};

public:
  stream() = default;
  stream(std::shared_ptr<context> ctx, unsigned int flags, bool is_null = false);

  ~stream();

  inline bool
  is_null() const
  {
    return null;
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

  void
  synchronize();

  void
  await_completion();

  void
  record_top_event(event* ev);
};

struct stream_set
{
  std::mutex mutex;
  xrt_core::handle_map<stream_handle, std::shared_ptr<stream>> stream_cache;
};
extern stream_set streams;

std::shared_ptr<stream>
get_stream(hipStream_t stream);
} // xrt::core::hip

#endif

