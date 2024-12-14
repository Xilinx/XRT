/**
 * Copyright (C) 2016-2022 Xilinx, Inc
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

#ifndef PROFILE_COUNTERS_DOT_H
#define PROFILE_COUNTERS_DOT_H

#include "xocl/core/event.h"
#include "xocl/core/execution_context.h"
#include "core/include/xrt/xrt_kernel.h"

namespace xocl {

  namespace profile {

     namespace counters {
      template<typename F, typename ...Args>
      inline void
      set_event_action(xocl::event* e, F&& f, Args&&... args)
      {
	if (xrt_core::config::get_opencl_trace() ||
            xrt_core::config::get_host_trace())
	  e->set_profile_counter_action(f(std::forward<Args>(args)...)) ;
      }
     } // end namespace counters

    // Functions used by the host counters plugin
    std::function<void (xocl::event*, cl_int)>
      counter_action_ndrange(cl_kernel kernel) ;
    std::function<void (xocl::event*, cl_int)>
      counter_action_read(cl_mem buffer) ;
    std::function<void (xocl::event*, cl_int)>
      counter_action_write(cl_mem buffer) ;
    std::function<void (xocl::event*, cl_int)>
      counter_action_migrate(cl_uint num_mem_objects, const cl_mem* mem_objects, cl_mem_migration_flags flags) ;
    std::function<void (xocl::event*, cl_int)>
      counter_action_ndrange_migrate(cl_event event, cl_kernel kernel);
    std::function<void (xocl::event*, cl_int)>
      counter_action_map(cl_mem buffer, cl_map_flags flags);
    std::function<void (xocl::event*, cl_int)>
      counter_action_unmap(cl_mem buffer) ;

    void log_cu_start(const xocl::execution_context* ctx,
		      const xrt::run& run) ;
    void log_cu_end(const xocl::execution_context* ctx,
		    const xrt::run& run); 
    void mark_objects_released() ;

    // Functions for loading the counters plugin
    void register_opencl_counters_functions(void* handle) ;
    void opencl_counters_warning_function() ;
    void load_xdp_opencl_counters() ;

    // Extern definitions of objects needed in the API callback hooks
    extern std::function<void (const char*, unsigned long long int, bool)> counter_function_start_cb ;
    extern std::function<void (const char*)> counter_function_end_cb ;

  } // end namespace profile

} // end namespace xocl

#endif
