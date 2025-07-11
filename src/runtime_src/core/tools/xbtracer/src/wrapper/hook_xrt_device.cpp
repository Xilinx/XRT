// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <wrapper/hook_xrt.h>
#include <boost/any.hpp>

boost::any
xrt::device::
get_info(xrt::info::device param) const
{
  const char* func_s = "xrt::device::get_info(xrt::info::device)";
  typedef boost::any (xrt::device::*func_t)(xrt::info::device) const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  boost::any ret_o = (this->*ofunc)(param);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}
#include <boost/any.hpp>

boost::any
xrt::device::
get_info(xrt::info::device param, const xrt::detail::abi& arg2) const
{
  const char* func_s = "xrt::device::get_info(xrt::info::device, const xrt::detail::abi&)";
  typedef boost::any (xrt::device::*func_t)(xrt::info::device, const xrt::detail::abi&) const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  boost::any ret_o = (this->*ofunc)(param, arg2);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

const char*
xrt::device::error::
what() const noexcept
{
  const char* func_s = "xrt::device::error::what(void)";
  typedef const char* (xrt::device::error::*func_t)() const;
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

std::any
xrt::device::
get_info_std(xrt::info::device param, const xrt::detail::abi& arg2) const
{
  const char* func_s = "xrt::device::get_info_std(xrt::info::device, const xrt::detail::abi&)";
  typedef std::any (xrt::device::*func_t)(xrt::info::device, const xrt::detail::abi&) const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  std::any ret_o = (this->*ofunc)(param, arg2);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

std::pair<const char*, size_t>
xrt::device::
get_xclbin_section(axlf_section_kind section, const xrt::uuid& uuid) const
{
  const char* func_s = "xrt::device::get_xclbin_section(axlf_section_kind, const xrt::uuid&)";
  typedef std::pair<const char*, size_t> (xrt::device::*func_t)(axlf_section_kind, const xrt::uuid&) const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  std::pair<const char*, size_t> ret_o = (this->*ofunc)(section, uuid);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

void
xrt::device::
reset()
{
  const char* func_s = "xrt::device::reset(void)";
  typedef void (xrt::device::*func_t)();
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

xrt::device::
device(const std::string& bdf)
{
  const char* func_s = "xrt::device::device(const std::string&)";
  typedef xrt::device* (*func_t)(void*, const std::string&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_constructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ofunc((void*)this, bdf);

  xbtracer_proto::Func func_exit;
  xbtracer_init_constructor_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

xrt::device::
device(unsigned int didx)
{
  const char* func_s = "xrt::device::device(unsigned int)";
  typedef xrt::device* (*func_t)(void*, unsigned int);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_constructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_trace_arg("didx", didx, func_entry);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ofunc((void*)this, didx);

  xbtracer_proto::Func func_exit;
  xbtracer_init_constructor_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

xrt::device::
device(xclDeviceHandle dhdl)
{
  const char* func_s = "xrt::device::device(xclDeviceHandle)";
  typedef xrt::device* (*func_t)(void*, xclDeviceHandle);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_constructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ofunc((void*)this, dhdl);

  xbtracer_proto::Func func_exit;
  xbtracer_init_constructor_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

xrt::device::error::
error(const std::string& msg)
{
  const char* func_s = "xrt::device::error::error(const std::string&)";
  typedef xrt::device::error* (*func_t)(void*, const std::string&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_constructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  ofunc((void*)this, msg);

  xbtracer_proto::Func func_exit;
  xbtracer_init_constructor_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);
}

xrt::device::
operator xclDeviceHandle() const
{
  const char* func_s = "xrt::device::operator xclDeviceHandle(void)";
  typedef xclDeviceHandle (xrt::device::*func_t)() const;
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  xclDeviceHandle ret_o = (this->*ofunc)();

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  if (need_trace) {
    xbtracer_trace_arg("dev_handle", ret_o, func_exit);
  }
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

xrt::uuid
xrt::device::
get_xclbin_uuid() const
{
  const char* func_s = "xrt::device::get_xclbin_uuid(void)";
  typedef xrt::uuid (xrt::device::*func_t)() const;
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

xrt::uuid
xrt::device::
load_xclbin(const axlf* xclbin)
{
  const char* func_s = "xrt::device::load_xclbin(const axlf*)";
  typedef xrt::uuid (xrt::device::*func_t)(const axlf*);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  xrt::uuid ret_o = (this->*ofunc)(xclbin);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

xrt::uuid
xrt::device::
load_xclbin(const std::string& xclbin_fnm)
{
  const char* func_s = "xrt::device::load_xclbin(const std::string&)";
  typedef xrt::uuid (xrt::device::*func_t)(const std::string&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  xrt::uuid ret_o = (this->*ofunc)(xclbin_fnm);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

xrt::uuid
xrt::device::
load_xclbin(const xrt::xclbin& xclbin)
{
  const char* func_s = "xrt::device::load_xclbin(const xrt::xclbin&)";
  typedef xrt::uuid (xrt::device::*func_t)(const xrt::xclbin&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  xrt::uuid ret_o = (this->*ofunc)(xclbin);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}

xrt::uuid
xrt::device::
register_xclbin(const xrt::xclbin& xclbin)
{
  const char* func_s = "xrt::device::register_xclbin(const xrt::xclbin&)";
  typedef xrt::uuid (xrt::device::*func_t)(const xrt::xclbin&);
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = reinterpret_cast<void **>(&ofunc);
  bool need_trace = false;

  xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);
  if (need_trace) {
    xbtracer_trace_class_pimpl_with_arg(xclbin.get_handle(), func_entry, "xclbin_impl", 1);
  }
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;

  xrt::uuid ret_o = (this->*ofunc)(xclbin);

  xbtracer_proto::Func func_exit;
  xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);
  if (need_trace) {
    std::string uuid_str = ret_o.to_string();
    xbtracer_trace_arg_string("uuid", uuid_str, func_exit);
  }
  xbtracer_write_protobuf_msg(func_exit, need_trace);

  return ret_o;
}
