// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <wrapper/hook_xrt.h>

void
xrt::hw_context::
add_config(const xrt::elf& elf)
{
  const char* func_s = "xrt::hw_context::add_config(const xrt::elf&)";
  typedef void (xrt::hw_context::*func_t)(const xrt::elf&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  (this->*ofunc)(elf);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

void
xrt::hw_context::
update_qos(const xrt::hw_context::qos_type& qos)
{
  const char* func_s = "xrt::hw_context::update_qos(const xrt::hw_context::qos_type&)";
  typedef void (xrt::hw_context::*func_t)(const xrt::hw_context::qos_type&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  (this->*ofunc)(qos);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

xrt::device
xrt::hw_context::
get_device() const
{
  const char* func_s = "xrt::hw_context::get_device(void)";
  typedef xrt::device (xrt::hw_context::*func_t)() const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  xrt::device ret_o = (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

xrt::hw_context::access_mode
xrt::hw_context::
get_mode() const
{
  const char* func_s = "xrt::hw_context::get_mode(void)";
  typedef xrt::hw_context::access_mode (xrt::hw_context::*func_t)() const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  xrt::hw_context::access_mode ret_o = (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

xrt::hw_context::
hw_context(const xrt::device& device, const xrt::elf& elf)
{
  const char* func_s = "xrt::hw_context::hw_context(const xrt::device&, const xrt::elf&)";
  typedef xrt::hw_context* (*func_t)(void*, const xrt::device&, const xrt::elf&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_constructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ofunc((void*)this, device, elf);

  xbtracer_proto::Func func_exit;
  xbtracer_init_constructor_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

xrt::hw_context::
hw_context(const xrt::device& device, const xrt::elf& elf, const xrt::hw_context::cfg_param_type& cfg_param, xrt::hw_context::access_mode mode)
{
  const char* func_s = "xrt::hw_context::hw_context(const xrt::device&, const xrt::elf&, const xrt::hw_context::cfg_param_type&, xrt::hw_context::access_mode)";
  typedef xrt::hw_context* (*func_t)(void*, const xrt::device&, const xrt::elf&, const xrt::hw_context::cfg_param_type&, xrt::hw_context::access_mode);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_constructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ofunc((void*)this, device, elf, cfg_param, mode);

  xbtracer_proto::Func func_exit;
  xbtracer_init_constructor_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

xrt::hw_context::
hw_context(const xrt::device& device, const xrt::hw_context::cfg_param_type& cfg_param, xrt::hw_context::access_mode mode)
{
  const char* func_s = "xrt::hw_context::hw_context(const xrt::device&, const xrt::hw_context::cfg_param_type&, xrt::hw_context::access_mode)";
  typedef xrt::hw_context* (*func_t)(void*, const xrt::device&, const xrt::hw_context::cfg_param_type&, xrt::hw_context::access_mode);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_constructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ofunc((void*)this, device, cfg_param, mode);

  xbtracer_proto::Func func_exit;
  xbtracer_init_constructor_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

xrt::hw_context::
hw_context(const xrt::device& device, const xrt::uuid& xclbin_id, const xrt::hw_context::cfg_param_type& cfg_param)
{
  const char* func_s = "xrt::hw_context::hw_context(const xrt::device&, const xrt::uuid&, const xrt::hw_context::cfg_param_type&)";
  typedef xrt::hw_context* (*func_t)(void*, const xrt::device&, const xrt::uuid&, const xrt::hw_context::cfg_param_type&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_constructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ofunc((void*)this, device, xclbin_id, cfg_param);

  xbtracer_proto::Func func_exit;
  xbtracer_init_constructor_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

xrt::hw_context::
hw_context(const xrt::device& device, const xrt::uuid& xclbin_id, xrt::hw_context::access_mode mode)
{
  const char* func_s = "xrt::hw_context::hw_context(const xrt::device&, const xrt::uuid&, xrt::hw_context::access_mode)";
  typedef xrt::hw_context* (*func_t)(void*, const xrt::device&, const xrt::uuid&, xrt::hw_context::access_mode);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_constructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  if (need_trace) {
    xbtracer_trace_class_pimpl_with_arg(device.get_handle(), func_entry, "dev_impl", 1);

    xbtracer_proto::Arg* arg = func_entry.add_arg();
    arg->set_name("uuid");
    arg->set_index(2);
    arg->set_type("std::string");
    std::string uuid_str = xclbin_id.to_string();
    arg->set_size(static_cast<uint32_t>(uuid_str.length()));
    arg->set_value(uuid_str);

    arg = func_entry.add_arg();
    arg->set_name("mode");
    arg->set_index(3);
    arg->set_type("xrt::hw_context::access_mode");
    arg->set_size(static_cast<uint32_t>(sizeof(mode)));
    arg->set_value(std::string(reinterpret_cast<const char*>(&mode), sizeof(mode)));
  }
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ofunc((void*)this, device, xclbin_id, mode);

  xbtracer_proto::Func func_exit;
  xbtracer_init_constructor_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

xrt::hw_context::
operator xrt_core::hwctx_handle*() const
{
  const char* func_s = "xrt::hw_context::operator xrt_core::hwctx_handle*(void)";
  typedef xrt_core::hwctx_handle* (xrt::hw_context::*func_t)() const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  xrt_core::hwctx_handle* ret_o = (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

xrt::uuid
xrt::hw_context::
get_xclbin_uuid() const
{
  const char* func_s = "xrt::hw_context::get_xclbin_uuid(void)";
  typedef xrt::uuid (xrt::hw_context::*func_t)() const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  xrt::uuid ret_o = (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

xrt::xclbin
xrt::hw_context::
get_xclbin() const
{
  const char* func_s = "xrt::hw_context::get_xclbin(void)";
  typedef xrt::xclbin (xrt::hw_context::*func_t)() const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  xrt::xclbin ret_o = (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}
