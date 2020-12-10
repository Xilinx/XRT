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

#ifndef xocl_api_profile_h
#define xocl_api_profile_h

/**
 * This file contains the API for adapting the xocl
 * data structures to the profiling infrastructure.
 *
 * The implementation of this API still requires old "xcl" data
 * hence profile.cpp currently lives under runtime_src/api/profile.cpp
 */

#include "xocl/core/object.h"
#include "xocl/core/event.h"
#include "xocl/core/command_queue.h"
#include <utility>
#include <string>

struct axlf;

namespace xocl { 

namespace profile {

/*
 * callback function types called from within action_ lambdas
*/
using cb_action_ndrange_type = std::function<void (xocl::event* event,cl_int status,const std::string& cu_name, cl_kernel kernel,
                                                   std::string kname, std::string xname, size_t workGroupSize,
                                                   const size_t* globalWorkDim, const size_t* localWorkDim, unsigned int programId)>;
using cb_action_read_type = std::function<void (xocl::event* event,cl_int status, cl_mem buffer, size_t size,
                                          uint64_t address, const std::string& bank, size_t user_offset, size_t user_size, bool entire_buffer)>;
using cb_action_map_type = std::function<void (xocl::event* event,cl_int status, cl_mem buffer, size_t size,
                                               uint64_t address, const std::string& bank, cl_map_flags map_flags)>;
using cb_action_write_type = std::function<void (xocl::event* event,cl_int status, cl_mem buffer, size_t size,
                                           uint64_t address, const std::string& bank, size_t user_offset, size_t user_size, bool entire_buffer)>;
using cb_action_unmap_type = std::function<void (xocl::event* event,cl_int status, cl_mem buffer, size_t size,
                                                 uint64_t address, const std::string& bank)>;
using cb_action_ndrange_migrate_type = std::function <void (xocl::event* event,cl_int status, cl_mem mem0,
                                                      size_t totalSize, uint64_t address, const std::string & bank)>;
using cb_action_migrate_type = std::function< void (xocl::event* event,cl_int status, cl_mem mem0, size_t totalSize, uint64_t address,
                                                    const std::string & bank, cl_mem_migration_flags flags)>;
using cb_action_copy_type = std::function< void (xocl::event* event, cl_int status, cl_mem src_buffer, cl_mem dst_buffer,
                                                 bool same_device, size_t size, uint64_t srcAddress, const std::string& srcBank,
                                                 uint64_t dstAddress, const std::string& dstBank)>;

/*
 * callback function types for function logging, dependency ...
 */

using cb_log_function_start_type = std::function<void(const char* functionName, long long queueAddress, unsigned int functionID)>;
using cb_log_function_end_type = std::function<void(const char* functionName, long long queueAddress, unsigned int functionID)>;
using cb_log_dependencies_type = std::function<void(xocl::event* event,  cl_uint num_deps, const cl_event* deps)>;
using cb_add_to_active_devices_type = std::function<void (const std::string& device_name)>;
using cb_set_kernel_clock_freq_type = std::function<void(const std::string& device_name, unsigned int freq)>;
using cb_reset_type = std::function<void(const axlf*)>;
using cb_init_type = std::function<void(void)>;

/*
 * callback functions to implementation in Profiling
 */
using cb_get_device_trace_type = std::function<void(bool forceReadTrace)>;
using cb_get_device_counters_type = std::function<void(bool firstReadAfterProgram, bool forceReadCounters)>;
using cb_start_device_profiling_type = std::function<void( size_t numComputeUnit)>;
using cb_reset_device_profiling_type = std::function<void(void)>;
using cb_end_device_profiling_type = std::function<void(void)>;

/*
 * callback registration functions called from profile
*/
XRT_XOCL_EXPORT void register_cb_action_ndrange (cb_action_ndrange_type&& cb);
XRT_XOCL_EXPORT void register_cb_action_read  (cb_action_read_type&& cb);
XRT_XOCL_EXPORT void register_cb_action_map (cb_action_map_type&& cb);
XRT_XOCL_EXPORT void register_cb_action_write (cb_action_write_type&& cb);
XRT_XOCL_EXPORT void register_cb_action_unmap (cb_action_unmap_type&& cb);
XRT_XOCL_EXPORT void register_cb_action_ndrange_migrate (cb_action_ndrange_migrate_type&& cb);
XRT_XOCL_EXPORT void register_cb_action_migrate (cb_action_migrate_type&& cb);
XRT_XOCL_EXPORT void register_cb_action_copy (cb_action_copy_type&& cb);

XRT_XOCL_EXPORT void register_cb_log_function_start (cb_log_function_start_type&& cb);
XRT_XOCL_EXPORT void register_cb_log_function_end (cb_log_function_end_type&& cb);
XRT_XOCL_EXPORT void register_cb_log_dependencies(cb_log_dependencies_type && cb);
XRT_XOCL_EXPORT void register_cb_add_to_active_devices(cb_add_to_active_devices_type&& cb);
XRT_XOCL_EXPORT void register_cb_set_kernel_clock_freq (cb_set_kernel_clock_freq_type&& cb);
XRT_XOCL_EXPORT void register_cb_reset(cb_reset_type && cb);
XRT_XOCL_EXPORT void register_cb_init (cb_init_type && cb);

XRT_XOCL_EXPORT void register_cb_get_device_trace (cb_get_device_trace_type&& cb);
XRT_XOCL_EXPORT void register_cb_get_device_counters (cb_get_device_counters_type&& cb);
XRT_XOCL_EXPORT void register_cb_start_device_profiling (cb_start_device_profiling_type&& cb);
XRT_XOCL_EXPORT void register_cb_reset_device_profiling (cb_reset_device_profiling_type&& cb);
XRT_XOCL_EXPORT void register_cb_end_device_profiling (cb_end_device_profiling_type&& cb);

XRT_XOCL_EXPORT void get_address_bank(cl_mem buffer, uint64_t &address, int &bank);
XRT_XOCL_EXPORT bool is_same_device(cl_mem buffer1, cl_mem buffer2);

XRT_XOCL_EXPORT std::string
get_event_string(xocl::event* currEvent);

XRT_XOCL_EXPORT std::string
get_event_dependencies_string(xocl::event* currEvent);

XRT_XOCL_EXPORT xocl::event::action_profile_type
action_ndrange(cl_event event,cl_kernel kernel);

XRT_XOCL_EXPORT xocl::event::action_profile_type
action_read(cl_mem buffer, size_t offset, size_t size, bool entire_buffer);

XRT_XOCL_EXPORT xocl::event::action_profile_type
action_map(cl_mem buffer,cl_map_flags map_flags);

XRT_XOCL_EXPORT xocl::event::action_profile_type
action_write(cl_mem buffer, size_t offset, size_t size, bool entire_buffer);

XRT_XOCL_EXPORT xocl::event::action_profile_type
action_unmap(cl_mem buffer);

XRT_XOCL_EXPORT xocl::event::action_profile_type
action_ndrange_migrate(cl_event event, cl_kernel kernel);

XRT_XOCL_EXPORT xocl::event::action_profile_type
action_migrate(cl_uint num_mem_objects, const cl_mem *mem_objects, cl_mem_migration_flags flags);

XRT_XOCL_EXPORT xocl::event::action_profile_type
action_copy(cl_mem src_buffer, cl_mem dst_buffer, size_t src_offset, size_t dst_offset, size_t size, bool same_device);

template <typename F, typename ...Args>
inline void
set_event_action(xocl::event* event, F&& f, Args&&... args)
{
  if (xrt_xocl::config::get_profile())
    event->set_profile_action(f(std::forward<Args>(args)...));
}

XRT_XOCL_EXPORT void
log(xocl::event* event, cl_int status);

XRT_XOCL_EXPORT void
log(xocl::event* event, cl_int status, const std::string& cuname);

XRT_XOCL_EXPORT void
log_dependencies (xocl::event* event,  cl_uint num_deps, const cl_event* deps);

struct function_call_logger
{
  function_call_logger(const char* function);
  function_call_logger(const char* function, long long address);
  ~function_call_logger();

  static std::atomic <unsigned int> m_funcid_global;
  unsigned int m_funcid;
  const char* m_name = nullptr;
  long long m_address = 0;
};

XRT_XOCL_EXPORT void
add_to_active_devices(const std::string& device_name);

XRT_XOCL_EXPORT void
set_kernel_clock_freq(const std::string& device_name, unsigned int freq);

XRT_XOCL_EXPORT void
reset(const axlf* xclbin);

/**
 * Initialize profiling (was RTSignleton::instance() blah blah
 */
XRT_XOCL_EXPORT void 
init();

XRT_XOCL_EXPORT void
get_device_trace (bool forceReadTrace);

XRT_XOCL_EXPORT void
get_device_counters (bool firstReadAfterProgram, bool forceReadCounters);

XRT_XOCL_EXPORT void
start_device_profiling(size_t numComputeUnits);

XRT_XOCL_EXPORT void
reset_device_profiling();

XRT_XOCL_EXPORT void
end_device_profiling();

}} // profile,xocl

#define PROFILE_LOG_FUNCTION_CALL xocl::profile::function_call_logger function_call_logger_object(__func__);
#define PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(Q) xocl::profile::function_call_logger function_call_logger_object(__func__, (long long)Q);

#endif


