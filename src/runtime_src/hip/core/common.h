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

// generic function for adding shared_ptr to handle_map
// {key , value} -> {shared_ptr.get(), shared_ptr}
// returns void* (handle returned to application)
template<typename map, typename value>
inline void*
insert_in_map(map& m, value&& v)
{
  auto handle = v.get();
  m.add(handle, std::move(v));
  return handle;
}
} // xrt::core::hip

namespace {
// common functions for throwing hip errors
inline void
throw_if(bool check, hipError_t err, const std::string& err_msg)
{
  if (check)
    throw xrt_core::system_error(err, err_msg);
}

inline void
throw_invalid_value_if(bool check, const std::string& err_msg)
{
  throw_if(check, hipErrorInvalidValue, err_msg);
}

inline void
throw_invalid_handle_if(bool check, const std::string& err_msg)
{
  throw_if(check, hipErrorInvalidHandle, err_msg);
}

inline void
throw_invalid_device_if(bool check, const std::string& err_msg)
{
  throw_if(check, hipErrorInvalidDevice, err_msg);
}

inline void
throw_invalid_resource_if(bool check, const std::string& err_msg)
{
  throw_if(check, hipErrorInvalidResourceHandle, err_msg);
}

inline void
throw_context_destroyed_if(bool check, const std::string& err_msg)
{
  throw_if(check, hipErrorContextIsDestroyed, err_msg);
}
}
#endif

