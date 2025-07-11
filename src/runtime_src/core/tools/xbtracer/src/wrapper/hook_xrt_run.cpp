// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <wrapper/hook_xrt.h>

const char*
xrt::run::command_error::
what() const noexcept
{
  const char* func_s = "xrt::run::command_error::what(void)";
  typedef const char* (xrt::run::command_error::*func_t)() const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  const char* ret_o = (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

ert_cmd_state
xrt::run::
abort()
{
  const char* func_s = "xrt::run::abort(void)";
  typedef ert_cmd_state (xrt::run::*func_t)();
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ert_cmd_state ret_o = (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

ert_cmd_state
xrt::run::command_error::
get_command_state() const
{
  const char* func_s = "xrt::run::command_error::get_command_state(void)";
  typedef ert_cmd_state (xrt::run::command_error::*func_t)() const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ert_cmd_state ret_o = (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

ert_cmd_state
xrt::run::
state() const
{
  const char* func_s = "xrt::run::state(void)";
  typedef ert_cmd_state (xrt::run::*func_t)() const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ert_cmd_state ret_o = (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

ert_cmd_state
xrt::run::
wait(const std::chrono::milliseconds& timeout) const
{
  const char* func_s = "xrt::run::wait(const std::chrono::milliseconds&)";
  typedef ert_cmd_state (xrt::run::*func_t)(const std::chrono::milliseconds&) const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ert_cmd_state ret_o = (this->*ofunc)(timeout);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

ert_packet*
xrt::run::
get_ert_packet() const
{
  const char* func_s = "xrt::run::get_ert_packet(void)";
  typedef ert_packet* (xrt::run::*func_t)() const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ert_packet* ret_o = (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

int
xrt::run::
get_arg_index(const std::string& argnm) const
{
  const char* func_s = "xrt::run::get_arg_index(const std::string&)";
  typedef int (xrt::run::*func_t)(const std::string&) const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  int ret_o = (this->*ofunc)(argnm);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

std::cv_status
xrt::run::
wait2(const std::chrono::milliseconds& timeout) const
{
  const char* func_s = "xrt::run::wait2(const std::chrono::milliseconds&)";
  typedef std::cv_status (xrt::run::*func_t)(const std::chrono::milliseconds&) const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  std::cv_status ret_o = (this->*ofunc)(timeout);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

uint32_t
xrt::run::
return_code() const
{
  const char* func_s = "xrt::run::return_code(void)";
  typedef uint32_t (xrt::run::*func_t)() const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  uint32_t ret_o = (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

void
xrt::run::
add_callback(ert_cmd_state state, std::function<void(const void*, ert_cmd_state, void*)> callback, void* data)
{
  const char* func_s = "xrt::run::add_callback(ert_cmd_state, std::function<void(const void*, ert_cmd_state, void*)>, void*)";
  typedef void (xrt::run::*func_t)(ert_cmd_state, std::function<void(const void*, ert_cmd_state, void*)>, void*);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  (this->*ofunc)(state, callback, data);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

void
xrt::run::
set_arg_at_index(int index, const void* value, size_t bytes)
{
  const char* func_s = "xrt::run::set_arg_at_index(int, const void*, size_t)";
  typedef void (xrt::run::*func_t)(int, const void*, size_t);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  (this->*ofunc)(index, value, bytes);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

void
xrt::run::
set_arg_at_index(int index, const xrt::bo& arg2)
{
  const char* func_s = "xrt::run::set_arg_at_index(int, const xrt::bo&)";
  typedef void (xrt::run::*func_t)(int, const xrt::bo&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  (this->*ofunc)(index, arg2);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

void
xrt::run::
start()
{
  const char* func_s = "xrt::run::start(void)";
  typedef void (xrt::run::*func_t)();
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

void
xrt::run::
start(const xrt::autostart& iterations)
{
  const char* func_s = "xrt::run::start(const xrt::autostart&)";
  typedef void (xrt::run::*func_t)(const xrt::autostart&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  (this->*ofunc)(iterations);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

void
xrt::run::
stop()
{
  const char* func_s = "xrt::run::stop(void)";
  typedef void (xrt::run::*func_t)();
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

void
xrt::run::
submit_signal(const xrt::fence& fence)
{
  const char* func_s = "xrt::run::submit_signal(const xrt::fence&)";
  typedef void (xrt::run::*func_t)(const xrt::fence&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  (this->*ofunc)(fence);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

void
xrt::run::
submit_wait(const xrt::fence& fence)
{
  const char* func_s = "xrt::run::submit_wait(const xrt::fence&)";
  typedef void (xrt::run::*func_t)(const xrt::fence&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  (this->*ofunc)(fence);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

void
xrt::run::
update_arg_at_index(int index, const void* value, size_t bytes)
{
  const char* func_s = "xrt::run::update_arg_at_index(int, const void*, size_t)";
  typedef void (xrt::run::*func_t)(int, const void*, size_t);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  (this->*ofunc)(index, value, bytes);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

void
xrt::run::
update_arg_at_index(int index, const xrt::bo& arg2)
{
  const char* func_s = "xrt::run::update_arg_at_index(int, const xrt::bo&)";
  typedef void (xrt::run::*func_t)(int, const xrt::bo&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  (this->*ofunc)(index, arg2);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

xrt::bo
xrt::run::
get_ctrl_scratchpad_bo() const
{
  const char* func_s = "xrt::run::get_ctrl_scratchpad_bo(void)";
  typedef xrt::bo (xrt::run::*func_t)() const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  xrt::bo ret_o = (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

xrt::run::aie_error::
aie_error(const xrt::run& run, const std::string& what)
:command_error(run, "")

{
  const char* func_s = "xrt::run::aie_error::aie_error(const xrt::run&, const std::string&)";
  typedef xrt::run::aie_error* (*func_t)(void*, const xrt::run&, const std::string&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_constructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ofunc((void*)this, run, what);

  xbtracer_proto::Func func_exit;
  xbtracer_init_constructor_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

xrt::run::aie_error::span<const uint32_t>
xrt::run::aie_error::
data() const
{
  const char* func_s = "xrt::run::aie_error::data(void)";
  typedef xrt::run::aie_error::span<const uint32_t> (xrt::run::aie_error::*func_t)() const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  xrt::run::aie_error::span<const uint32_t> ret_o = (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

xrt::run::command_error::
command_error(const xrt::run& run, const std::string& what)
{
  const char* func_s = "xrt::run::command_error::command_error(const xrt::run&, const std::string&)";
  typedef xrt::run::command_error* (*func_t)(void*, const xrt::run&, const std::string&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_constructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ofunc((void*)this, run, what);

  xbtracer_proto::Func func_exit;
  xbtracer_init_constructor_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

xrt::run::command_error::
command_error(ert_cmd_state state, const std::string& what)
{
  const char* func_s = "xrt::run::command_error::command_error(ert_cmd_state, const std::string&)";
  typedef xrt::run::command_error* (*func_t)(void*, ert_cmd_state, const std::string&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_constructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ofunc((void*)this, state, what);

  xbtracer_proto::Func func_exit;
  xbtracer_init_constructor_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

xrt::run::
run(const xrt::kernel& krnl)
{
  const char* func_s = "xrt::run::run(const xrt::kernel&)";
  typedef xrt::run* (*func_t)(void*, const xrt::kernel&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_constructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ofunc((void*)this, krnl);

  xbtracer_proto::Func func_exit;
  xbtracer_init_constructor_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}
