// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

#include <iostream>

#include "event.h"
#include "hip/hip_xrt.h"
#include "memory.h"

namespace xrt::core::hip {
event::event()
  : command(type::event)
{
}

void event::record(std::shared_ptr<stream> s)
{
  cstream = std::move(s);
  auto ev = std::dynamic_pointer_cast<event>(command_cache.get(static_cast<command_handle>(this)));
  throw_invalid_handle_if(!ev, "event passed is invalid");
  if (is_recorded()) {
    // already recorded
    cstream->erase_cmd(ev);
  }
  // update recorded commands list
  cstream->enqueue_event(ev);
  set_state(state::recorded);
}

bool event::is_recorded() const
{
  //the event is recorded only if the state is not init
  return get_state() >= command::state::recorded;
}

bool event::query()
{
  //This function will return true if all commands in the appropriate stream which specified to hipEventRecord() have completed.
  std::lock_guard lock(m_mutex_rec_coms);
  for (auto& rec_com : m_recorded_commands){
    state command_state = rec_com->get_state();
    if (command_state != state::completed){
      return false;
    }
  }
  return true;
}

bool event::synchronize()
{
  //wait for commands in recorded list of the event to be completed
  std::lock_guard rec_lock(m_mutex_rec_coms);
  for (auto& rec_com : m_recorded_commands) {
    rec_com->wait();
  }
  //then the event is considered as completed
  set_state(state::completed);
  ctime = std::chrono::system_clock::now();

  //all commands depend on the event start running
  std::lock_guard ch_lock(m_mutex_chain_coms);
  for (auto& coms_ch :m_chain_of_commands){
    coms_ch->submit();
  }
  return true;
}

bool event::wait()
{
  state event_state = get_state();
  if (event_state == state::error)
    throw_hip_error(hipErrorRuntimeOther, "event is in error state");
  if (event_state < state::completed)
  {
    synchronize();
    set_state(state::completed);
    return true;
  }
  return false;
}

bool event::submit()
{
  return true;
}

std::shared_ptr<stream> event::get_stream()
{
  return cstream;
}

void event::add_to_chain(std::shared_ptr<command> cmd)
{
  std::lock_guard lock(m_mutex_chain_coms);
  m_chain_of_commands.push_back(std::move(cmd));
}

void event::add_dependency(std::shared_ptr<command> cmd)
{
  std::lock_guard lock(m_mutex_rec_coms);
  m_recorded_commands.push_back(std::move(cmd));
}

kernel_start::kernel_start(std::shared_ptr<stream> s, std::shared_ptr<function> f, void** args)
  : command(type::kernel_start, std::move(s))
  , func{std::move(f)}
  , m_ctrl_scratchpad_bo_sync_rd{false}
{
  auto k = func->get_kernel();

  /*
   * args (or kernelParams) is defined as the following by CUDA documentation:
   *
   * "Kernel parameters can be specified via kernelParams. If f has N
   * parameters, then kernelParams needs to be an array of N pointers. Each of
   * kernelParams[0] through kernelParams[N-1] must point to a region of memory
   * from which the actual kernel parameter will be copied. The number of kernel
   * parameters and their offsets and sizes do not need to be specified as that
   * information is retrieved directly from the kernel's image."
   *
   * Essentially args is an array of void * where each element points to the
   * "actual argument" which may be either a scalar or pointer to a buffer.
   * See the following example:
        uint64_t opcode = 3;
        void *o0 = obj0.getDeviceView(); // pointer to device buffer
        void *o1 = obj1.getDeviceView(); // pointer to device buffer
        void *o2 = obj2.getDeviceView(); // pointer to device buffer
        void *o4 = obj4.getDeviceView(); // pointer to device buffer
        std::array<void *, 8> args = {
        &opcode, // pointer to scalar
        nullptr, // ctrlcode pointer
        nullptr, // pointer to control code size
        &o0, // pointer to pointer
        &o1, // pointer to pointer
        &o2, // pointer to pointer
        nullptr, // ctrlpkt pointer
        &o4}; // pointer to pointer
   */

  // create run object and set args
  r = xrt::run(k);

  using karg = xrt_core::xclbin::kernel_argument;
  int idx = 0;
  for (const auto& arg : xrt_core::kernel_int::get_args(k)) {
    // non index args are not supported, this condition will not hit in case of HIP
    throw_invalid_value_if(arg->index == karg::no_index, "function has invalid argument");

    if (!args[idx]) {
      // Skip nullptr which is used for ctrlcode, ctrlcode size and ctrlpkt
      idx++;
      continue;
    }

    switch (arg->type) {
      case karg::argtype::scalar :
        xrt_core::kernel_int::set_arg_at_index(r, arg->index, args[idx], arg->size);
        break;

      case karg::argtype::global: {
        void **bufptr = static_cast<void **>(args[idx]);
        auto hip_mem = memory_database::instance().get_hip_mem_from_addr(*bufptr).first;
        if (!hip_mem) {
            std::string err_msg = "failed to get memory from arg at index - " + std::to_string(idx);
	    throw_hip_error(hipErrorInvalidValue, err_msg.c_str());
	}

        r.set_arg(arg->index, hip_mem->get_xrt_bo());
        break;
      }
      case karg::argtype::constant :
      case karg::argtype::local :
      case karg::argtype::stream :
      default :
        throw_hip_error(hipErrorInvalidValue,
          "function has unsupported arg type");
    }
    idx++;
  }
}

kernel_start::
kernel_start(std::shared_ptr<stream> s, std::shared_ptr<function> f, void** args, void** extra)
  : kernel_start(std::move(s), std::move(f), args)
{
  if (!extra)
    return;

  throw_invalid_value_if(!(*extra),
                         "kernel start cmd creation failed, extra is specified with null pointer.");

  // check for control scratchpad bo requirement
  auto extra_array = static_cast<hipXrtInfoExtraArray_t*>(*extra);
  throw_invalid_value_if((!extra_array->numExtras || extra_array->numExtras > 1),
                         "kernel start cmd creation failed, invalid number of extra information.");
  struct hipXrtInfoExtraHead *extra_headers = extra_array->extras;
  for (uint32_t i = 0; i < extra_array->numExtras; i++) {
    struct hipXrtInfoExtraHead *extra_head = &extra_headers[i];

    throw_invalid_value_if(extra_head->extraId != hipXrtExtraInfoCtrlScratchPad,
                           "kernel start cmd creation failed, extra Info is not control scratchpad bo.");

    auto ctrl_sp_bo_info = static_cast<hipXrtInfoCtrlScratchPad_t*>(extra_head->info);
    void* ctrl_sp_host_ptr = reinterpret_cast<void*>(ctrl_sp_bo_info->ctrlScratchPadHostPtr);
    uint32_t ctrl_sp_size = ctrl_sp_bo_info->ctrlScratchPadSize;
    throw_invalid_value_if((!ctrl_sp_host_ptr || !ctrl_sp_size),
			   "kernel start cmd creation failed, invalid control scratchpad bo information.");

    // no control scratchpad bo for the run
    m_ctrl_scratchpad_bo = r.get_ctrl_scratchpad_bo();
    throw_invalid_value_if(!m_ctrl_scratchpad_bo,
			   "kernel start cmd creation failed, control scratchpad bo expected but not allocated for the run.");
    throw_invalid_value_if(ctrl_sp_bo_info->ctrlScratchPadSize > m_ctrl_scratchpad_bo.size(),
			   "kernel start cmd creation failed, control scratchpad bo size provided by user is larger than allocated size.");

    // there is control scratchpad bo allocated for the run, return the information to user
    ctrl_sp_bo_info->ctrlScratchPadHostPtr =  reinterpret_cast<uint64_t>(m_ctrl_scratchpad_bo.map());
    ctrl_sp_bo_info->ctrlScratchPadSize = m_ctrl_scratchpad_bo.size();
    if (ctrl_sp_bo_info->syncAfterRun)
      m_ctrl_scratchpad_bo_sync_rd = true;
    else
      m_ctrl_scratchpad_bo_sync_rd = false;

    // sync control scratchpad bo to device before kernel start
    m_ctrl_scratchpad_bo.write(ctrl_sp_host_ptr, static_cast<size_t>(ctrl_sp_size), 0);
    m_ctrl_scratchpad_bo.sync(xclBOSyncDirection::XCL_BO_SYNC_BO_TO_DEVICE);
  }
}

bool kernel_start::submit()
{
  state kernel_start_state = get_state();
  if (kernel_start_state == state::init)
  {
    r.start();
    set_state(state::running);
    return true;
  }
  else if (kernel_start_state == state::running)
    return true;

  return false;
}

bool kernel_start::wait()
{
  state kernel_start_state = get_state();
  if (kernel_start_state == state::running)
  {
    try {
      r.wait2();

      // if control scratchpad bo is required to be synced back to host, do it here
      if (m_ctrl_scratchpad_bo_sync_rd && m_ctrl_scratchpad_bo)
        m_ctrl_scratchpad_bo.sync(xclBOSyncDirection::XCL_BO_SYNC_BO_FROM_DEVICE);

      set_state(state::completed);
      return true;
    }
    catch (const std::exception& ex) {
      // catch exception here is to set cmmand state to error.
      // re-throw errors so that caller can catch and handle it
      set_state(state::error);
      throw_hip_error(hipErrorLaunchFailure, ex.what());
    }
    catch (...) {
      set_state(state::error);
      throw_hip_error(hipErrorLaunchFailure, "Unknown error from kernel wait");
    }
  }
  else if (kernel_start_state == state::completed)
    return true;

  return false;
}

bool memcpy_command::submit()
{
  m_handle = std::async(std::launch::async, &hipMemcpy, m_dst, m_src, m_size, m_kind);
  return true;
}

bool memcpy_command::wait()
{
  m_handle.wait();
  set_state(state::completed);
  return true;
}

bool memory_pool_command::submit()
{
  switch (m_type)
  {
  case alloc:
    m_mem_pool->malloc(m_ptr, m_size);
    set_state(state::completed);
    break;
  case free:
    m_mem_pool->free(m_ptr);
    set_state(state::completed);
    break;

  default:
    throw_hip_error(hipErrorInvalidValue, "Invalid memory pool operation type.");
    break;
  }

  return true;
}

bool memory_pool_command::wait()
{
  // no-op
  return true;
}

// Global map of commands
xrt_core::handle_map<command_handle, std::shared_ptr<command>> command_cache;

} // xrt::core::hip
