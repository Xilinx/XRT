// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <wrapper/hook_xrt.h>

bool
xrt::aie::device::
write_aie_reg(pid_t pid, uint16_t context_id, uint16_t col, uint16_t row, uint32_t reg_addr, uint32_t reg_val)
{
  const char* func_s = "xrt::aie::device::write_aie_reg(pid_t, uint16_t, uint16_t, uint16_t, uint32_t, uint32_t)";
  typedef bool (xrt::aie::device::*func_t)(pid_t, uint16_t, uint16_t, uint16_t, uint32_t, uint32_t);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  bool ret_o = (this->*ofunc)(pid, context_id, col, row, reg_addr, reg_val);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

size_t
xrt::aie::device::
write_aie_mem(pid_t pid, uint16_t context_id, uint16_t col, uint16_t row, uint32_t offset, const std::vector<char>& data)
{
  const char* func_s = "xrt::aie::device::write_aie_mem(pid_t, uint16_t, uint16_t, uint16_t, uint32_t, const std::vector<char>&)";
  typedef size_t (xrt::aie::device::*func_t)(pid_t, uint16_t, uint16_t, uint16_t, uint32_t, const std::vector<char>&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  size_t ret_o = (this->*ofunc)(pid, context_id, col, row, offset, data);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

std::vector<char>
xrt::aie::device::
read_aie_mem(pid_t pid, uint16_t context_id, uint16_t col, uint16_t row, uint32_t offset, uint32_t size) const
{
  const char* func_s = "xrt::aie::device::read_aie_mem(pid_t, uint16_t, uint16_t, uint16_t, uint32_t, uint32_t)";
  typedef std::vector<char> (xrt::aie::device::*func_t)(pid_t, uint16_t, uint16_t, uint16_t, uint32_t, uint32_t) const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  std::vector<char> ret_o = (this->*ofunc)(pid, context_id, col, row, offset, size);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

uint32_t
xrt::aie::device::
read_aie_reg(pid_t pid, uint16_t context_id, uint16_t col, uint16_t row, uint32_t reg_addr) const
{
  const char* func_s = "xrt::aie::device::read_aie_reg(pid_t, uint16_t, uint16_t, uint16_t, uint32_t)";
  typedef uint32_t (xrt::aie::device::*func_t)(pid_t, uint16_t, uint16_t, uint16_t, uint32_t) const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  uint32_t ret_o = (this->*ofunc)(pid, context_id, col, row, reg_addr);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

void
xrt::aie::device::
open_context(xrt::aie::device::access_mode mode)
{
  const char* func_s = "xrt::aie::device::open_context(xrt::aie::device::access_mode)";
  typedef void (xrt::aie::device::*func_t)(xrt::aie::device::access_mode);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  (this->*ofunc)(mode);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

void
xrt::aie::program::
valid_or_error()
{
  const char* func_s = "xrt::aie::program::valid_or_error(void)";
  typedef void (xrt::aie::program::*func_t)();
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

xrt::aie::program::size_type
xrt::aie::program::
get_partition_size() const
{
  const char* func_s = "xrt::aie::program::get_partition_size(void)";
  typedef xrt::aie::program::size_type (xrt::aie::program::*func_t)() const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  xrt::aie::program::size_type ret_o = (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}
