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

// An instance of the native_api_call_logger class will be created
//  in every function we are monitoring.  The constructor marks the
//  start time, and the destructor marks the end time
class api_call_logger
{
 private:
  uint64_t m_funcid ;
  const char* m_name = nullptr ;
  const char* m_type = nullptr ;
 public:
  api_call_logger(const char* function, const char* type = nullptr) ;
  ~api_call_logger() ;
} ;

template <typename Callable, typename ...Args>
auto
profiling_wrapper(const char* function, const char* type,
                  Callable&& f, Args&&...args)
{
  if (xrt_core::config::get_native_xrt_trace()) {
    api_call_logger log_object(function, type) ;
    return f(std::forward<Args>(args)...) ;
  }
  return f(std::forward<Args>(args)...) ;
}

} // end namespace native
} // end namespace xdp

#endif
