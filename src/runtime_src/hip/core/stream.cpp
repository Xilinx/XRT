// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.

#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "common.h"
#include "event.h"
#include "stream.h"

namespace xrt::core::hip {
stream::
stream(std::shared_ptr<context> ctx, unsigned int flags, bool is_null)
  : m_ctx{std::move(ctx)}
  , m_flags{flags}
  , null{is_null}
{
  // instream stream handle in list maintained by context
  m_ctx->add_stream(this);
}

stream::
~stream()
{
  m_ctx->remove_stream(this);
}

void
stream::
enqueue(std::shared_ptr<command>&& cmd)
{
  // if there is top event dont start command
  // add command to chain list of event
  bool start = top_event ? false : true;
  cmd->submit(start);
  if (top_event)
    top_event->add_to_chain(cmd);

  std::lock_guard<std::mutex> lock(m);
  cmd_queue.emplace(std::move(cmd));
}

std::shared_ptr<command>
stream::
dequeue()
{
  std::lock_guard<std::mutex> lock(m);
  if (cmd_queue.empty()) {
    return nullptr;
  }
  auto cmd = std::move(cmd_queue.front());
  cmd_queue.pop();
  return cmd;
}

void
stream::
synchronize()
{
  if (m_flags & hipStreamNonBlocking)
    return; // non blocking stream

  // lock streams before accessing
  std::lock_guard<std::mutex> lock(streams.mutex);
  // iterate over streams in this ctx
  for (auto stream_handle : this->m_ctx->get_stream_handles()) {
    // check if valid stream, stream is blocking
    // and stream is not current stream
    auto hip_stream = streams.stream_cache.get(stream_handle);
    if (!hip_stream)
     continue;

    if (!(hip_stream->flags() & hipStreamNonBlocking) && hip_stream.get() != this) {
      // non null streams wait on null stream only and
      // null stream waits on all blocking streams
      if (!null && !hip_stream->is_null())
        continue;
      hip_stream->await_completion();
    }
  }
}

void
stream::
await_completion()
{
  std::lock_guard<std::mutex> lk(m);
  while(!cmd_queue.empty()) {
    auto cmd = cmd_queue.front();
    cmd->wait();
    cmd_queue.pop();
  }
}

void
stream::
record_top_event(event* ev)
{
  std::lock_guard<std::mutex> lk(m);
  top_event = ev;
}

std::shared_ptr<stream>
get_stream(hipStream_t stream)
{
  // lock before getting any stream
  std::lock_guard<std::mutex> lock(streams.mutex);
  // app dint pass stream, use legacy default stream (null stream)
  if (!stream) {
    auto ctx = get_current_context();
    throw_context_destroyed_if(!ctx, "context is destroyed, no active context");
    return ctx->get_null_stream();
  }
  // TODO: Add support for per thread streams
  // if (stream == hipStreamPerThread)
  //   return get_per_thread_stream();

  return streams.stream_cache.get(stream);
}

// Global map of streams
stream_set streams;
}

