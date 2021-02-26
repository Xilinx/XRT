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

#include "core/include/experimental/plugin/xdp/native_profile.h"
#include "core/common/module_loader.h"
#include "core/common/utils.h"
#include "core/common/dlfcn.h"
#include "core/common/config_reader.h"

#include <map>

#ifndef _WIN32
#include <cxxabi.h>
#endif

namespace {

  static std::string full_name(const char* type, const char* function)
  {
    if (type == nullptr) return function ;

#ifdef _WIN32
    std::string combine = type ;
#else
    int status = 0 ;
    std::string combined = abi::__cxa_demangle(type, nullptr, nullptr, &status);
    if (status != 0) combined = "" ;
#endif
    combined += "::" ;
    combined += function ;
    return combined ;
  }
} // end anonymous namespace

namespace xdpnative {

  void load_xdp_native()
  {
    static xrt_core::module_loader xdp_native_loader("xdp_native_plugin",
						     register_native_functions,
						     native_warning_function);
  }

  std::function<void (const char*, unsigned long long int)> function_start_cb ;
  std::function<void (const char*, unsigned long long int)> function_end_cb ;
  
  void register_native_functions(void* handle)
  {
    typedef void (*ftype)(const char*, unsigned long long int) ;
    function_start_cb = (ftype)(xrt_core::dlsym(handle, "native_function_start")) ;
    if (xrt_core::dlerror() != NULL) function_start_cb = nullptr ;

    function_end_cb = (ftype)(xrt_core::dlsym(handle, "native_function_end")) ;
    if (xrt_core::dlerror() != NULL) function_end_cb = nullptr ;
  }

  void native_warning_function()
  {
  }

  NativeFunctionCallLogger::NativeFunctionCallLogger(const char* function, const char* type) :
    m_name(function), m_type(type)
  {
    static bool s_load_native = false ;
    if (!s_load_native) {
      s_load_native = true ;
      if (xrt_core::config::get_native_xrt_trace())
	load_xdp_native() ;
    }

    m_funcid = xrt_core::utils::issue_id() ;
    if (function_start_cb) {
      if (m_type != nullptr)
	function_start_cb(full_name(m_type, m_name).c_str(), m_funcid) ;
      else
	function_start_cb(m_name, m_funcid) ;
    }
  }

  NativeFunctionCallLogger::~NativeFunctionCallLogger()
  {
    if (function_end_cb) {
      if (m_type != nullptr)
	function_end_cb(full_name(m_type, m_name).c_str(), m_funcid) ;
      else
	function_end_cb(m_name, m_funcid) ;
    }
  }

  static std::map<void*, unsigned int> storage;

  void profiling_start(void* object, const char* function, const char* type)
  {
    if (function_start_cb) {
      auto id = xrt_core::utils::issue_id() ;

      storage[object] = id ;
      
      std::string combined = full_name(type, function) ;
      function_start_cb(combined.c_str(), id) ;
    }
  }

  void profiling_end(void* object, const char* function, const char* type)
  {
    if (function_end_cb) {
      auto id = storage[object] ;

      std::string combined = full_name(type, function) ;
      function_end_cb(combined.c_str(), id) ;
    }
  }

} // end namespace xdpnative

