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

/**
 * This file contains the API for adapting the mixed xcl/xocl
 * data structures to the profiling infrastructure.
 *
 * Once xcl has been eliminated, this file should move to xocl/core
 * Temporarily, the file abuses the xocl namespace, but it cannot
 * currently be moved since profile.cpp has xcl dependencies that
 * are strictly forbidden in xocl.
 */

#include "xocl/core/event.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/device.h"
#include "xocl/core/kernel.h"
#include "xocl/core/context.h"
#include "xocl/core/program.h"
#include "xocl/core/range.h"
#include "xocl/core/execution_context.h"

#include "xocl/xclbin/xclbin.h"

#include <map>
#include <sstream>
#include "plugin/xdp/profile.h"

namespace {

// Global flag that remains valid during shutdown
static bool s_exiting = false;

// Exiter that is constructed after (=> destructured before)
// critical data structures that should disable profiling.
struct X
{
  ~X() { s_exiting = true; }
};

} // namespace

namespace xocl { namespace profile {

/*
 * callback functions called from within action_ lambdas
*/
cb_action_ndrange_type cb_action_ndrange;
cb_action_read_type cb_action_read;
cb_action_map_type cb_action_map;
cb_action_write_type cb_action_write;
cb_action_unmap_type cb_action_unmap ;
cb_action_ndrange_migrate_type cb_action_ndrange_migrate;
cb_action_migrate_type cb_action_migrate;

/*
 * callback functions called for function logging and dependencies...
 */
cb_log_function_start_type cb_log_function_start;
cb_log_function_end_type cb_log_function_end;
cb_log_dependencies_type cb_log_dependencies;
cb_add_to_active_devices_type cb_add_to_active_devices;
cb_set_kernel_clock_freq_type cb_set_kernel_clock_freq;
cb_reset_type cb_reset;
cb_init_type cb_init;

/*
 * callback functions to implementation in Profiling
 */
cb_get_device_trace_type cb_get_device_trace;
cb_get_device_counters_type cb_get_device_counters;
cb_start_device_profiling_type cb_start_device_profiling;
cb_reset_device_profiling_type cb_reset_device_profiling;
cb_end_device_profiling_type cb_end_device_profiling;


/*
 * callback registration functions used by lambda generators called from profile
*/
void register_cb_action_ndrange (cb_action_ndrange_type&& cb)
{
  cb_action_ndrange = std::move(cb);
}
void register_cb_action_read  (cb_action_read_type&& cb)
{
  cb_action_read = std::move(cb);
}
void register_cb_action_map (cb_action_map_type&& cb)
{
  cb_action_map = std::move(cb);
}
void register_cb_action_write (cb_action_write_type&& cb)
{
  cb_action_write = std::move(cb);
}
void register_cb_action_unmap (cb_action_unmap_type&& cb)
{
  cb_action_unmap  = std::move(cb);
}
void register_cb_action_ndrange_migrate (cb_action_ndrange_migrate_type&& cb)
{
  cb_action_ndrange_migrate = std::move(cb);
}
void register_cb_action_migrate (cb_action_migrate_type&& cb)
{
  cb_action_migrate = std::move(cb);
}

void register_cb_log_function_start (cb_log_function_start_type&& cb)
{
  cb_log_function_start = std::move(cb);
}

void register_cb_log_function_end (cb_log_function_end_type&& cb)
{
  cb_log_function_end = std::move(cb);
}

void register_cb_log_dependencies(cb_log_dependencies_type&& cb)
{
  cb_log_dependencies = std::move(cb);
}

void register_cb_add_to_active_devices(cb_add_to_active_devices_type&& cb)
{
  cb_add_to_active_devices = std::move(cb);
}

void register_cb_set_kernel_clock_freq (cb_set_kernel_clock_freq_type&& cb)
{
  cb_set_kernel_clock_freq = std::move(cb);
}

void register_cb_reset (cb_reset_type && cb)
{
  cb_reset = std::move(cb);
}

void register_cb_init (cb_init_type && cb)
{
  cb_init = std::move(cb);
}

/*
 * callbacks to functions in "Profiling"
 */
void register_cb_get_device_trace (cb_get_device_trace_type&& cb)
{
  cb_get_device_trace = std::move(cb);
}
void register_cb_get_device_counters (cb_get_device_counters_type&& cb)
{
  cb_get_device_counters = std::move(cb);
}
void register_cb_start_device_profiling (cb_start_device_profiling_type&& cb)
{
  cb_start_device_profiling = std::move(cb);
}

void register_cb_reset_device_profiling(cb_reset_device_profiling_type&& cb)
{
  cb_reset_device_profiling = std::move(cb);
}

void register_cb_end_device_profiling(cb_end_device_profiling_type&& cb)
{
  cb_end_device_profiling = std::move(cb);
}

void
log(xocl::event* event, cl_int status)
{
  if (!s_exiting)
    event->trigger_profile_action(status,"");
}

void
log(xocl::event* event, cl_int status, const std::string& cuname)
{
  if (!s_exiting)
    event->trigger_profile_action(status,cuname);
}

void
log_dependencies (xocl::event* event,  cl_uint num_deps, const cl_event* deps)
{
  if(cb_log_dependencies)
    cb_log_dependencies(event, num_deps, deps);
}

// Attempt to get DDR physical address and bank number
void get_address_bank(cl_mem buffer, uint64_t &address, std::string &bank)
{
  address = 0;
  bank = "Unknown";
  try {
    if (auto xmem=xocl::xocl(buffer))
      xmem->try_get_address_bank(address, bank);
    return;
  }
  catch (const xocl::error& ex) {
  }
}

xocl::event::action_profile_type
action_ndrange(cl_event event, cl_kernel kernel)
{
  // profile action is invoked after the event is marked complete and
  // at that time the kenel may have been released by a subsequent
  // clReleaseKernel.
  auto exctx = xocl::xocl(event)->get_execution_context();
  auto workGroupSize = xocl::xocl(kernel)->get_wg_size();
  auto globalWorkDim = exctx->get_global_work_size();
  size_t localWorkDim[3] = {0,0,0};
  range_copy(xocl::xocl(kernel)->get_compile_wg_size_range(),localWorkDim);
  if (localWorkDim[0] == 0 && localWorkDim[1] == 0 && localWorkDim[2] == 0) {
    std::copy(exctx->get_local_work_size(),exctx->get_local_work_size()+3,localWorkDim);
  }

  // Leg work to access the xclbin project name.  The device may have
  // been reloaded with a new binary by the time the action itself is
  // called, so the work has be done here.
  auto device = xocl::xocl(event)->get_command_queue()->get_device();
  auto program = xocl::xocl(kernel)->get_program();
  auto programId = xocl::xocl(kernel)->get_program()->get_uid();
  auto xclbin = program->get_xclbin(device);

  std::string xname = xclbin.project_name();
  std::string kname  = xocl::xocl(kernel)->get_name();

  return [kernel,kname,xname,workGroupSize,globalWorkDim,localWorkDim,programId](xocl::event* ev,cl_int status,const std::string& cu_name) {
    if (cb_action_ndrange)
      cb_action_ndrange(ev, status, cu_name, kernel, kname, xname, workGroupSize, globalWorkDim, localWorkDim, programId);
  };
}

xocl::event::action_profile_type
action_read(cl_mem buffer, size_t user_offset, size_t user_size, bool entire_buffer)
{
  std::string bank;
  uint64_t address;
  get_address_bank(buffer, address, bank);
  auto size = xocl::xocl(buffer)->get_size();

  return [buffer,size,address,bank,user_offset,user_size,entire_buffer](xocl::event* event,cl_int status, const std::string&) {
    if (cb_action_read)
      cb_action_read(event, status, buffer, size, address, bank, entire_buffer, user_size, user_offset);
  };
}

xocl::event::action_profile_type
action_map(cl_mem buffer,cl_map_flags map_flags)
{
  std::string bank;
  uint64_t address;
  get_address_bank(buffer, address, bank);
  auto size = xocl::xocl(buffer)->get_size();

  return [buffer,size,address,bank,map_flags](xocl::event* event,cl_int status,const std::string&) {
    if (cb_action_map)
      cb_action_map(event, status, buffer, size, address, bank, map_flags);
  };
}

xocl::event::action_profile_type
action_write(cl_mem buffer)
{
  std::string bank;
  uint64_t address;
  get_address_bank(buffer, address, bank);
  auto size = xocl::xocl(buffer)->get_size();

  return [buffer,size,address,bank](xocl::event* event,cl_int status,const std::string&) {
    if (cb_action_write)
      cb_action_write(event, status, buffer, size, address, bank);
  };
}

xocl::event::action_profile_type
action_unmap(cl_mem buffer)
{
  std::string bank;
  uint64_t address;
  get_address_bank(buffer, address, bank);
  auto size = xocl::xocl(buffer)->get_size();

  return [buffer,size,address,bank](xocl::event* event,cl_int status,const std::string&) {
    if (cb_action_unmap)
      cb_action_unmap(event, status, buffer, size, address, bank);
  };
}

xocl::event::action_profile_type
action_ndrange_migrate(cl_event event, cl_kernel kernel)
{
  cl_mem mem0 = nullptr;
  std::string bank = "Unknown";
  uint64_t address = 0;
  size_t totalSize = 0;

  auto command_queue = xocl::xocl(event)->get_command_queue();
  auto device = command_queue->get_device();

  // Calculate total size and grab first address & bank
  // NOTE: argument must be: NOT a progvar, NOT write only, and NOT resident
  for (auto& arg : xocl::xocl(kernel)->get_argument_range()) {
    if (auto mem = arg->get_memory_object()) {
      if (arg->is_progvar() && arg->get_address_qualifier()==CL_KERNEL_ARG_ADDRESS_GLOBAL)
        // DO NOTHING: progvars are not transfered
        continue;
      else if (mem->is_resident(device))
        continue;
      else if (!(mem->get_flags() & (CL_MEM_WRITE_ONLY|CL_MEM_HOST_NO_ACCESS))) {
        if (totalSize == 0) {
          mem0 = mem;
          get_address_bank(mem, address, bank);
        }

        totalSize += xocl::xocl(mem)->get_size();
      }
    }
  }

  return [mem0,totalSize,address,bank](xocl::event* ev,cl_int status,const std::string&) {
    if (cb_action_ndrange_migrate)
      cb_action_ndrange_migrate(ev, status, mem0, totalSize, address, bank);
  };
}

xocl::event::action_profile_type
action_migrate(cl_uint num_mem_objects, const cl_mem *mem_objects, cl_mem_migration_flags flags)
{
  // profile action is invoked after the event is marked complete and
  // at that time the buffer may have been released by a subsequent
  // clReleaseMemObject
  cl_mem mem0 = (num_mem_objects > 0) ? mem_objects[0] : nullptr;

  std::string bank;
  uint64_t address;
  get_address_bank(mem0, address, bank);

  size_t totalSize = 0;
  for (auto mem : xocl::get_range(mem_objects,mem_objects+num_mem_objects)) {
    totalSize += xocl::xocl(mem)->get_size();
  }

  return [mem0,totalSize,address,bank,flags](xocl::event* event,cl_int status,const std::string&) {
    if (cb_action_migrate)
      cb_action_migrate(event, status, mem0, totalSize, address, bank, flags);
  };
}

function_call_logger::
function_call_logger(const char* function)
  : function_call_logger(function,0)
{}

function_call_logger::
function_call_logger(const char* function, long long address)
  : m_name(function), m_address(address)
{
  static bool s_load_xdp = false;

  //If this is the first API called, then we should attempt loading dll
  //This call here should occur just once per application run
  if (!s_load_xdp) {
    s_load_xdp = true;
    if (xrt::config::get_app_debug() || xrt::config::get_profile()) {
      xrt::hal::load_xdp();
    }
  }

  m_funcid = m_funcid_global++;
  if (cb_log_function_start)
    cb_log_function_start(m_name, m_address, m_funcid);
}

function_call_logger::
~function_call_logger()
{
  if (cb_log_function_end)
    cb_log_function_end(m_name, m_address, m_funcid);
}

std::atomic <unsigned int>  function_call_logger::m_funcid_global(0);

void add_to_active_devices(const std::string& device_name)
{
  if (cb_add_to_active_devices)
   cb_add_to_active_devices(device_name);
}

void
set_kernel_clock_freq(const std::string& device_name, unsigned int freq)
{
  if (cb_set_kernel_clock_freq)
    cb_set_kernel_clock_freq(device_name, freq);
}

void
reset(const xocl::xclbin& xclbin)
{
  if (cb_reset)
    cb_reset(xclbin);
}

void
init()
{
  /*
   Checking for s_exiting doesn't really help. There is no gurantee the order in which
   the static objects get claimed when program is exiting. So we don't really need X
   TODO: Clean this up.
  */
  static X x;

  if (cb_init)
    cb_init();
}

void get_device_trace (bool forceReadTrace)
{
  if (cb_get_device_trace)
    cb_get_device_trace (forceReadTrace);
}

void get_device_counters (bool firstReadAfterProgram, bool forceReadCounters)
{
  if (cb_get_device_counters)
    cb_get_device_counters(firstReadAfterProgram, forceReadCounters);
}

void start_device_profiling(size_t numComputeUnits)
{
  if (cb_start_device_profiling)
    cb_start_device_profiling(numComputeUnits);
}
void reset_device_profiling()
{
  if (cb_reset_device_profiling)
    cb_reset_device_profiling();
}
void end_device_profiling()
{
  if (cb_end_device_profiling)
    cb_end_device_profiling();
}

}}
