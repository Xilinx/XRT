// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.

#include "common.h"
#include "context.h"
#include "device.h"
#include "stream.h"

namespace xrt::core::hip {
context::
context(std::shared_ptr<device> device)
  : m_device{std::move(device)}
{}

// Global map of contexts
xrt_core::handle_map<context_handle, std::shared_ptr<context>> context_cache;

// thread local hip objects
thread_local hip_tls_objs tls_objs;

// returns current context
// if primary context is active it is current
// else returns top of ctx stack
// this function returns primary ctx on active device if
// no context is active
std::shared_ptr<context>
get_current_context()
{
  if (tls_objs.pri_ctx_info.active)
    return context_cache.get(tls_objs.pri_ctx_info.ctx_hdl);

  // primary ctx is not active
  // retrun ctx from stack, top of stack can be invalid
  // because of previous destroy calls, return valid ones
  std::shared_ptr<context> ctx;
  while (!ctx && !tls_objs.ctx_stack.empty()) {
    ctx = tls_objs.ctx_stack.top().lock();
    if (!ctx)
      tls_objs.ctx_stack.pop();
  }

  if (ctx)
    return ctx;

  // if no active ctx, create primary ctx on active device
  auto ctx_hdl = hip_device_primary_ctx_retain(tls_objs.dev_hdl);
  return context_cache.get(ctx_hdl);
}

std::shared_ptr<stream>
context::
get_null_stream()
{
  std::shared_ptr<stream> s = null_stream.lock();
  if (s)
    return s;

  // create null stream
  auto null_s = std::make_shared<stream>(shared_from_this(), hipStreamDefault, true);
  // assign weak ptr for future use
  null_stream = null_s;
  insert_in_map(stream_cache, std::move(null_s));
  return null_stream.lock(); // inserted in map, so we can get shared_ptr from weak_ptr
}
} // xrt::core::hip

