// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "common.h"
#include "context.h"
#include "device.h"
#include "stream.h"

namespace xrt::core::hip {
context::
context(device* device)
  : m_device{ device }
{}

// Global map of contexts
//we should override clang-tidy warning by adding NOLINT since context_cache is non-const parameter
xrt_core::handle_map<context_handle, std::shared_ptr<context>> context_cache; //NOLINT

// thread local hip objects
//we should override clang-tidy warning by adding NOLINT since tls_objs is non-const parameter
thread_local hip_tls_objs tls_objs; //NOLINT

// create primary context on given device if not already present
// else increment reference count
// TODO: this function is copied from the old hip_context.cpp, future improvement can be consider to
// remove HIP context concept. hip context management APIs are deprecated since version 1.9.0, since
// HIP context itself is widely used inside XRT HIP implementation, keeps the concept here for now,
// while removing the user APIs implementation.
static
context_handle
primary_ctx_retain_from_device(device_handle dev)
{
  auto hip_dev = device_cache.get(dev);
  throw_invalid_device_if(!hip_dev, "Invalid device");

  auto hip_ctx = hip_dev->get_pri_ctx();
  // create primary context
  if (!hip_ctx) {
    hip_ctx = std::make_shared<context>(hip_dev);
    hip_dev->set_pri_ctx(hip_ctx);
  }
  // ref count of primary context is incremented by pushing on to map with
  // unqiue handle, using thread id here as primary context is unique per thread
  auto ctx_hdl =
      reinterpret_cast<context_handle>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
  context_cache.add(ctx_hdl, std::move(hip_ctx));

  tls_objs.pri_ctx_info.active = true;
  tls_objs.pri_ctx_info.ctx_hdl = ctx_hdl;
  tls_objs.dev_hdl = dev;
  return ctx_hdl;
}

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
  auto ctx_hdl = primary_ctx_retain_from_device(tls_objs.dev_hdl);
  return context_cache.get(ctx_hdl);
}

std::shared_ptr<stream>
context::
get_null_stream()
{
  std::shared_ptr<stream> s = m_null_stream.lock();
  if (s)
    return s;

  // create null stream
  auto null_s = std::make_shared<stream>(shared_from_this(), hipStreamDefault, true);
  // track the newly created shared ptr
  m_null_stream = null_s;
  insert_in_map(stream_cache, std::move(null_s));
  return m_null_stream.lock();
}

device*
get_current_device()
{
  auto ctx = get_current_context();
  throw_context_destroyed_if(!ctx, "context is destroyed, no active context");
  return ctx->get_device();
}
} // xrt::core::hip

