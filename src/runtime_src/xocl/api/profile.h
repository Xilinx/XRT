/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

namespace xocl { 

class xclbin;

namespace profile {

bool isProfilingOn();

void get_address_bank(cl_mem buffer, uint64_t &address, int &bank);

std::string
get_event_string(xocl::event* currEvent);

std::string
get_event_dependencies_string(xocl::event* currEvent);

xocl::event::action_profile_type
action_ndrange(cl_event event,cl_kernel kernel);

xocl::event::action_profile_type
action_read(cl_mem buffer);

xocl::event::action_profile_type
action_map(cl_mem buffer,cl_map_flags map_flags);

xocl::event::action_profile_type
action_write(cl_mem buffer);

xocl::event::action_profile_type
action_unmap(cl_mem buffer);

xocl::event::action_profile_type
action_ndrange_migrate(cl_event event, cl_kernel kernel);

xocl::event::action_profile_type
action_migrate(cl_uint num_mem_objects, const cl_mem *mem_objects, cl_mem_migration_flags flags);

template <typename F, typename ...Args>
inline void
set_event_action(xocl::event* event, F&& f, Args&&... args)
{
  // if profiling is off, then avoid creating the lambdas
#if 0 // For the time being, to preserve old log profile data behavior, 
      // always store the profile action, see comments in xocl/core/event.h
  if (!event->get_command_queue()->is_profiling_enabled())
    return;
#endif

  event->set_profile_action(f(std::forward<Args>(args)...));
}

void
log(xocl::event* event, cl_int status);

void
log(xocl::event* event, cl_int status, const std::string& cuname);

void
log_dependencies (xocl::event* event,  cl_uint num_deps, const cl_event* deps);

struct function_call_logger
{
  function_call_logger(const char* function);
  function_call_logger(const char* function, long long address);
  ~function_call_logger();

  const char* m_name = nullptr;
  long long m_address = 0;
};

void
add_to_active_devices(const std::string& device_name);

void
set_kernel_clock_freq(const std::string& device_name, unsigned int freq);

void
reset(const xocl::xclbin& xclbin);

/**
 * Initialize profiling (was RTSignleton::instance() blah blah
 */
void 
init();

}} // profile,xocl

#define PROFILE_LOG_FUNCTION_CALL xocl::profile::function_call_logger function_call_logger_object(__func__);
#define PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(Q) xocl::profile::function_call_logger function_call_logger_object(__func__, (long long)Q);

#endif


