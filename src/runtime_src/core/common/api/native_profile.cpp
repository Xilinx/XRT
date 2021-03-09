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

// Anonymous namespace for helper functions
namespace {

static std::string full_name(const char* type, const char* function)
{
  if (type == nullptr) return function ;
  
  std::string combined = type ;
  combined += "::" ;
  combined += function ;
  return combined ;
}

} // end anonymous namespace

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
std::function<void (const char*, unsigned long long int)> function_end_cb ;
  
void register_functions(void* handle)
{
  typedef void (*ftype)(const char*, unsigned long long int) ;
  function_start_cb =
    (ftype)(xrt_core::dlsym(handle, "native_function_start")) ;
  if (xrt_core::dlerror() != nullptr)
    function_start_cb = nullptr ;

  function_end_cb = (ftype)(xrt_core::dlsym(handle, "native_function_end")) ;
  if (xrt_core::dlerror() != nullptr)
    function_end_cb = nullptr ;
}

void warning_function()
{}

api_call_logger::
api_call_logger(const char* function, const char* type)
  : m_funcid(0), m_name(function), m_type(type)
{
  // Since all api_call_logger objects exist inside the profiling_wrapper
  // we don't need to check the config reader here (it's done already)
  static bool s_load_native = load() ;

  if (function_start_cb && s_load_native) {
    m_funcid = xrt_core::utils::issue_id() ;
    if (m_type != nullptr)
      function_start_cb(full_name(m_type, m_name).c_str(), m_funcid) ;
    else
      function_start_cb(m_name, m_funcid) ;
  }
}

api_call_logger::
~api_call_logger()
{
  if (function_end_cb) {
    if (m_type != nullptr)
      function_end_cb(full_name(m_type, m_name).c_str(), m_funcid) ;
    else
      function_end_cb(m_name, m_funcid) ;
  }
}

} // end namespace native
} // end namespace xdp

