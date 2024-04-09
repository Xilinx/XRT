// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc.  All rights reserved.
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef NATIVE_PROFILE_DOT_H
#define NATIVE_PROFILE_DOT_H
#include "core/common/config.h"
#include "core/common/config_reader.h"
#include "core/include/xrt.h"

// This file contains the callback mechanisms for connecting the
// Native XRT API (C/C++ layer) to the XDP plugin
namespace xdp::native {

// The functions responsible for loading and linking the plugin
void
load();

void
register_functions(void* handle);

void
warning_function();

// An instance of the api_call_logger class will be created in every
// function we are monitoring.  The constructor marks the start time,
// and the destructor marks the end time
class api_call_logger
{
 protected:
  uint64_t m_funcid ;
  const char* m_fullname = nullptr ;
 public:
  explicit api_call_logger(const char* function);
  virtual ~api_call_logger() = default ;
} ;

class generic_api_call_logger : public api_call_logger
{
  generic_api_call_logger() = delete ;
  generic_api_call_logger(const generic_api_call_logger&) = delete ;
  generic_api_call_logger(generic_api_call_logger&&) = delete ;
  void operator=(const generic_api_call_logger&) = delete ;
  void operator=(generic_api_call_logger&&) = delete ;

public:
  explicit generic_api_call_logger(const char* function);
  ~generic_api_call_logger();
} ;

template <typename Callable, typename ...Args>
auto
profiling_wrapper(const char* function, Callable&& f, Args&&...args)
{
  if (xrt_core::config::get_native_xrt_trace()
      || xrt_core::config::get_host_trace()) {
    generic_api_call_logger log_object(function) ;
    return f(std::forward<Args>(args)...) ;  // NOLINT, clang-tidy false positive [potential leak]
  }
  return f(std::forward<Args>(args)...) ;    // NOLINT, clang-tidy false positive [potential leak]
}

// Specializations of the logger for capturing different information
// for use in summary tables.
class sync_logger : public api_call_logger
{
  bool m_is_write;
  size_t m_buffer_size;

  sync_logger() = delete;
  sync_logger(const sync_logger&) = delete;
  sync_logger(sync_logger&&) = delete;
  void operator=(const sync_logger&) = delete;
  void operator=(sync_logger&&) = delete;

 public:
  explicit sync_logger(const char* function, bool w, size_t s);
  ~sync_logger();
} ;

template <typename Callable, typename ...Args>
auto
profiling_wrapper_sync(const char* function, xclBOSyncDirection dir, size_t size, Callable&& f, Args&&...args)
{
  if (xrt_core::config::get_native_xrt_trace() ||
      xrt_core::config::get_host_trace()) {
    sync_logger log_object(function, (dir == XCL_BO_SYNC_BO_TO_DEVICE), size);
    return f(std::forward<Args>(args)...) ;
  }
  return f(std::forward<Args>(args)...) ;
}

} // end namespace xdp::native

#endif
