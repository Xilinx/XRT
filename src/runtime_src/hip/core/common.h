// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrthip_common_h
#define xrthip_common_h

#include "core/common/error.h"

#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "hip/core/error.h"

#include "context.h"
#include "device.h"

#include <cstdlib>
#include <memory>
#include <stack>
#include <string>
#include <thread>
#include <typeinfo>

#if defined(__GNUC__) || defined(__clang__)
# include <cxxabi.h>
#endif

namespace xrt::core::hip {
struct ctx_info
{
  context_handle ctx_hdl{nullptr};
  bool active{false};
};

// thread local hip objects
struct hip_tls_objs
{
  hip_tls_objs() noexcept {}
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

template <class T>
std::string
get_unmangled_type_name(const T& obj)
{
  const char* mangled_name = typeid(obj).name();
#if defined(__GNUC__) || defined(__clang__)
  int status = 0;
  std::unique_ptr<char, decltype(&std::free)> demangled_name(
    abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status), &std::free
  );
  return (status == 0) ? demangled_name.get() : mangled_name;
#elif defined(_MSC_VER)
  // MSVC's typeid().name() returns a readable name by default
  return mangled_name;
#else
  return mangled_name;
#endif
}
} // xrt::core::hip

namespace {

template<typename F> hipError_t
handle_hip_func_error(const char* func_name, hipError_t default_err, F && f)
{
  xrt::core::hip::error::instance().reset_local_errors();
  try {
    std::forward<F>(f)();
    return hipSuccess;
  }
  catch (const xrt::core::hip::hip_exception& ex) {
    xrt::core::hip::error::instance().record_local_error(ex.value(), ex.what());
    xrt_core::send_exception_message(std::string(func_name) + " - " + ex.what());
    return ex.value();
  }
  catch (const xrt_core::system_error& ex) {
    hipError_t hip_err = xrt::core::hip::system_to_hip_error(ex.value());
    xrt::core::hip::error::instance().record_local_error(hip_err, ex.what());
    xrt_core::send_exception_message(std::string(func_name) + " - " + ex.what());
    return hip_err;
  }
  catch (const std::exception& ex) {
    // This is not thrown as hipError_t, that is it is not recorded, record it here.
    xrt::core::hip::error::instance().record_local_error(default_err, ex.what());
    xrt_core::send_exception_message(std::string(func_name) + " - " + ex.what());
  }
  return default_err;
}

// common functions for throwing hip errors
inline void
throw_hip_error(hipError_t err, const char* err_msg)
{
  throw xrt::core::hip::hip_exception(err, err_msg);
}

inline void
throw_if(bool check, hipError_t err, const char* err_msg)
{
  if (check)
    throw_hip_error(err, err_msg);
}

inline void
throw_invalid_value_if(bool check, const char* err_msg)
{
  throw_if(check, hipErrorInvalidValue, err_msg);
}

inline void
throw_invalid_handle_if(bool check, const char* err_msg)
{
  throw_if(check, hipErrorInvalidHandle, err_msg);
}

inline void
throw_invalid_device_if(bool check, const char* err_msg)
{
  throw_if(check, hipErrorInvalidDevice, err_msg);
}

inline void
throw_invalid_resource_if(bool check, const char* err_msg)
{
  throw_if(check, hipErrorInvalidResourceHandle, err_msg);
}

inline void
throw_context_destroyed_if(bool check, const char* err_msg)
{
  throw_if(check, hipErrorContextIsDestroyed, err_msg);
}

}
#endif

