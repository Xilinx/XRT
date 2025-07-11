// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <version.h>

#include <wrapper/tracer.h>
#include <core/common/linux/linux_utils.h>
#include <common/trace_utils.h>

namespace xrt::tools::xbtracer
{
  tracer::tracer(const std::string& outf, tracer::level tl) :
	 tracer_ofile(outf, std::ios::out | std::ios::binary | std::ios::trunc),
         tlevel(tl)
  {
    if (!tracer_ofile || !tracer_ofile.is_open())
      throw std::runtime_error("xbtracer failed to open output file: \"" + std::string(outf) + "\".");
    coreutil_lib_h = load_library_os(XBRACER_XRT_COREUTIL_LIB);
    if (!coreutil_lib_h)
      throw std::runtime_error("xbrtracer failer to open lib: \"" +
                               std::string(XBRACER_XRT_COREUTIL_LIB) + "\".");
    std::get<0>(xrt_dev_ref_tracker) = "xrt::device::~device()";
    std::get<0>(xrt_kernel_ref_tracker) = "xrt::kernel::~kernel()";
    std::get<0>(xrt_bo_ref_tracker) = "xrt::bo::~bo()";
    std::get<0>(xrt_bo_async_ref_tracker) = "xrt::bo::async:~async()";
    std::get<0>(xrt_hw_context_ref_tracker) = "xrt::hw_context::~hw_context()";
    std::get<0>(xrt_module_ref_tracker) = "xrt::module::~module()";
    std::get<0>(xrt_elf_ref_tracker) = "xrt::elf::~elf()";
    std::get<0>(xrt_fence_ref_tracker) = "xrt::fence::~fence()";
    std::get<0>(xrt_ip_ref_tracker) = "xrt::ip::~ip()";
    std::get<0>(xrt_ip_intr_ref_tracker) = "xrt::ip::interrupt::~interrupt()";
    std::get<0>(xrt_mailbox_ref_tracker) = "xrt::mailbox::~mailbox()";
    std::get<0>(xrt_dev_err_ref_tracker) = "xrt::device::error::~error()";
    std::get<0>(xrt_queue_ref_tracker) = "xrt::queue::~queue()";
    std::get<0>(xrt_run_ref_tracker) = "xrt::run::~run()";
    std::get<0>(xrt_run_cmd_err_ref_tracker) = "xrt::run::command_error::~command_error()";
    std::get<0>(xrt_runlist_ref_tracker) = "xrt::runlist::~runlist()";
    std::get<0>(xrt_runlist_cmd_err_ref_tracker) = "xrt::runlist::command_error::~command_error()";
    std::get<0>(xrt_xclbin_ref_tracker) = "xrt::xclbin::~xclbin()";
    std::get<0>(xrt_xclbin_aie_part_ref_tracker) = "xrt::xclbin::aie_partition::~aie_partition()";
    std::get<0>(xrt_xclbin_arg_ref_tracker) = "xrt::xclbin::arg::~arg()";
    std::get<0>(xrt_xclbin_ip_ref_tracker) = "xrt::xclbin::ip::~ip()";
    std::get<0>(xrt_xclbin_kernel_ref_tracker) = "xrt::xclbin::kernel::~kernel()";
    std::get<0>(xrt_xclbin_mem_ref_tracker) = "xrt::xclbin::mem::~mem()";
    std::get<0>(xrt_xclbin_repo_ref_tracker) = "xrt::xclbin_repository::~xclbin_repository()";
    std::get<0>(xrt_xclbin_repo_iter_ref_tracker) = "xrt::xclbin_repository::iterator::~iterator()";
  }

  tracer::~tracer()
  {
    if (coreutil_lib_h)
      close_library_os(coreutil_lib_h);
    if (tracer_ofile.is_open())
      tracer_ofile.close();
  }

  proc_addr_type
  tracer::get_proc_addr(const char* symbol)
  {
    return get_proc_addr_os(coreutil_lib_h, symbol);
  }

  bool
  tracer::trace_pid(uint32_t pid)
  {
    std::lock_guard<std::mutex> lock(pids_mlock);
    trace_pids.push_back(pid);
    return true;
  }

  bool
  tracer::remove_trace_pid(uint32_t pid)
  {
    std::lock_guard<std::mutex> lock(pids_mlock);
    auto is_matched = [pid](uint32_t _pid) { return pid == _pid; };
    auto it = std::remove_if(trace_pids.begin(), trace_pids.end(), is_matched);
    if (it != trace_pids.end()) {
      trace_pids.erase(it, trace_pids.end());
      return true;
    }
    return false;
  }

  bool
  tracer::is_pid_traced(uint32_t pid)
  {
    std::lock_guard<std::mutex> lock(pids_mlock);
    auto is_matched = [pid](uint32_t _pid) { return pid == _pid; };
    auto it = std::remove_if(trace_pids.begin(), trace_pids.end(), is_matched);
    if (it != trace_pids.end())
      return true;
    return false;
  }

  constexpr size_t tracer_tlevel_str_len_max = 16;
  constexpr size_t tracer_dir_str_len_max = 2048;

  tracer&
  tracer::get_instance()
  {
    std::call_once(init_instance_flag, []() {
      // Create a tracer
      // Get environment variable to get the path and the tracing level
      std::string tlevel(tracer_tlevel_str_len_max, '\0');
      std::string odir(tracer_dir_str_len_max, '\0');
      getenv_os("XBTRACER_OUT_DIR", odir.data(), odir.capacity());
      getenv_os("XBRACER_TRACE_LEVEL", tlevel.data(), tlevel.capacity());
      tracer::level l = tracer::level::DEFAULT;

      if (strlen(tlevel.c_str())) {
	// TODO: we only support DEFAULT tracing level for now.
        tlevel.resize(strlen(tlevel.c_str()));
        if (tlevel != "DEFAULT")
          throw std::runtime_error("xbtracer: unsupported tracing level: \"" + tlevel + "\".");
      }

      std::filesystem::path opath;
      if (!strlen(odir.c_str())) {
        opath = std::filesystem::current_path();
      } else {
        odir.resize(strlen(odir.c_str()));
        opath = odir;
      }
      auto pid = getpid_current_os();
      opath.append(std::string("trace_protobuf" + std::to_string(pid) + ".bin"));
      // convert path to string first before converting it to c string to
      // make it work for both Linux and Windows.
      instance = std::unique_ptr<tracer>(new tracer(opath.string(), l));

      // Log XRT version
      GOOGLE_PROTOBUF_VERIFY_VERSION;
      xbtracer_proto::XrtExportApiCapture msg;
      msg.set_version(XRT_DRIVER_VERSION);
      if (!instance->write_protobuf_msg(msg))
        xbtracer_pcritical("get tracer instance failed, failed to log version information.");
    });
    return *instance;
  }

  template <typename T>
  bool
  local_find_impl_ref_nolock(const std::shared_ptr<T>& sh_impl,
                             const std::tuple<std::string, std::vector<std::shared_ptr<T>>>& tracker)
  {
    auto* impl_ptr = sh_impl.get();
    for (auto &sh_ptr: std::get<1>(tracker)) {
      if (impl_ptr == sh_ptr.get())
        return true;
    }
    return false;
  }

  template <typename T>
  void
  local_add_impl_ref_nolock(const std::shared_ptr<T>& sh_impl,
                            std::tuple<std::string, std::vector<std::shared_ptr<T>>>& tracker)
  {
    std::get<1>(tracker).push_back(sh_impl);
    xbtracer_pdebug("Add IMPL TRACE: \"", std::get<0>(tracker), "\", ", sh_impl.get(), ", ref count: ", sh_impl.use_count(), ".");
  }

  template <typename T>
  bool
  local_find_add_impl_ref_nolock(const std::shared_ptr<T>& sh_impl,
                                 std::tuple<std::string, std::vector<std::shared_ptr<T>>>& tracker,
				 bool add)
  {
    bool found = local_find_impl_ref_nolock(sh_impl, tracker);
    if (add && !found) {
      local_add_impl_ref_nolock(sh_impl, tracker);
    }
    return found;
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt_core::device>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_dev_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::kernel_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_kernel_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::bo_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_bo_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::bo::async_handle_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_bo_async_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::hw_context_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_hw_context_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::module_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_module_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::ip_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_ip_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::elf_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_elf_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::ip::interrupt_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_ip_intr_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::fence_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_fence_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::mailbox_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_mailbox_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::device::error_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_dev_err_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::queue_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_queue_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::run_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_run_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::run::command_error_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_run_cmd_err_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::runlist_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_runlist_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::runlist::command_error_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_runlist_cmd_err_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::xclbin_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_xclbin_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::xclbin::aie_partition_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_xclbin_aie_part_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::xclbin::arg_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_xclbin_arg_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::xclbin::ip_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_xclbin_ip_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::xclbin::kernel_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_xclbin_kernel_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::xclbin::mem_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_xclbin_mem_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::xclbin_repository_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_xclbin_repo_ref_tracker, add);
  }

  bool
  tracer::
  find_add_impl_ref_nolock(const std::shared_ptr<xrt::xclbin_repository::iterator_impl>& sh_impl, bool add)
  {
    return local_find_add_impl_ref_nolock(sh_impl, xrt_xclbin_repo_iter_ref_tracker, add);
  }

  void
  tracer::
  check_impl_refs()
  {
    std::lock_guard<std::mutex> lock(refs_mlock);
    check_impl_refs_tracker_nolock(xrt_bo_async_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_bo_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_fence_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_kernel_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_hw_context_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_module_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_elf_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_ip_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_ip_intr_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_mailbox_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_run_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_run_cmd_err_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_runlist_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_runlist_cmd_err_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_xclbin_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_xclbin_aie_part_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_xclbin_arg_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_xclbin_ip_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_xclbin_kernel_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_xclbin_mem_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_xclbin_repo_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_xclbin_repo_iter_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_dev_err_ref_tracker);
    check_impl_refs_tracker_nolock(xrt_dev_ref_tracker);
  }

} // namespace xrt::tools::xbtracer

std::unique_ptr<xrt::tools::xbtracer::tracer> xrt::tools::xbtracer::tracer::instance = nullptr;
std::once_flag xrt::tools::xbtracer::tracer::init_instance_flag;

void
xbtracer_check_impl_refs()
{
  xrt::tools::xbtracer::tracer::get_instance().check_impl_refs();
}

bool
xbtracer_needs_trace_func()
{
  uint32_t pid = getpid_current_os();
  return !xrt::tools::xbtracer::tracer::get_instance().is_pid_traced(pid);
}

bool
xbtrace_trace_current_func()
{
  uint32_t pid = getpid_current_os();
  return xrt::tools::xbtracer::tracer::get_instance().trace_pid(pid);
}

void
xbtrace_untrace_current_func()
{
  uint32_t pid = getpid_current_os();
  xrt::tools::xbtracer::tracer::get_instance().remove_trace_pid(pid);
}

bool
xbtracer_trace_file_content(const std::string& fname, uint32_t arg_id,
                            const std::string& arg_name, xbtracer_proto::Func& func_msg)
{
  std::ifstream ifile(fname, std::ios::binary | std::ios::ate);
  if (!ifile.is_open()) {
    xbtracer_perror(__func__, ": failed to open \"", fname, "\", ", sys_dep_get_last_err_msg(), ".");
    return false;
  }

  std::streamsize size = ifile.tellg();
  ifile.seekg(0, std::ios::beg);

  std::string buf(size, 0);
  if (!ifile.read(&buf[0], size)) {
    xbtracer_perror(__func__, ": failed to read \"", fname, "\", ", sys_dep_get_last_err_msg(), ".");
    return false;
  }
  ifile.close();

  xbtracer_proto::Arg* arg = func_msg.add_arg();
  arg->set_name(arg_name);
  arg->set_index(arg_id);
  arg->set_type("byes");
  arg->set_size(static_cast<uint32_t>(size));
  arg->set_value(buf);
  return true;
}

bool
xbtracer_trace_mem_dump(const void* data, size_t size, uint32_t arg_id,
                        const std::string& arg_name, xbtracer_proto::Func& func_msg)
{
  xbtracer_proto::Arg* arg = func_msg.add_arg();
  arg->set_name(arg_name);
  arg->set_index(arg_id);
  arg->set_type("byes");
  arg->set_size(static_cast<uint32_t>(size));
  arg->set_value(std::string(reinterpret_cast<const char*>(data), size));
  return true;
}
