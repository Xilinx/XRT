// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef tracer_h
#define tracer_h

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <mutex>
#include <vector>

#include <xrt.h>
#include <xrt/xrt_bo.h>
#include <xrt/xrt_aie.h>
#include <xrt/xrt_device.h>
#include <xrt/xrt_hw_context.h>
#include <xrt/xrt_kernel.h>
#include <xrt/experimental/xrt_ip.h>
#include <xrt/experimental/xrt_mailbox.h>
#include <xrt/experimental/xrt_module.h>
#include <xrt/experimental/xrt_kernel.h>
#include <xrt/experimental/xrt_profile.h>
#include <xrt/experimental/xrt_queue.h>
#include <xrt/experimental/xrt_error.h>
#include <xrt/experimental/xrt_ext.h>
#include <xrt/experimental/xrt_ini.h>
#include <xrt/experimental/xrt_message.h>
#include <xrt/experimental/xrt_system.h>
#include <xrt/experimental/xrt_aie.h>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/timestamp.pb.h>
#include <func.pb.h>
#include <common/trace_utils.h>

template <typename PFUNC>
void
xbtracer_trace_arg_proto(PFUNC& func_msg, const char* arg_name, const char* type_name,
                         uint32_t arg_id, const std::string& val)
{
  xbtracer_proto::Arg* arg_proto = func_msg.add_arg();
  arg_proto->set_name(arg_name);
  arg_proto->set_index(arg_id);
  arg_proto->set_type(type_name);
  arg_proto->set_size(static_cast<uint32_t>(val.length()));
  arg_proto->set_value(val);
}

template <typename T, typename PFUNC>
void
xbtracer_trace_arg(const char* arg_name, const char* type_name, uint32_t arg_id, const T& arg,
                   PFUNC& func_msg)
{
  xbtracer_trace_arg_proto(func_msg, arg_name, type_name, arg_id,
                           std::string(reinterpret_cast<const char*>(&arg), sizeof(arg)));
}

template <typename T, typename PFUNC>
void
xbtracer_trace_arg(const char* arg_name, const char* type_name, const T& arg, PFUNC& func_msg)
{
  xbtracer_trace_arg_proto(func_msg, arg_name, type_name, 0,
                           std::string(reinterpret_cast<const char*>(&arg), sizeof(arg)));
}

template <typename T, typename PFUNC>
void
xbtracer_trace_arg(const char* arg_name, const T& arg, PFUNC& func_msg)
{
  xbtracer_trace_arg_proto(func_msg, arg_name, typeid(arg).name(), 0,
                           std::string(reinterpret_cast<const char*>(&arg), sizeof(arg)));
}

template <typename PFUNC>
void
xbtracer_trace_arg_string(const char* arg_name, const std::string& arg, PFUNC& func_msg)
{
  xbtracer_trace_arg_proto(func_msg, arg_name, "std::string", 0, arg);
}

template <typename SHPIMPLT, typename PFUNC>
void
xbtracer_trace_class_pimpl_with_arg(const SHPIMPLT& sh_pimpl, PFUNC& func_msg,
                                    std::string arg_name, uint32_t arg_id)
{
  // all classes we trace has pimpl handle or similar
  // the pointer of the handle will be used as an ID of the object
  const void* this_pimpl_ptr = reinterpret_cast<const void*>(sh_pimpl.get());
  xbtracer_trace_arg(arg_name.c_str(), "void", arg_id, this_pimpl_ptr, func_msg);
}

template <typename SHPIMPLT, typename PFUNC>
void
xbtracer_trace_class_pimpl(const SHPIMPLT& sh_pimpl, PFUNC& func_msg)
{
  xbtracer_trace_class_pimpl_with_arg(sh_pimpl, func_msg, "pimpl", 0);
}

template <typename PFUNC, typename PFUNC_TRACE_TYPE>
void
xbrtracer_init_func_proto_msg(PFUNC& func_msg, const char* func_name, PFUNC_TRACE_TYPE func_trace_type)
{
  func_msg.set_name(func_name);
  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
  auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration - seconds);

  google::protobuf::Timestamp* ts = func_msg.mutable_timestamp();
  ts->set_seconds(seconds.count());
  // Convert microseconds to nanoseconds
  // set_nanos() input is INT32
  constexpr uint32_t us_to_ns = 1000;
  ts->set_nanos(static_cast<int32_t>(micros.count() * us_to_ns)); // Convert microseconds to nanoseconds

  uint32_t pid = getpid_current_os();
  func_msg.set_pid(pid);
  func_msg.set_status(func_trace_type);
}

namespace xrt::tools::xbtracer
{
class tracer
{
  enum class level
  {
    DEFAULT = 0,
  };

public:
  tracer(const std::string& outf, level tl);

  // we always need to output tracing to a file
  tracer() = delete;
  // singleton, delete copy constructor and assignment operator to enforce singleton
  tracer(const tracer&) = delete;
  tracer& operator=(const tracer&) = delete;
  // singleton, elete move constructor and move assignment operator
  tracer(tracer&&) = delete;
  tracer& operator=(tracer&&) = delete;

  ~tracer();

  proc_addr_type
  get_proc_addr(const char* symbol);

  template <typename protobuf_msg>
  bool
  write_protobuf_msg(const protobuf_msg& msg)
  {
    auto msg_size = static_cast<uint32_t>(msg.ByteSizeLong());

    std::lock_guard<std::mutex> lock(trace_mlock);
    google::protobuf::io::OstreamOutputStream zero_copy_output(&tracer_ofile);
    google::protobuf::io::CodedOutputStream coded_output(&zero_copy_output);

    coded_output.WriteVarint32(msg_size);
    msg.SerializeToCodedStream(&coded_output);

    tracer_ofile.flush();
    return !coded_output.HadError();
  }

  bool
  trace_pid(uint32_t pid);

  bool
  remove_trace_pid(uint32_t pid);

  bool
  is_pid_traced(uint32_t pid);

  static
  tracer&
  get_instance();

  template <typename T>
  bool
  find_impl_ref(const std::shared_ptr<T>& sh_impl)
  {
    std::lock_guard<std::mutex> lock(pids_mlock);
    return find_add_impl_ref_nolock(sh_impl, false);
  }

  template <typename T>
  bool
  add_impl_ref(const std::shared_ptr<T>& sh_impl)
  {
    std::lock_guard<std::mutex> lock(refs_mlock);
    return find_add_impl_ref_nolock(sh_impl, true);
  }

  void
  check_impl_refs();

private:
  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt_core::device>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::kernel_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::bo_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::bo::async_handle_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::hw_context_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::module_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::elf_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::fence_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::ip_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::ip::interrupt_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::mailbox_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::device::error_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::queue_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::run_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::run::command_error_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::runlist_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::runlist::command_error_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::xclbin_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::xclbin::aie_partition_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::xclbin::arg_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::xclbin::ip_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::xclbin::kernel_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::xclbin::mem_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::xclbin_repository_impl>& sh_impl, bool add);

  bool
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::xclbin_repository::iterator_impl>& sh_impl, bool add);

  template <typename T>
  void
  check_impl_refs_tracker_nolock(std::tuple<std::string, std::vector<std::shared_ptr<T>>>& tracker)
  {
    const char* func_s = std::get<0>(tracker).c_str();
    auto& refs = std::get<1>(tracker);
    for (auto it = refs.begin(); it != refs.end(); ) {
      if (it->use_count() >= 2) {
        // still referenced by application
        //xbtracer_pdebug("DESTRUCTOR INSERT: TRACE: ", func_s, ", ", it->get(), ", ref=",
        //                it->use_count(), ".");
        it++;
      }
      else {
        xbtracer_proto::Func func_entry;
        xbtracer_pdebug("DESTRUCTOR INSERT: TRACE: ", func_s, ", ", it->get(), ", ref=",
                        it->use_count(), ".");
        xbrtracer_init_func_proto_msg(func_entry, func_s, xbtracer_proto::Func_FuncStatus_FUNC_INJECT);
        xbtracer_trace_class_pimpl(*it, func_entry);
        write_protobuf_msg(func_entry);
        it = refs.erase(it);
      }
    }
  }

  static std::unique_ptr<tracer> instance;
  static std::once_flag init_instance_flag;
  std::fstream tracer_ofile;
  level tlevel;
  lib_handle_type coreutil_lib_h;
  std::vector<uint32_t> trace_pids{};
  std::mutex pids_mlock; // track PIDs lock
  std::mutex refs_mlock; // track references lock
  std::mutex trace_mlock; // writing messages for functions APIs lock
  std::tuple<std::string, std::vector<std::shared_ptr<xrt_core::device>>> xrt_dev_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::kernel_impl>>> xrt_kernel_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::bo_impl>>> xrt_bo_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::bo::async_handle_impl>>> xrt_bo_async_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::hw_context_impl>>> xrt_hw_context_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::module_impl>>> xrt_module_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::elf_impl>>> xrt_elf_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::fence_impl>>> xrt_fence_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::ip_impl>>> xrt_ip_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::ip::interrupt_impl>>> xrt_ip_intr_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::mailbox_impl>>> xrt_mailbox_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::device::error_impl>>> xrt_dev_err_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::queue_impl>>> xrt_queue_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::run_impl>>> xrt_run_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::run::command_error_impl>>> xrt_run_cmd_err_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::runlist_impl>>> xrt_runlist_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::runlist::command_error_impl>>> xrt_runlist_cmd_err_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::xclbin_impl>>> xrt_xclbin_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::xclbin::aie_partition_impl>>> xrt_xclbin_aie_part_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::xclbin::arg_impl>>> xrt_xclbin_arg_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::xclbin::ip_impl>>> xrt_xclbin_ip_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::xclbin::kernel_impl>>> xrt_xclbin_kernel_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::xclbin::mem_impl>>> xrt_xclbin_mem_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::xclbin_repository_impl>>> xrt_xclbin_repo_ref_tracker{};
  std::tuple<std::string, std::vector<std::shared_ptr<xrt::xclbin_repository::iterator_impl>>> xrt_xclbin_repo_iter_ref_tracker{};
}; // class xrt::tools::xbracer::tracer

} // namespace xrt::tools::xbtracer

template <typename T>
bool
xbtracer_find_impl_ref(const std::shared_ptr<T>& sh_impl)
{
  return xrt::tools::xbtracer::tracer::get_instance().find_impl_ref(sh_impl);
}

template <typename T>
bool
xbtracer_add_impl_ref(const std::shared_ptr<T>& sh_impl)
{
  return xrt::tools::xbtracer::tracer::get_instance().add_impl_ref(sh_impl);
}

void
xbtracer_check_impl_refs();

template <typename protobuf_msg>
bool
xbtracer_write_protobuf_msg(const protobuf_msg& msg, bool need_trace)
{
  if (!need_trace)
    return true;
  return xrt::tools::xbtracer::tracer::get_instance().write_protobuf_msg(msg);
}

proc_addr_type
xbtracer_get_original_func_addr(const char* symbol);

bool
xbtracer_needs_trace_func();

bool
xbtrace_trace_current_func();

void
xbtrace_untrace_current_func();

template <typename PFUNC>
bool
xbtracer_init_func_entry(PFUNC& func_msg, bool& need_trace, const char* func_s,
                         proc_addr_type& paddr_ptr)
{
  const char* func_mname = get_func_mname_from_signature(func_s);
  if (!func_mname)
    xbtracer_pcritical("failed to get mangled name for function\"", std::string(func_s), "\".");
  paddr_ptr = xbtracer_get_original_func_addr(func_mname);
  if (!paddr_ptr)
    xbtracer_pcritical("failed to get function\"", std::string(func_s), "\", \"", std::string(func_s), "\".");

  if (!xbtracer_needs_trace_func()) {
    // if function doesn't need to be traced, do not initialize protobuf message
    // this is the case that the function is called from the library.
    xbtracer_pdebug("internal call to \"", std::string(func_s), "\", not tracing.");
    need_trace = false;
    return true;
  }

  xbtracer_check_impl_refs();
  xbtrace_trace_current_func();
  xbrtracer_init_func_proto_msg(func_msg, func_s, xbtracer_proto::Func_FuncStatus_FUNC_ENTRY);
  need_trace = true;
  xbtracer_pdebug("TRACE: \"", std::string(func_s), "\".");
  // check impl references
  return true;
}

template <typename PFUNC>
bool
xbtracer_init_func_exit(PFUNC& func_msg, bool need_trace, const char* func_s)
{
  if (!need_trace)
    return true;
  xbtrace_untrace_current_func();
  xbrtracer_init_func_proto_msg(func_msg, func_s, xbtracer_proto::Func_FuncStatus_FUNC_EXIT);
  return true;
}

template <typename PFUNC, typename SHPIMPLT>
bool
xbtracer_init_member_func_entry(const SHPIMPLT& sh_pimpl,
                                PFUNC& func_msg, bool& need_trace, const char* func_s,
                                proc_addr_type& paddr_ptr)
{
  bool ret = xbtracer_init_func_entry(func_msg, need_trace, func_s, paddr_ptr);
  if (need_trace) {
    // trace object handle pointer (pimpl) for member function
    if (!xbtracer_find_impl_ref(sh_pimpl))
      xbtracer_pinfo("member func: \"", func_s, "\" impl: ", sh_pimpl.get(), " not tracked.");
    xbtracer_trace_class_pimpl(sh_pimpl, func_msg);
  }
  return ret;
}

template <typename PFUNC, typename SHPIMPLT>
bool
xbtracer_init_member_func_exit(const SHPIMPLT& sh_pimpl, PFUNC& func_msg, bool& need_trace, const char* func_s)
{
  bool ret = xbtracer_init_func_exit(func_msg, need_trace, func_s);
  // trace object handle pointer (pimpl) for member function
  if (need_trace)
    xbtracer_trace_class_pimpl(sh_pimpl, func_msg);
  return ret;
}

template <typename PFUNC, typename SHPIMPLT>
bool
xbtracer_init_constructor_entry(const SHPIMPLT& sh_pimpl,
                                PFUNC& func_msg, bool& need_trace, const char* func_s,
                                proc_addr_type& paddr_ptr)
{
  bool ret = xbtracer_init_func_entry(func_msg, need_trace, func_s, paddr_ptr);
  if (need_trace)
    xbtracer_trace_class_pimpl(sh_pimpl, func_msg);
  return ret;
}

template <typename PFUNC, typename SHPIMPLT>
bool
xbtracer_init_constructor_exit(const SHPIMPLT& sh_pimpl, PFUNC& func_msg, bool need_trace,
		               const char* func_s)
{
  if (need_trace)
    xbtracer_add_impl_ref(sh_pimpl);
  xbtracer_init_func_exit(func_msg, need_trace, func_s);
  // trace object handle pointer (pimpl) for constructor
  if (need_trace)
    xbtracer_trace_class_pimpl(sh_pimpl, func_msg);
  return true;
}

template <typename PFUNC, typename SHPIMPLT>
bool
xbtracer_init_destructor_entry(const SHPIMPLT& sh_pimpl,
                                PFUNC& func_msg, bool& need_trace, const char* func_s,
                                proc_addr_type& paddr_ptr)
{
  bool ret = xbtracer_init_func_entry(func_msg, need_trace, func_s, paddr_ptr);
  if (need_trace)
    xbtracer_trace_class_pimpl(sh_pimpl, func_msg);
  return ret;
}

template <typename PFUNC>
bool
xbtracer_init_destructor_exit(PFUNC& func_msg, bool need_trace, const char* func_s)
{
  return xbtracer_init_func_exit(func_msg, need_trace, func_s);
}

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
// The following are implemented as macro because wanted to expand to use `this` pointer
// and macro will update the input argument `func_msg`
// no lint check to avoid this warning:
// warning: function-like macro 'xbtracer_init_constructor_entry_handle' used; consider a
// 'constexpr' template function
#define xbtracer_init_constructor_entry_handle(func_msg, need_trace, func_s, paddr_ptr) \
     xbtracer_init_constructor_entry(this->get_handle(), func_msg, need_trace, func_s, paddr_ptr);

#define xbtracer_init_constructor_exit_handle(func_msg, need_trace, func_s) \
     xbtracer_init_constructor_exit(this->get_handle(), func_msg, need_trace, func_s);

#define xbtracer_init_member_func_entry_handle(func_msg, need_trace, func_s, paddr_ptr) \
     xbtracer_init_member_func_entry(this->get_handle(), func_msg, need_trace, func_s, paddr_ptr);

#define xbtracer_init_member_func_exit_handle(func_msg, need_trace, func_s) \
     xbtracer_init_member_func_exit(this->get_handle(), func_msg, need_trace, func_s);

#define xbtracer_init_destructor_entry_handle(func_msg, need_trace, func_s, paddr_ptr) \
     xbtracer_init_destructor_entry(this->get_handle(), func_msg, need_trace, func_s, paddr_ptr);
// NOLINTEND(cppcoreguidelines-macro-usage)

bool
xbtracer_trace_file_content(const std::string& fname, uint32_t arg_id,
                            const std::string& arg_name, xbtracer_proto::Func& func_msg);

bool
xbtracer_trace_mem_dump(const void* data, size_t size, uint32_t arg_id,
                        const std::string& arg_name, xbtracer_proto::Func& func_msg);
#endif // tracer_h
