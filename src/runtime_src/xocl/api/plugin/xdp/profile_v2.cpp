/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#include <functional>

#include "plugin/xdp/profile_v2.h"

#include "core/common/module_loader.h"
#include "core/common/utils.h"
#include "core/common/dlfcn.h"
#include "core/common/config_reader.h"

// The anonymous namespace is responsible for loading the XDP plugins
namespace {

  // ******** OpenCL Counters/Guidance Plugin *********
  void register_opencl_counters_functions(void* handle)
  {
  }

  void opencl_counters_warning_function()
  {
  }

  void load_xdp_opencl_counters()
  {
    static xrt_core::module_loader
      opencl_trace_loader("xdp_opencl_counters_plugin",
			  register_opencl_counters_functions,
			  opencl_counters_warning_function) ;
  }

  // ******** OpenCL API Trace Plugin *********

  // All of the function pointers that will be dynamically linked to
  //  callback functions on the XDP plugin side
  std::function<void (const char*, uint64_t, uint64_t)> function_start_cb ;
  std::function<void (const char*, uint64_t, uint64_t)> function_end_cb ;

  void register_opencl_trace_functions(void* handle)
  {
    typedef void (*ftype)(const char*, uint64_t, uint64_t) ;
    function_start_cb = (ftype)(xrt_core::dlsym(handle, "function_start")) ;
    if (xrt_core::dlerror() != NULL) function_start_cb = nullptr ;

    function_end_cb = (ftype)(xrt_core::dlsym(handle, "function_end")) ;
    if (xrt_core::dlerror() != NULL) function_end_cb = nullptr ;
  }

  void opencl_trace_warning_function()
  {
    // No warnings currently
  }

  void load_xdp_opencl_trace()
  {
    static xrt_core::module_loader
      opencl_trace_loader("xdp_opencl_trace_plugin",
			  register_opencl_trace_functions,
			  opencl_trace_warning_function) ;
  }

  // ******** OpenCL Device Trace Plugin *********
  std::function<void (void*)> update_device_cb ;
  std::function<void (void*)> flush_device_cb ;

  void register_device_offload_functions(void* handle) 
  {
    typedef void (*ftype)(void*) ;
    update_device_cb = (ftype)(xrt_core::dlsym(handle, "updateDeviceOpenCL")) ;
    if (xrt_core::dlerror() != NULL) update_device_cb = nullptr ;

    flush_device_cb = (ftype)(xrt_core::dlsym(handle, "flushDeviceOpenCL")) ;
    if (xrt_core::dlerror() != NULL) flush_device_cb = nullptr ;
  }

  void device_offload_warning_function()
  {
    // No warnings at this level
  }

  void load_xdp_device_offload()
  {
    static xrt_core::module_loader 
      xdp_device_offload_loader("xdp_device_offload_plugin",
				register_device_offload_functions,
				device_offload_warning_function) ;
  }

} // end anonymous namespace

namespace xocl {
  namespace profile {

    // ******** OpenCL Counter/Guidance Callbacks *********

    // ******** OpenCL API Trace Callbacks *********
    OpenCLAPILogger::OpenCLAPILogger(const char* function) :
      OpenCLAPILogger(function, 0)
    {
    }

    OpenCLAPILogger::OpenCLAPILogger(const char* function, uint64_t address) :
      m_name(function), m_address(address)
    {
      // Use the OpenCL API logger as the hook to load all of the OpenCL
      //  level XDP plugins.  Once loaded, they are completely independent,
      //  but this provides us a common place where all OpenCL applications
      //  can safely load them.
      static bool s_load_detailed_profile = false ;
      if (!s_load_detailed_profile)
      {
	s_load_detailed_profile = true ;
	if (xrt_core::config::get_profile())
	  load_xdp_opencl_counters() ;
	if (xrt_core::config::get_timeline_trace())
	  load_xdp_opencl_trace() ;
	if (xrt_core::config::get_data_transfer_trace() != "off")
	  load_xdp_device_offload() ;
      }

      // Log the stats for this function
      m_funcid = xrt_core::utils::issue_id() ;
      if (function_start_cb)
	function_start_cb(m_name, m_address, m_funcid) ;
    }

    OpenCLAPILogger::~OpenCLAPILogger()
    {
      if (function_end_cb)
	function_end_cb(m_name, m_address, m_funcid) ;
    }

    // ******** OpenCL Device Trace Callbacks *********
    void flush_device(xrt::device* handle)
    {
      if (flush_device_cb)
	flush_device_cb(handle) ;
    }

    void update_device(xrt::device* handle)
    {
      if (update_device_cb)
	update_device_cb(handle) ;
    }

  } // end namespace profile
} // end namespace xocl
