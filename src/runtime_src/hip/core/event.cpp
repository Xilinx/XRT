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
  auto ev = std::dynamic_pointer_cast<event>(command_cache.get(static_cast<command_handle>(this)));
  throw_invalid_handle_if(!ev, "event passed is invalid");
  // Do not record the event again if it is being recorded in a stream
  {
    std::lock_guard lock(m_state_lock);
    throw_if((get_state() != state::init) && (get_state() < state::completed),
             hipErrorIllegalState, "event is being recorded");
    m_recorded_stream = std::move(s);
    // set state to recording, we are going to unhold the lock. As we are going to
    // call stream fucntions to enqueue the event as next step, which can call event
    // functions and may need to acquired the lock. The event state will be set to
    // to recorded in the end of this function after the event is enqueued. Setting
    // state to recording here is to avoid other threads to record the event again
    // before the event is enqueued.
    set_state(state::recording);
  }
  // reset the dependecies of this event as this is new recording
  {
    std::lock_guard lock(m_recorded_cmds_lock);
    m_recorded_commands.clear();
  }

  // this will update stream command queue, which will have lock pretection
  m_recorded_stream->enqueue_event(ev);
  {
    std::lock_guard lock(m_state_lock);
    // set state to recorded
    set_state(state::recorded);
  }
}

void event::init_wait_event(const std::shared_ptr<stream>& s, const std::shared_ptr<event>& e)
{
  auto wait_event_shared_ptr = std::dynamic_pointer_cast<event>(command_cache.get(this));
  add_dependency(e);
  e->add_to_chain(wait_event_shared_ptr);
  // enqueue wait event into wait stream
  s->enqueue(wait_event_shared_ptr);
  s->record_top_event(std::move(wait_event_shared_ptr));
  {
    std::lock_guard lock(m_state_lock);
    set_state(state::recorded);
  }
}

bool event::is_recorded_no_lock() const
{
  // event is recorded if it event::record() has been called and no error has been detected
  return (get_state() >= command::state::recorded);
}

bool event::is_recorded()
{
  std::lock_guard lock(m_state_lock);
  return is_recorded_no_lock();
}

bool event::query()
{
  //This function will return true if all commands in the appropriate stream which specified to hipEventRecord() have completed.
  std::lock_guard lock(m_state_lock);
  return (get_state() == state::completed);
}

// check if all dependencies are completed, update event state, and launch chain of commands if all
// dependencies are completed
// @input: wait_for_dependencies - if true, wait for dependencies to be completed
// @return: true if all dependencies are completed, false otherwise
// Note: This function can throw exception if dependencies have error, or launch chain of commands fails
bool event::check_and_launch_chain(bool wait_for_dependencies)
{
  bool event_is_completed = false;
  {
    std::lock_guard lock(m_state_lock);
    auto event_state = get_state();
    if (!is_recorded_no_lock() || (event_state == state::running && !wait_for_dependencies)) {
      // if event is not recorded, or event is already running that is event is already checking
      // dependencies in another thread, and caller doesn't want to wait, return false as we haven't
      // launched the chain of commands yet. This is in the event submit() case.
      return false;
    }
    if (event_state == state::completed) {
      event_is_completed = true;
    }
    else {
      // if event is recorded, check all dependencies and mark the event state as running
      set_state(state::running);
    }
  }

  bool dependencies_has_error = false;
  bool dependencies_completed = true;
  if (!event_is_completed) {
    // Check all dependencies
    std::lock_guard lock(m_recorded_cmds_lock);
    for (auto& rec_com : m_recorded_commands) {
      if (wait_for_dependencies)
        rec_com->wait();
      if (rec_com->get_state() == state::completed)
        continue;
      if (rec_com->get_state() > state::completed) {
        dependencies_has_error = true;
        break;
      }
      else {
        // dependency is not completed, return false and no state update
        dependencies_completed = false;
        break;
      }
    }
  }

  {
    std::lock_guard lock(m_state_lock);
    if (dependencies_has_error) {
      // dependencies have error, set event state to error
      set_state(state::error);
      throw_hip_error(hipErrorLaunchFailure, "event sync failed due to dependencies failure");
    }
    else if (!dependencies_completed) {
      // dependencies are not completed, return false and no state update
      set_state(state::recorded);
      return false;
    }
    set_state(state::completed);
    // update event completion time
    ctime = std::chrono::system_clock::now();
  }

  //all commands depend on the event start running after the event is completed
  {
    std::lock_guard lock(m_chain_cmds_lock);
    for (auto it = m_chain_of_commands.begin(); it != m_chain_of_commands.end(); ) {
      (*it)->submit();
      it = m_chain_of_commands.erase(it);
    }
  }
  return true;
}

bool event::synchronize()
{
  return check_and_launch_chain(true);
}

bool event::wait()
{
  return synchronize();
}

// submit event to start its chain of commands if all dependencies are completed
// unlike wait(), submit() will not wait for dependencies to be completed
// one event can be changed to another event.
// e.g. of multiple depdencies of wait event:
// - stream1: produce event1
// - stream2: produce event2
// - stream3: (wait_event1) wait on event1 --> do_cmd1 --> (wait_event2) wait on event2 --> do_cmd2
// do_cmd1 will wait on wait_event1 and do_cmd2 will wait on wait_event2
// wait_event2 has two dependencies: wait_event1 and event2
bool event::submit()
{
  // don't wait for dependencies to complete
  return check_and_launch_chain(false);
}

bool event::is_recorded_stream(const stream* s) noexcept
{
  std::lock_guard lock(m_state_lock);
  return (m_recorded_stream.get() == s);
}

void event::add_to_chain(std::shared_ptr<command> cmd)
{
  std::lock_guard lock(m_chain_cmds_lock);
  m_chain_of_commands.push_back(std::move(cmd));
}

void event::add_dependency(std::shared_ptr<command> cmd)
{
  std::lock_guard lock(m_recorded_cmds_lock);
  m_recorded_commands.push_back(std::move(cmd));
}

kernel_start::kernel_start(std::shared_ptr<function> f, void** args)
  : command(type::kernel_start)
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
kernel_start(std::shared_ptr<function> f, void** args, void** extra)
  : kernel_start(std::move(f), args)
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

bool
kernel_list_start::
wait()
{
  auto kernel_list_start_state = get_state();
  if (kernel_list_start_state == state::completed)
    return true;

  if (kernel_list_start_state != state::running)
    return false;

  m_rl.wait();
  set_state(state::completed);
  return true;
}

bool
kernel_list_start::
submit()
{
  auto kernel_list_start_state = get_state();
  if (kernel_list_start_state == state::running)
    return true;

  if (kernel_list_start_state == state::completed)
    return true;

  if (kernel_list_start_state != state::init)
    return false;

  m_rl.execute();
  set_state(state::running);
  return true;
}

bool event_record_command::submit()
{
  throw_invalid_value_if(!m_event, "event is nullptr");

  auto s = m_stream.lock();
  throw_invalid_value_if(!s, "stream is not set or has been destroyed for event record command");

  // Record the event in the stream
  m_event->record(s);
  set_state(state::completed);
  return true;
}

bool event_wait_command::submit()
{
  throw_invalid_value_if(!m_event, "event is nullptr");

  auto s = m_stream.lock();
  throw_invalid_value_if(!s, "stream is not set or has been destroyed for event wait command");

  // check stream on which wait is called is same as stream in which event is enqueued
  if (m_event->is_recorded_stream(s.get()))
    s->record_top_event(m_event);

  // Wait for the event to complete
  m_event->wait();

  // Clear the top event after wait completes (if it was set for this stream)
  if (m_event->is_recorded_stream(s.get()))
    s->clear_top_event();

  set_state(state::completed);
  return true;
}

// Global map of commands
xrt_core::handle_map<command_handle, std::shared_ptr<command>> command_cache;

} // xrt::core::hip
