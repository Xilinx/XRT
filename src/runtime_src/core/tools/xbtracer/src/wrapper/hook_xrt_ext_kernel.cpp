// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <wrapper/hook_xrt.h>

xrt::ext::kernel::
kernel(const xrt::hw_context& ctx, const std::string& name)
{
  const char* func_s = "xrt::ext::kernel::kernel(const xrt::hw_context&, const std::string&)";
  typedef xrt::ext::kernel* (*func_t)(void*, const xrt::hw_context&, const std::string&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_constructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ofunc((void*)this, ctx, name);

  xbtracer_proto::Func func_exit;
  xbtracer_init_constructor_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

xrt::ext::kernel::
kernel(const xrt::hw_context& ctx, const xrt::module& mod, const std::string& name)
{
  const char* func_s = "xrt::ext::kernel::kernel(const xrt::hw_context&, const xrt::module&, const std::string&)";
  typedef xrt::ext::kernel* (*func_t)(void*, const xrt::hw_context&, const xrt::module&, const std::string&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_constructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ofunc((void*)this, ctx, mod, name);

  xbtracer_proto::Func func_exit;
  xbtracer_init_constructor_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}
