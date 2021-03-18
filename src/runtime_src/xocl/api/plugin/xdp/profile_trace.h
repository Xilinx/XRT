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

#ifndef PROFILE_TRACE_DOT_H
#define PROFILE_TRACE_DOT_H

#include "xocl/core/device.h"
#include "xocl/core/event.h"

namespace xdp {

namespace opencl_trace {
  void load() ;
} // end namespace opencl_trace

namespace device_offload {
  void load() ;
} // end namespace device_offload

} // end namespace xdp

namespace xocl {
  namespace profile {

    // Trace support for task that go through the command queues
    template <typename F, typename ...Args>
    inline void
    set_event_action(xocl::event* e, F&& f, Args&&... args)
    {
      if (xrt_core::config::get_timeline_trace() ||
	  xrt_core::config::get_opencl_trace())
	e->set_profile_action(f(std::forward<Args>(args)...)) ;
    }

    // Class for logging all OpenCL APIs
    class OpenCLAPILogger 
    {
    private:
      uint64_t m_funcid ;
      const char* m_name = nullptr ;
      uint64_t m_address = 0 ;
    public:
      OpenCLAPILogger(const char* function) ;
      OpenCLAPILogger(const char* function, uint64_t address) ;
      ~OpenCLAPILogger() ;
    } ;

    // Functions used by the host trace plugin
    void log_dependency(uint64_t id, uint64_t dependency) ;
    std::function<void (xocl::event*, cl_int, const std::string&)>
      action_read(cl_mem buffer) ;
    std::function<void (xocl::event*, cl_int, const std::string&)>
      action_write(cl_mem buffer) ;
    std::function<void (xocl::event*, cl_int, const std::string&)>
      action_map(cl_mem buffer, cl_map_flags flags) ;
    std::function<void (xocl::event*, cl_int, const std::string&)>
      action_migrate(cl_mem mem0, cl_mem_migration_flags flags) ;
    std::function<void (xocl::event*, cl_int, const std::string&)>
      action_ndrange_migrate(cl_event event, cl_kernel kernel) ;
    std::function<void (xocl::event*, cl_int, const std::string&)>
      action_ndrange(cl_event event, cl_kernel kernel) ;
    std::function<void (xocl::event*, cl_int, const std::string&)>
      action_unmap(cl_mem buffer) ;
    std::function<void (xocl::event*, cl_int, const std::string&)>
      action_copy(cl_mem src_buffer, cl_mem dst_buffer) ;

    // Functions used by the device trace plugin
    void flush_device(xrt_xocl::device* handle) ;
    void update_device(xrt_xocl::device* handle) ;

  } // end namespace profile

} // end namespace xocl

#define PROFILE_LOG_FUNCTION_CALL \
xocl::profile::OpenCLAPILogger profileObject(__func__);
#define PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(Q) \
xocl::profile::OpenCLAPILogger profileObject(__func__, (uint64_t)(Q));

#endif
