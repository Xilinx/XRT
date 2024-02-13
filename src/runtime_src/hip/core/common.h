// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_common_h
#define xrthip_common_h

#include "context.h"
#include "device.h"

#include <stack>

namespace xrt::core::hip {
struct ctx_info
{
  context_handle_t ctx_hdl = nullptr;
  device_handle dev_hdl = nullptr;
  bool active = false;
};

// thread local hip objects
struct hip_tls_objs
{
  std::stack<std::weak_ptr<context>> ctx_stack;
  ctx_info pri_ctx_info;
};
extern thread_local hip_tls_objs tls_objs;
} // xrt::core::hip

#endif
