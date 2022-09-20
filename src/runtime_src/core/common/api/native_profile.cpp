/**
 * Copyright (C) 2016-2022 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#define XRT_CORE_COMMON_SOURCE

#include "native_profile.h"
#include "core/common/module_loader.h"
#include "core/common/utils.h"
#include "core/common/dlfcn.h"
#include "core/common/time.h"

namespace xdp::native {

bool load()
{
  static xrt_core::module_loader xdp_native_loader("xdp_native_plugin",
						   register_functions,
						   warning_function);
  return true ;
}

// Callbacks for generic start/stop function tracking
std::function<void (const char*, unsigned long long int)> function_start_cb ;
std::function<void (const char*, unsigned long long int, unsigned long long int)> function_end_cb ;

// Callbacks for individual functions to track start/stop and statistics
  std::function<void (const char*, unsigned long long int, bool)> sync_start_cb ;
std::function<void (const char*, unsigned long long int, unsigned long long int, bool, unsigned long long int)> sync_end_cb ;
  
void register_functions(void* handle)
{
  using start_type      = void (*)(const char*, unsigned long long int) ;
  using sync_start_type = void (*)(const char*, unsigned long long int, bool) ;
  using end_type        = void (*)(const char*, unsigned long long int,
                                   unsigned long long int) ;
  using end_sync_type   = void (*)(const char*, unsigned long long int,
                                   unsigned long long int, bool,
                                   unsigned long long int) ;

  // Generic callbacks
  function_start_cb =
    reinterpret_cast<start_type>(xrt_core::dlsym(handle, "native_function_start")) ;

  function_end_cb =
    reinterpret_cast<end_type>(xrt_core::dlsym(handle, "native_function_end")) ;

  // Sync callbacks
  sync_start_cb =
    reinterpret_cast<sync_start_type>(xrt_core::dlsym(handle, "native_sync_start")) ;

  sync_end_cb =
    reinterpret_cast<end_sync_type>(xrt_core::dlsym(handle, "native_sync_end")) ;
}

void warning_function()
{}

api_call_logger::
api_call_logger(const char* function)
  : m_funcid(0), m_fullname(function)
{
  // With the addition of the generic "host_trace" feature, we have to
  //  check if we should load the plugin.  We only want to load it if
  //  native_xrt_trace is specified or if we are the topmost layer and
  //  host_trace was specified
  static bool s_load_native =
    (xrt_core::config::get_native_xrt_trace() ||
     xrt_core::utils::load_host_trace()) ? load() : false;
  if (s_load_native) return;

}

generic_api_call_logger::
generic_api_call_logger(const char* function)
  : api_call_logger(function)
{
  if (function_start_cb) {
    m_funcid = xrt_core::utils::issue_id() ;
    function_start_cb(m_fullname, m_funcid) ;
  }
}

generic_api_call_logger::
~generic_api_call_logger()
{
  auto timestamp = static_cast<unsigned long long int>(xrt_core::time_ns());

  if (function_end_cb) {
    function_end_cb(m_fullname, m_funcid, timestamp) ;
  }
}

sync_logger::
sync_logger(const char* function, bool w, size_t s)
  : api_call_logger(function), m_is_write(w), m_buffer_size(s)
{
  if (sync_start_cb) {
    m_funcid = xrt_core::utils::issue_id() ;
    sync_start_cb(m_fullname, m_funcid, m_is_write) ;
  }
}

sync_logger::
~sync_logger()
{
  auto timestamp = static_cast<unsigned long long int>(xrt_core::time_ns());

  if (sync_end_cb) {
    sync_end_cb(m_fullname, m_funcid, timestamp, m_is_write, static_cast<unsigned long long int>(m_buffer_size)) ;
  }
}

} // end namespace xdp::native

