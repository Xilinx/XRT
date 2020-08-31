/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

// This file implements XRT kernel APIs as declared in 
// core/include/experimental/xrt_profile.h

#include <functional>
#include <mutex>

#define XCL_DRIVER_DLL_EXPORT 
#define XRT_CORE_COMMON_SOURCE
#include "core/include/experimental/xrt_profile.h"

#include "core/common/module_loader.h"
#include "core/common/utils.h"
#include "core/common/dlfcn.h"
#include "core/common/message.h"

namespace xrt { namespace profile {

  user_range::user_range(const std::string& label,
			 const std::string& tooltip) : active(true)
  {
    id = static_cast<uint32_t>(xrt_core::utils::issue_id()) ;

    xrtURStart(id, label.c_str(), tooltip.c_str()) ;
  }

  user_range::user_range() : id(0), active(false)
  {
  }

  user_range::~user_range()
  {
    if (active) xrtUREnd(id) ;
  }

  void user_range::start(const std::string& label, 
			 const std::string& tooltip)
  {
    // Handle case where start is called while started
    if (active) xrtUREnd(id) ;

    id = static_cast<uint32_t>(xrt_core::utils::issue_id()) ;
    xrtURStart(id, label.c_str(), tooltip.c_str()) ;
    active = true ;
  }

  void user_range::end()
  {
    // Handle case when end when not tracking time
    if (!active) return ;

    xrtUREnd(id) ;
    active = false ;
  }

  user_event::user_event()
  {
  }

  user_event::~user_event()
  {
  }

  void user_event::mark(const char* label)
  {
    xrtUEMark(label) ;
  }

}} // end namespaces profile and xrt


// Anonymous namespace for dynamic loading and connection
namespace {
  
  std::function<void (unsigned int, const char*, const char*)> user_range_start_cb ;
  std::function<void (unsigned int)> user_range_end_cb ;
  std::function<void (const char*)> user_event_cb ;

  static void register_user_functions(void* handle)
  {
    typedef void (*dtype)(unsigned int, const char*, const char*) ;
    user_range_start_cb = (dtype)(xrt_core::dlsym(handle, "user_event_start_cb"));
    if (xrt_core::dlerror() != NULL) user_range_start_cb = nullptr ;

    typedef void (*ftype)(unsigned int) ;
    user_range_end_cb = (ftype)(xrt_core::dlsym(handle, "user_event_end_cb")) ;
    if (xrt_core::dlerror() != NULL) user_range_end_cb = nullptr ;

    typedef void (*btype)(const char*) ;
    user_event_cb = (btype)(xrt_core::dlsym(handle, "user_event_happened_cb")) ;
    if (xrt_core::dlerror() != NULL) user_event_cb = nullptr ;
  }

  static void load_user_profiling_plugin()
  {
    static xrt_core::module_loader user_event_loader("xdp_user_plugin",
						     register_user_functions,
						     nullptr) ;
  }

} // end anonymous

extern "C"
{
  void xrtURStart(unsigned int id, const char* label, const char* tooltip) 
  {
    try {
      load_user_profiling_plugin() ;
      if (user_range_start_cb != nullptr) 
	user_range_start_cb(id, label, tooltip) ;
    }
    catch(const std::exception& ex)
    {
      xrt_core::message::send(xrt_core::message::severity_level::XRT_ERROR,
			      "XRT",
			      ex.what()) ;
    }
  }

  void xrtUREnd(unsigned int id)
  {
    try {
      load_user_profiling_plugin() ;
      if (user_range_end_cb != nullptr) 
	user_range_end_cb(id);
    }
    catch(const std::exception& ex)
    {
      xrt_core::message::send(xrt_core::message::severity_level::XRT_ERROR,
			      "XRT",
			      ex.what()) ;
    }
  }

  void xrtUEMark(const char* label)
  {
    try {
      load_user_profiling_plugin() ;
      if (user_event_cb != nullptr) 
	user_event_cb(label) ;
    }
    catch(const std::exception& ex)
    {
      xrt_core::message::send(xrt_core::message::severity_level::XRT_ERROR,
			      "XRT",
			      ex.what()) ;
    }
  }
}
