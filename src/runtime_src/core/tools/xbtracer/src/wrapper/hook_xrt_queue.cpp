// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <wrapper/hook_xrt.h>

// NOLINTBEGIN(cppcoreguidelines-rvalue-reference-param-not-moved)
void
xrt::queue::
add_task(xrt::queue::task&& ev)
{
  const char* func_s = "xrt::queue::add_task(xrt::queue::task&&)";
  typedef void (xrt::queue::*func_t)(xrt::queue::task&&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry(this->m_impl, func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  (this->*ofunc)(std::move(ev));

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit(this->m_impl, func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}
// NOLINTEND(cppcoreguidelines-rvalue-reference-param-not-moved)

xrt::queue::
queue()
{
  const char* func_s = "xrt::queue::queue(void)";
  typedef xrt::queue* (*func_t)(void*);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_constructor_entry(this->m_impl, func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ofunc((void*)this);

  xbtracer_proto::Func func_exit;
  xbtracer_init_constructor_exit(this->m_impl, func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}
