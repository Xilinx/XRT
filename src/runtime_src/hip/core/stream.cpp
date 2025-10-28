// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include <typeinfo>

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
  , m_null{is_null}
{
  // insert stream handle in list maintained by context
  m_ctx->add_stream(this);
}

stream::
~stream()
{
  m_ctx->remove_stream(this);
}

void
stream::
enqueue(std::shared_ptr<command> cmd)
{
  // if there is top event add command chain list of this event
  // else submit the command
  std::lock_guard<std::mutex> lock(m_cmd_lock);
  if (m_top_event)
    m_top_event->add_to_chain(cmd);
  else
    cmd->submit();

  m_cmd_queue.emplace_back(std::move(cmd));
}

std::shared_ptr<command>
stream::
dequeue()
{
  std::lock_guard<std::mutex> lock(m_cmd_lock);
  if (m_cmd_queue.empty()) {
    return nullptr;
  }
  auto cmd = m_cmd_queue.front();
  m_cmd_queue.pop_front();
  return cmd;
}

bool
stream::
erase_cmd(std::shared_ptr<command> cmd)
{
  std::lock_guard<std::mutex> lock(m_cmd_lock);
  auto it = std::find(m_cmd_queue.begin(), m_cmd_queue.end(), cmd);
  if (it != m_cmd_queue.end()) {
    m_cmd_queue.erase(it);
    return true;
  }
  return false;
}

void
stream::
enqueue_event(const std::shared_ptr<event>& ev)
{
  {
    // iterate over commands and add them to recorded list of event
    std::lock_guard<std::mutex> lock(m_cmd_lock);
    for (const auto& cmd : m_cmd_queue) {
      ev->add_dependency(cmd);
    }
  }
  enqueue(ev);
}

void
stream::
synchronize_streams()
{
  // non blocking stream doesn't wait on any other streams
  if (m_flags & hipStreamNonBlocking)
    return;

  // iterate over streams in this ctx
  for (auto stream_handle : this->m_ctx->get_stream_handles()) {
    // check if valid stream, stream is blocking
    // and stream is not current stream
    auto hip_stream = stream_cache.get(stream_handle);
    if (!hip_stream)
     continue;

    if (!(hip_stream->flags() & hipStreamNonBlocking) && hip_stream.get() != this) {
      // non null streams wait on null stream only and
      // null stream waits on all blocking streams
      if (!m_null && !hip_stream->is_null())
        continue;
      // complete commands
      hip_stream->await_completion();
    }
  }
}

void
stream::
await_completion()
{
  std::lock_guard<std::mutex> lk(m_cmd_lock);
  uint32_t cmd_id = 0;
  bool has_failure = false;
  while(!m_cmd_queue.empty()) {
    auto cmd = m_cmd_queue.front();
    try {
      cmd->wait();
      if (cmd->get_state() != command::state::completed)
        throw std::runtime_error("execution failed.");
    }
    catch (const std::exception &ex) {
      std::string err_str = "CMD[" + std::to_string(cmd_id) + "]:" + get_unmangled_type_name(*cmd) + ":" + ex.what();
      xrt::core::hip::error::instance().record_local_error(hipErrorLaunchFailure, err_str);
      has_failure = true;
    }
    catch (...) {
      std::string err_str = "CMD[" + std::to_string(cmd_id) + "]:" + get_unmangled_type_name(*cmd) + ":unknown exception";
      xrt::core::hip::error::instance().record_local_error(hipErrorLaunchFailure, err_str);
      has_failure = true;
    }
    // kernel_start and copy_buffer cmds needs to be explicitly removed from cache
    // there is no destroy call for them
    if (cmd->get_type() != command::type::event)
      command_cache.remove(cmd.get());
    m_cmd_queue.pop_front();
    cmd_id++;
  }
  // reset m_top_event as stream completed
  m_top_event = nullptr;
  // throw launch failure error if there is any command failed to execute.
  if (has_failure)
    throw_hip_error(hipErrorLaunchFailure, "Stream execution failed.");
}

void
stream::
synchronize()
{
  // synchronize among streams in this ctx
  synchronize_streams();

  // complete commands in this stream
  await_completion();
}

void
stream::
record_top_event(std::shared_ptr<event> ev)
{
  std::lock_guard<std::mutex> lk(m_cmd_lock);

  // previous top event is added as a dependency to the new top event
  if (m_top_event) {
    ev->add_dependency(std::move(m_top_event));
  }

  m_top_event = std::move(ev);
}

std::shared_ptr<stream>
get_stream(hipStream_t stream)
{
  // app did not pass stream, use legacy default stream (null stream)
  if (!stream) {
    auto ctx = get_current_context();
    throw_context_destroyed_if(!ctx, "context is destroyed, no active context");
    return ctx->get_null_stream();
  }
  // TODO: Add support for per thread streams
  // if (stream == hipStreamPerThread)
  //   return get_per_thread_stream();

  return stream_cache.get(stream);
}

// Global map of streams
xrt_core::handle_map<stream_handle, std::shared_ptr<stream>> stream_cache;
}
