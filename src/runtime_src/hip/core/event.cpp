// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.

#include "event.h"
#include "memory.h"

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

  //all commands depend on the event start running
  std::lock_guard ch_lock(m_mutex_chain_coms);
  for (auto& coms_ch :m_chain_of_commands){
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
  std::lock_guard lock(m_mutex_chain_coms);
  m_chain_of_commands.push_back(std::move(cmd));
}

void event::add_dependency(std::shared_ptr<command> cmd)
{
  std::lock_guard lock(m_mutex_rec_coms);
  m_recorded_commands.push_back(std::move(cmd));
}

kernel_start::kernel_start(std::shared_ptr<stream> s, std::shared_ptr<function> f, void** args)
  : command(std::move(s))
  , func{std::move(f)}
{
  ctype = type::kernel_start;
  auto k = func->get_kernel();

  // create run object and set args
  r = xrt::run(k);

  using karg = xrt_core::xclbin::kernel_argument;
  int idx = 0;
  for (const auto& arg : xrt_core::kernel_int::get_args(k)) {
    // non index args are not supported, this condition will not hit in case of HIP
    if (arg->index == karg::no_index)
      throw std::runtime_error("function has invalid argument");

    switch (arg->type) {
      case karg::argtype::scalar :
        xrt_core::kernel_int::set_arg_at_index(r, arg->index, args[idx], arg->size);
        break;
      case karg::argtype::global : {
        auto hip_mem = memory_database::instance().get_hip_mem_from_addr(args[idx]).first;
        if (!hip_mem)
          throw std::runtime_error("failed to get memory from arg at index - " + std::to_string(idx));

        // NPU device is not coherent. We need to sync the buffer objects before launching kernel
        if (hip_mem->get_type() != memory_type::device)
          hip_mem->sync(xclBOSyncDirection::XCL_BO_SYNC_BO_TO_DEVICE);
        r.set_arg(idx, hip_mem->get_xrt_bo());
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

copy_buffer::copy_buffer(std::shared_ptr<stream> s, xclBOSyncDirection direction, std::shared_ptr<memory> buf, void* ptr, size_t size, size_t offset)
  : command(std::move(s)), cdirection(direction), buffer(std::move(buf)), host_ptr(ptr), copy_size(size), dev_offset(offset)
{
  ctype = type::buffer_copy;
}

bool copy_buffer::submit()
{
  switch(cdirection)
  {
    case XCL_BO_SYNC_BO_TO_DEVICE:
      handle = std::async(std::launch::async, &memory::write, buffer, host_ptr, copy_size, 0, dev_offset);
      break;

    case XCL_BO_SYNC_BO_FROM_DEVICE:
      handle = std::async(std::launch::async, &memory::read, buffer, host_ptr, copy_size, dev_offset, 0);
      break;

    default:
      break;
  };

  return true;
}

bool copy_buffer::wait()
{
  handle.wait();
  set_state(state::completed);
  return true;
}

// Global map of commands
xrt_core::handle_map<command_handle, std::shared_ptr<command>> command_cache;

} // xrt::core::hip
