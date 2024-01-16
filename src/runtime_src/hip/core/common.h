// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_common_h
#define xrthip_common_h

#include "context.h"
#include "device.h"

#include <stack>

namespace xrt::core::hip {
// thread local hip objects
class hip_tls_objs
{
public:
  std::stack<std::shared_ptr<context>> ctx_stack;
  bool pri_ctx_active = false;
  std::shared_ptr<context> pri_ctx = nullptr;
};
extern thread_local hip_tls_objs tls_objs;
} // xrt::core::hip

#endif
