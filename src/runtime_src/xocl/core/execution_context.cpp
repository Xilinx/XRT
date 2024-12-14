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

#include "execution_context.h"
#include "device.h"
#include "event.h"
#include "xrt/detail/ert.h"

#include "core/common/xclbin_parser.h"
#include "core/common/api/kernel_int.h"
#include "core/include/xrt/experimental/xrt_xclbin.h"

#include <iostream>
#include <fstream>
#include <bitset>

#ifdef _WIN32
#pragma warning ( disable : 4996 4267 )
#endif

namespace xocl {

static std::vector<command_callback_function_type> cmd_start_cb;
static std::vector<command_callback_function_type> cmd_done_cb;

void
add_command_start_callback(command_callback_function_type fcn)
{
  cmd_start_cb.emplace_back(std::move(fcn));
}

void
add_command_done_callback(command_callback_function_type fcn)
{
  cmd_done_cb.emplace_back(std::move(fcn));
}


inline void
run_start_callbacks(const execution_context* ctx, const xrt::run& run)
{
  for (auto& cb : cmd_start_cb)
    cb(ctx, run);
}

inline void
run_done_callbacks(const execution_context* ctx, const xrt::run& run)
{
  for (auto& cb : cmd_done_cb)
    cb(ctx, run);
}

// Forward declaration of event callback function for event of last
// copy stage of a job.
static void
run_done(const void* key, ert_cmd_state state, void* data);

void
execution_context::
set_global_arg_at_index(xrt::run& run, size_t argidx, const xocl::memory* mem)
{
  const auto& xbo = mem->get_buffer_object_or_error(m_device);
  run.set_arg(argidx, xbo);
}

void
execution_context::
set_rtinfo_printf(xrt::run& run, size_t arginfo_idx, const xocl::memory* printf_buffer)
{
  if (!printf_buffer)
    return;

  // This computes the offset that gets added to a physical printf buffer
  // address for a given workgroup. Necessary so we have a different
  // segment to hold each workgroup in the overall buffer.
  size_t lwsx = m_lsize[0];
  size_t lwsy = m_lsize[1];
  size_t lwsz = m_lsize[2];
  size_t gwsx = m_gsize[0];
  size_t gwsy = m_gsize[1];
  size_t local_buffer_size = lwsx * lwsy * lwsz * 2048 /*XCL::Printf::getWorkItemPrintfBufferSize()*/;
  size_t group_x_size = gwsx / lwsx;
  size_t group_y_size = gwsy / lwsy;
  size_t group_id = m_cu_group_id[0] +
    group_x_size * m_cu_group_id[1] +
    group_y_size * group_x_size * m_cu_group_id[2];
  auto printf_buffer_offset = group_id * local_buffer_size;
  auto xbo = printf_buffer->get_buffer_object_or_error(m_device);
  auto addr = xbo.address() + printf_buffer_offset;
  xrt_core::kernel_int::set_arg_at_index(run, arginfo_idx, &addr, sizeof(addr));
}

void
execution_context::
set_rtinfo_arg1(xrt::run& run, size_t arginfo_idx, size1 value)
{
  xrt_core::kernel_int::set_arg_at_index(run, arginfo_idx, &value, sizeof(value));
}

void
execution_context::
set_rtinfo_arg3(xrt::run& run, size_t arginfo_idx, const size3& value3)
{
  for (auto idx = 0; idx < 3; ++idx) {
    auto value = value3[idx];
    xrt_core::kernel_int::set_arg_at_index(run, idx + arginfo_idx, &value, sizeof(value));
  }
}

void
execution_context::
set_rtinfo_args(xrt::run& run)
{
  for (auto& arg : m_kernel->get_rtinfo_xargument_range()) {
    switch (arg->get_rtinfo_type()) {
    case xocl::kernel::rtinfo_type::dim:
      set_rtinfo_arg1(run, arg->get_arginfo_idx(), m_dim);
      break;
    case xocl::kernel::rtinfo_type::goff:
      set_rtinfo_arg3(run, arg->get_arginfo_idx(), m_goffset);
      break;
    case xocl::kernel::rtinfo_type::gsize:
      set_rtinfo_arg3(run, arg->get_arginfo_idx(), m_gsize);
      break;
    case xocl::kernel::rtinfo_type::lsize:
      set_rtinfo_arg3(run, arg->get_arginfo_idx(), m_lsize);
      break;
    case xocl::kernel::rtinfo_type::ngrps: {
      size3 num_workgroups {0,0,0};
      for (auto d : {0,1,2})
        if (m_lsize[d])
          num_workgroups[d] = m_gsize[d] / m_lsize[d];
      set_rtinfo_arg3(run, arg->get_arginfo_idx(), num_workgroups);
      break;
    }
    case xocl::kernel::rtinfo_type::gid:
      set_rtinfo_arg3(run, arg->get_arginfo_idx(), m_cu_global_id);
      break;
    case xocl::kernel::rtinfo_type::lid: {
      size3 local_id {0,0,0};
      set_rtinfo_arg3(run, arg->get_arginfo_idx(), local_id);
      break;
    }
    case xocl::kernel::rtinfo_type::grid:
      set_rtinfo_arg3(run, arg->get_arginfo_idx(), m_cu_group_id);
      break;
    case xocl::kernel::rtinfo_type::printf:
      throw std::runtime_error("internal error: rtinfo may not contain printf arg");
      break;
    }
  }

  // printf
  for (auto& arg : m_kernel->get_printf_xargument_range()) {
    switch (arg->get_rtinfo_type()) {
    case xocl::kernel::rtinfo_type::printf:
      set_rtinfo_printf(run, arg->get_arginfo_idx(), arg->get_memory_object());
      break;
    default:
      throw std::runtime_error("internal error: printf may not contain rtinfo arg");
      break;
    }
  }
  
}

execution_context::
execution_context(device* device
                  ,kernel* kd
                  ,event* event
                  ,size_t work_dim
                  ,const size_t* global_work_offset
                  ,const size_t* global_work_size
                  ,const size_t* local_work_size)
  : m_dim(work_dim)
  , m_event(event)
  , m_kernel(kd)
  , m_device(device)
    // cloning could potentially be managed in xocl::kernel
    // such that run behaves as copy-on-write if usecount>1
  , m_run(xrt_core::kernel_int::clone(kd->get_xrt_run(device)))
{
  static unsigned int count = 0;
  m_uid = count++;

  XOCL_DEBUGF("execution_context::execution_context(%d) for kernel(%s)\n",m_uid,m_kernel->get_name().c_str());
  std::copy(global_work_offset,global_work_offset+work_dim,m_goffset.begin());
  std::copy(global_work_size,global_work_size+work_dim,m_gsize.begin());
  std::copy(local_work_size,local_work_size+work_dim,m_lsize.begin());

  m_run.add_callback(ERT_CMD_STATE_COMPLETED, run_done, this);

  // populate run object with global kernel arguments
  size_t argidx = 0;
  for (auto& arg : m_kernel->get_indexed_xargument_range()) {
    if (auto mem = arg->get_memory_object())
      set_global_arg_at_index(m_run, argidx, mem);
    ++argidx;
  }

  m_num_cus = xrt_core::kernel_int::get_num_cus(m_run);
  m_control = xrt_core::kernel_int::get_control_protocol(m_run);

  m_freeruns.push_back(m_run);
}

execution_context::
~execution_context()
{
  XOCL_DEBUGF("execution_context::~execution_context(%d) for kernel(%s)\n",m_uid,m_kernel->get_name().c_str());
  xrt_core::kernel_int::pop_callback(m_run);
}

xrt::run
execution_context::
get_free_run()
{
  if (m_freeruns.empty()) {
    auto run = xrt_core::kernel_int::clone(m_run);
    // the callback must be able to identify the run object
    // but run objects are transient, so use run_impl, which
    // remains valid as long as some run refers to it.  The
    // callback may not retain the handle as it would prevent
    // deletion unless callback was removed
    auto handle = run.get_handle();

    
    run.add_callback(ERT_CMD_STATE_COMPLETED, run_done, this);
    return run;
  }
    
  auto run = m_freeruns.back();
  m_freeruns.pop_back();
  return run;
}

void
execution_context::
mark_active(const xrt::run& run)
{
  auto key = run.get_handle().get();
  m_activeruns.emplace(std::make_pair(key,run));
  ++m_active;
}

xrt::run
execution_context::
mark_inactive(const void* key)
{
  auto itr = m_activeruns.find(key);
  if (itr == m_activeruns.end())
    throw std::runtime_error("unexpected error, no active run");
  auto run = itr->second;
  m_activeruns.erase(itr);
  m_freeruns.push_back(run);
  --m_active;
  return run;
}

void
execution_context::
start()
{
  XOCL_DEBUGF("execution_context(%d) starting workgroup(%d,%d,%d)\n"
              ,get_uid(),m_cu_group_id[0],m_cu_group_id[1],m_cu_group_id[2]);

  // On first work load, transition event to CL_RUNNING
  if ( (m_cu_group_id[0]==0) && (m_cu_group_id[1]==0) && (m_cu_group_id[2]==0))
    m_event->set_status(CL_RUNNING);

  auto run = get_free_run();
  
  // Set OCL specific runtime control parameters which are based
  // current workgroups, etc
  set_rtinfo_args(run);

  // After setting rtinfo the work group data can be updated
  // This must be done before chance of calling run_done()
  update_work();

  mark_active(run);
  run.start();

  run_start_callbacks(this, run);

  return;
}

void
execution_context::
update_work()
{
  for (unsigned int dim=0; dim<m_dim; ++dim) {

    if (m_cu_global_id[dim]+m_lsize[dim] < m_gsize[dim]) {
      m_cu_global_id[dim] += m_lsize[dim];
      ++m_cu_group_id[dim];
      return;
    }

    m_cu_global_id[dim] = m_goffset[dim];
    m_cu_group_id[dim] = 0;

  }

  m_done = true;
}

bool
execution_context::
done(const void* key)
{
  // Care must be taken not to mark event complete and later reference
  // any data members of context which is owned (and deleted) with event
  bool ctx_done = false;
  {
    std::lock_guard<std::mutex> lk(m_mutex);

    // use key to retrieve and inactivate corresponding run object
    auto run = mark_inactive(key);

    // run callbacks on run object before it can be reused
    run_done_callbacks(this, run);

    if (m_active==0 && m_done)
      ctx_done=true;
  }

  // Only one thread will be able to set local ctx_done to true, so it's
  // safe to proceed without exclusive lock (mutex is a data member)
  if (ctx_done) {
    m_event->set_status(CL_COMPLETE);
    return true;
  }

  // execute more workgroups if any left
  execute();
  return false;
}

bool
execution_context::
execute()
{
  std::lock_guard<std::mutex> lk(m_mutex);

  if (m_done)
    return true;

  // Schedule workgroups.  But don't blindly schedule all workgroups
  // because that would fill the command queue with commands that
  // compete for same CUs and block (CQ full) other kernel calls that
  // may want to use other CUs.
  //
  // Also scheduling all work-groups may drain the memory for
  // execution buffers.
  //
  // In order to keep scheduler busy, we need more than just one
  // workgroup at a time, so here we try to ensure that the scheduled
  // commands at any given time is twice the number of available CUs.
  auto limit = (m_control == xrt::xclbin::ip::control_type::chain) ? 20 * m_num_cus : 2 * m_num_cus;
  for (size_t i = m_active; !m_done && i < limit; ++i) {
    start();
    XRT_DEBUGF("active=%d\n",m_active);
  }

  return m_done;
}

static void
run_done(const void* key, ert_cmd_state state, void* data)
{
  reinterpret_cast<execution_context*>(data)->done(key);
}

} // namespace xocl
