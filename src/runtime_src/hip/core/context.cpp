// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.

#include "common.h"
#include "context.h"
#include "device.h"

namespace xrt::core::hip {
context::
context(std::shared_ptr<device> device)
  : m_device(std::move(device))
{}

// Global map of contexts
xrt_core::handle_map<context_handle, std::shared_ptr<context>> context_cache;

// thread local hip objects
thread_local hip_tls_objs tls_objs;

// returns current context
// if primary context is active it is current
// else returns top of ctx stackk
std::shared_ptr<context>
get_current_context()
{
  if (tls_objs.pri_ctx_info.active)
    return context_cache.get(tls_objs.pri_ctx_info.ctx_hdl);

  if (!tls_objs.ctx_stack.empty())
    return tls_objs.ctx_stack.top();

  return nullptr;
}
}
