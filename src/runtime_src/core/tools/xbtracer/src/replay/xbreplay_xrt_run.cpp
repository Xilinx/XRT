// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include <common/trace_utils.h>
#include <replay/xbreplay_common.h>

namespace xrt::tools::xbtracer
{
void
replayer::
register_run_func()
{
  xbreplay_funcs_map["xrt::run::run(const xrt::kernel&)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::bo::bo(xclDeviceHandle, size_t, xrt::bo::flags, xrt::memory_group) needs entry and exit, one of them is empty.");
    }
    if (entry_msg->arg_size() < 2) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 2, ".");
    }

    uint64_t kernel_impl;
    if (get_impl_from_proto_arg(entry_msg->arg(1), kernel_impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get xclbin impl from entry message.");
    }
    auto kernel_sh = get_tracked_kernel(kernel_impl);
    if (!kernel_sh) {
      xbtracer_pcritical(entry_msg->name(), " failed to get kernel with ",
                         reinterpret_cast<const void*>(kernel_impl), ".");
    }

    uint64_t impl;
    if (exit_msg->arg_size() < 1) {
      xbtracer_pcritical(exit_msg->name(), " invalid number of exit args, ", exit_msg->arg_size(),
                         ", ", 1, ".");
    }
    if (get_impl_from_proto_arg(exit_msg->arg(0), impl)) {
      xbtracer_pcritical(exit_msg->name(), " failed to get impl from exit message.");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<const void*>(impl),
                   ", with kernel: ", reinterpret_cast<const void*>(kernel_impl), ".");
    std::shared_ptr<xrt::run> run_sh = std::make_shared<xrt::run>(*kernel_sh);
    track(run_sh, impl);
    return 0;
  };

  xbreplay_funcs_map["xrt::run::~run()"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    (void)exit_msg;
    if (!entry_msg) {
      xbtracer_pcritical("xrt::run::~run() needs entry, one of them is empty.");
    }
    if (entry_msg->arg_size() < 1) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 5, ".");
    }

    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get impl from entry message.");
    }
    auto run_sh = get_tracked_run(impl);
    if (!run_sh) {
      xbtracer_pcritical(entry_msg->name(), "failed to get run from impl, ",
                         reinterpret_cast<const void*>(impl), ".");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<const void*>(impl),
                   ".");
    untrack_run(impl);
    return 0;
  };

  xbreplay_funcs_map["xrt::run::set_arg_at_index(int, const void*, size_t)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::run::set_arg_at_index(int, const void*, size_t) needs entry and exit, one of them is empty.");
    }
    if (entry_msg->arg_size() < 3) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 2, ".");
    }

    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get run impl from entry message.");
    }
    auto run_sh = get_tracked_run(impl);
    if (!run_sh) {
      xbtracer_pcritical(entry_msg->name(), " failed to get kernel with ",
                         reinterpret_cast<const void*>(impl), ".");
    }

    int index = 0;
    if (get_arg_from_proto_arg(entry_msg, 1, index)) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get index from trace message.");
    }
    size_t t_size = 0;
    const void* t_value = get_data_from_proto_arg(*entry_msg, 2, t_size);
    if (!t_size) {
      xbtracer_pcritical(entry_msg->name(), ", failed, not able to get value from message.");
    }
    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(impl),
                   ", index: ", index, ", value size: ", t_size, ".");
    run_sh->set_arg(index, t_value, t_size);
    return 0;
  };

  xbreplay_funcs_map["xrt::run::set_arg_at_index(int, const xrt::bo&)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::run::set_arg_at_index(int, const xrt::bo&) needs entry and exit, one of them is empty.");
    }
    if (entry_msg->arg_size() < 3) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 2, ".");
    }

    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get run impl from entry message.");
    }
    auto run_sh = get_tracked_run(impl);
    if (!run_sh) {
      xbtracer_pcritical(entry_msg->name(), " failed to get kernel with ",
                         reinterpret_cast<const void*>(impl), ".");
    }

    int index = 0;
    if (get_arg_from_proto_arg(entry_msg, 1, index)) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get index from trace message.");
    }
    uint64_t bo_impl;
    if (get_impl_from_proto_arg(entry_msg->arg(2), bo_impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get bo impl from entry message.");
    }
    auto bo_sh = get_tracked_bo(bo_impl);
    if (!bo_sh) {
      xbtracer_pcritical(entry_msg->name(), "failed to get bo from impl, ",
                         reinterpret_cast<void*>(impl), ".");
    }
    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(impl),
                   ", index: ", index, ", bo impl: ", reinterpret_cast<void*>(bo_impl), ".");
    run_sh->set_arg(index, *bo_sh);
    return 0;
  };

  xbreplay_funcs_map["xrt::run::start(void)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::run::start(void) needs entry and exit, one of them is empty.");
    }
    if (entry_msg->arg_size() < 1) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 2, ".");
    }

    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get run impl from entry message.");
    }
    auto run_sh = get_tracked_run(impl);
    if (!run_sh) {
      xbtracer_pcritical(entry_msg->name(), " failed to get kernel with ",
                         reinterpret_cast<const void*>(impl), ".");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", impl: ",
                   reinterpret_cast<void*>(impl), ".");
    run_sh->start();
    return 0;
  };

  xbreplay_funcs_map["xrt::run::wait2(const std::chrono::milliseconds&)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::run::wait2(const std::chrono::milliseconds&) needs entry and exit, one of them is empty.");
    }
    if (entry_msg->arg_size() < 2) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 2, ".");
    }

    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get run impl from entry message.");
    }
    auto run_sh = get_tracked_run(impl);
    if (!run_sh) {
      xbtracer_pcritical(entry_msg->name(), " failed to get kernel with ",
                         reinterpret_cast<const void*>(impl), ".");
    }

    uint64_t t_ms = 0;
    if (get_arg_from_proto_arg(entry_msg, 1, t_ms)) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get timeout from trace message.");
    }

    std::chrono::milliseconds timeout_ms(t_ms);
    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", impl: ",
                   reinterpret_cast<void*>(impl), ", timeout: ", timeout_ms.count(), "ms.");
    std::cv_status status = run_sh->wait2(timeout_ms);
    if (exit_msg->arg_size() < 2) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of exit args, ", exit_msg->arg_size(),
                         ", ", 2, ".");
    }
    uint32_t t_status = 0;
    if (get_arg_from_proto_arg(exit_msg, 1, t_status)) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get timeout from trace message.");
    }
    if (static_cast<uint32_t>(status) != t_status) {
      xbtracer_pcritical(entry_msg->name(), ", impl: ", reinterpret_cast<void*>(impl),
                         ", status mismatched: ", t_status, ",",
                         static_cast<uint32_t>(status));
    }
    return 0;
  };
}

} // namespace xrt::tools::xbtracer
