// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.

#include "event.h"

namespace xrt::core::hip {
event::event()
{
  ctype = type::event;
}

void event::record(std::shared_ptr<stream> s)
{
  cstream = std::move(s);
  auto ev = std::dynamic_pointer_cast<event>(command_cache.get(static_cast<command_handle>(this)));
  throw_invalid_handle_if(!ev, "event passed is invalid");
  /*
  TODO
  if (is_recorded()) {
    // already recorded
    cstream->erase_cmd(ev); It is commented will be uncomment after stream PR is pushed
  }
  // update recorded commands list
  cstream->enqueue_event(std::move(ev));It is commented will be uncomment after stream PR is pushed
  */
  set_state(state::recorded);
}

bool event::is_recorded() const
{
  return get_state() >= command::state::recorded;
}

bool event::query()
{
  for (auto rec_com : recorded_commands){
    state command_state = rec_com->get_state();
    if (command_state != state::completed){
      return false;
    }
  }
  return true;
}

bool event::synchronize()
{
  for (auto rec_com : recorded_commands) {
    rec_com->wait();
  }
  set_state(state::completed);
  for (auto coms_ch :chain_of_commands){
    coms_ch->submit();
  }
  return true;
}

bool event::wait()
{
  ctime = std::chrono::system_clock::now();
  state event_state = get_state();
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
  // lock and add
}

void event::add_dependency(std::shared_ptr<command> cmd)
{
  // lock and add
}

float event::elapsed_time (const std::shared_ptr<command>& end)
{
  auto duration = end->get_time() - get_time();
  auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  return millis;
}

kernel_start::kernel_start(std::shared_ptr<stream> s, std::shared_ptr<function> f, void** args)
  : command(std::move(s))
  , func{std::move(f)}
{
  ctype = type::kernel_start;
  auto k = func->get_kernel();
  auto arginfo = std::move(xrt_core::kernel_int::get_args(k));

  // create run object and set args
  r = xrt::run(k);

  using karg = xrt_core::xclbin::kernel_argument;
  size_t idx = 0;
  for (const auto& arg : arginfo) {
    // non index args are not supported, this condition will not hit in case of HIP
    if (arg->index == karg::no_index)
      throw std::runtime_error("function has invalid argument");

    switch (arg->type) {
      case karg::argtype::scalar :
        xrt_core::kernel_int::set_arg_at_index(r, arg->index, args[idx], arg->size);
        break;
      case karg::argtype::global : {
        // TODO : add code for memory validation
        // auto hip_mem = memory_database::GetInstance()->get_hip_mem_from_addr(args[idx]);
        //if (!hip_mem)
          //throw std::runtime_error("failed to get memory from arg at index - " + std::to_string(idx));
        //r.set_arg(idx, hip_mem->get_xrt_bo()); // get_xrt_bo should return xrt::bo
        break;
      }
      case karg::argtype::constant :
      case karg::argtype::local :
      case karg::argtype::stream :
      default :
       throw std::runtime_error("function has unsupported arg type");
    }
    idx++;
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
    r.wait();
    set_state(state::completed);
    return true;
  }
  else if (kernel_start_state == state::completed)
    return true;

  return false;
}

copy_buffer::copy_buffer(std::shared_ptr<stream> s)
  : command(std::move(s))
{
  ctype = type::buffer_copy;
}

bool copy_buffer::submit()
{
  //handle = std::async(&xrt::bo::sync, &cbo, cdirection);
  return true;
}

bool copy_buffer::wait()
{
  handle.wait();
  return true;
}

// Global map of commands
xrt_core::handle_map<command_handle, std::shared_ptr<command>> command_cache;

} // xrt::core::hip
