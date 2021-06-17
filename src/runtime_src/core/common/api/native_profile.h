/**
 * Copyright (C) 2016-2021 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef NATIVE_PROFILE_DOT_H
#define NATIVE_PROFILE_DOT_H

#include "core/common/config.h"
#include "core/common/config_reader.h"
#include "core/include/xrt.h"

/**
 * This file contains the callback mechanisms for connecting the
 * Native XRT API (C/C++ layer) to the XDP plugin
 */

namespace xdp {
namespace native {

// The functions responsible for loading and linking the plugin
bool load() ;
void register_functions(void* handle) ;
void warning_function() ;

// An instance of the api_call_logger class will be created
//  in every function we are monitoring.  The constructor marks the
//  start time, and the destructor marks the end time
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
 private:
  generic_api_call_logger() = delete ;
  generic_api_call_logger(const generic_api_call_logger& x) = delete ;
  generic_api_call_logger(generic_api_call_logger&& x) = delete ;
  void operator=(const generic_api_call_logger& x) = delete ;
  void operator=(generic_api_call_logger&& x) = delete ;
 public:
  explicit generic_api_call_logger(const char* function) ;
  ~generic_api_call_logger() override ;
} ;

template <typename Callable, typename ...Args>
auto
profiling_wrapper(const char* function, Callable&& f, Args&&...args)
{
  if (xrt_core::config::get_native_xrt_trace()) {
    generic_api_call_logger log_object(function) ;
    return f(std::forward<Args>(args)...) ;
  }
  return f(std::forward<Args>(args)...) ;
}

// Specializations of the logger for capturing different information
//  for use in summary tables.

class sync_logger : public api_call_logger
{
 private:
  bool m_is_write ;
  size_t m_buffer_size ;

  sync_logger() = delete ;
  sync_logger(const generic_api_call_logger& x) = delete ;
  sync_logger(generic_api_call_logger&& x) = delete ;
  void operator=(const sync_logger& x) = delete ;
  void operator=(sync_logger&& x) = delete ;

 public:
  explicit sync_logger(const char* function, bool w, size_t s);
  ~sync_logger() override ;
} ;

template <typename Callable, typename ...Args>
auto
profiling_wrapper_sync(const char* function, xclBOSyncDirection dir, size_t size, Callable&& f, Args&&...args)
{
  if (xrt_core::config::get_native_xrt_trace()) {
    sync_logger log_object(function, (dir == XCL_BO_SYNC_BO_TO_DEVICE), size);
    return f(std::forward<Args>(args)...) ;
  }
  return f(std::forward<Args>(args)...) ;
}

} // end namespace native
} // end namespace xdp

#endif
