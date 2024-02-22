// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_common_h
#define xrthip_common_h

#include "core/common/error.h"

#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "context.h"
#include "device.h"

#include <stack>
#include <thread>

namespace xrt::core::hip {
struct ctx_info
{
  context_handle ctx_hdl{nullptr};
  bool active{false};
};

// thread local hip objects
struct hip_tls_objs
{
  device_handle dev_hdl{std::numeric_limits<uint32_t>::max()};
  std::stack<std::weak_ptr<context>> ctx_stack;
  ctx_info pri_ctx_info;
};
extern thread_local hip_tls_objs tls_objs;
} // xrt::core::hip

namespace {
// common function that throws hip error
inline void
throw_if(bool check, hipError_t err, const std::string& err_msg)
{
  check ? throw xrt_core::system_error(err, err_msg) : void();
}
}
#endif

