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

#define XRT_CORE_COMMON_SOURCE

#include "native_profile.h"
#include "core/common/module_loader.h"
#include "core/common/utils.h"
#include "core/common/dlfcn.h"
#include "core/common/time.h"

namespace xdp {
namespace native {

bool load()
{
  static xrt_core::module_loader xdp_native_loader("xdp_native_plugin",
						   register_functions,
						   warning_function);
  return true ;
}

std::function<void (const char*, unsigned long long int)> function_start_cb ;
std::function<void (const char*, unsigned long long int, unsigned long long int)> function_end_cb ;
  
void register_functions(void* handle)
{
  typedef void (*ftype)(const char*, unsigned long long int) ;
  function_start_cb =
    (ftype)(xrt_core::dlsym(handle, "native_function_start")) ;
  if (xrt_core::dlerror() != nullptr)
    function_start_cb = nullptr ;

  typedef void (*etype)(const char*, unsigned long long int, unsigned long long int);
  function_end_cb = (etype)(xrt_core::dlsym(handle, "native_function_end")) ;
  if (xrt_core::dlerror() != nullptr)
    function_end_cb = nullptr ;
}

void warning_function()
{}

api_call_logger::
api_call_logger(const char* function)
  : m_funcid(0), m_fullname(function)
{
  // Since all api_call_logger objects exist inside the profiling_wrapper
  // we don't need to check the config reader here (it's done already)
  static bool s_load_native = load() ;

  if (function_start_cb && s_load_native) {
    m_funcid = xrt_core::utils::issue_id() ;
    function_start_cb(m_fullname, m_funcid) ;
  }
}

api_call_logger::
~api_call_logger()
{
  unsigned long long int timestamp =
    static_cast<unsigned long long int>(xrt_core::time_ns());

  if (function_end_cb) {
    function_end_cb(m_fullname, m_funcid, timestamp) ;
  }
}

} // end namespace native
} // end namespace xdp

