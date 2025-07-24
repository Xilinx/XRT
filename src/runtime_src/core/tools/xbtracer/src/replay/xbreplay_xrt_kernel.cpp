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
register_kernel_func()
{
  xbreplay_funcs_map["xrt::kernel::kernel(const xrt::hw_context&, const std::string&)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::kernel::kernel(const xrt::hw_context&, const std::string&) needs entry and exit, one of them is empty.");
    }
    if (entry_msg->arg_size() < 3) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 3, ".");
    }
    uint64_t hw_context_impl;
    if (get_impl_from_proto_arg(entry_msg->arg(1), hw_context_impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get hw_context impl from entry message.");
    }
    auto hw_context_sh = get_tracked_hw_context(hw_context_impl);
    if (!hw_context_sh) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get tracked hw_context from impl: ",
                         reinterpret_cast<void*>(hw_context_impl));
    }

    const xbtracer_proto::Arg& name_arg = entry_msg->arg(2);
    std::string name_str = name_arg.value();

    uint64_t impl;
    if (exit_msg->arg_size() < 1) {
      xbtracer_pcritical(exit_msg->name(), " invalid number of exit args, ", exit_msg->arg_size(),
                         ", ", 1, ".");
    }
    if (get_impl_from_proto_arg(exit_msg->arg(0), impl)) {
      xbtracer_pcritical(exit_msg->name(), " failed to get impl from exit message.");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(impl),
                   ", hw_context: ", reinterpret_cast<void*>(hw_context_impl), ", name: ",
                   name_str, ".");
    std::shared_ptr<xrt::kernel> kernel_sh = std::make_shared<xrt::kernel>(*hw_context_sh,
                                                                           name_str);
    if (!hw_context_sh) {
      xbtracer_pcritical(entry_msg->name(), "failed to create hw_context.");
    }
    track(kernel_sh, impl);
    return 0;
  };

  xbreplay_funcs_map["xrt::kernel::~kernel()"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    (void)exit_msg;
    if (!entry_msg) {
      xbtracer_pcritical("xrt::kernel::~kernel() needs entry, one of them is empty.");
    }
    if (entry_msg->arg_size() < 1) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 1, ".");
    }

    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get impl from entry message.");
    }
    auto kernel_sh = get_tracked_kernel(impl);
    if (!kernel_sh) {
      xbtracer_pcritical(entry_msg->name(), "failed to get kernel from impl, ",
                         reinterpret_cast<void*>(impl), ".");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(impl), ".");
    untrack_kernel(impl);
    return 0;
  };

  xbreplay_funcs_map["xrt::kernel::group_id(int)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::kernel::group_id(int) needs entry and exit, one of them is empty.");
    }
    if (entry_msg->arg_size() < 2) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 2, ".");
    }
    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get kernel impl from entry message.");
    }
    auto kernel_sh = get_tracked_kernel(impl);
    if (!kernel_sh) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get tracked kernel from impl: ",
                         reinterpret_cast<void*>(impl));
    }

    int i_id = 0;
    if (get_arg_from_proto_arg(entry_msg, 1, i_id)) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get input group id from trace message.");
    }

    int o_id = 0;
    if (get_arg_from_proto_arg(exit_msg, 1, o_id)) {
      xbtracer_pcritical(entry_msg->name(),
                         ", failed to get returned group id from trace message.");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(impl), ", id: ",
                   i_id, ".");
    auto ret_id = kernel_sh->group_id(i_id);
    if (track_kernel_group_id(o_id, ret_id)) {
      xbtracer_pcritical(entry_msg->name(), "impl: ", reinterpret_cast<void*>(impl),
                         " failed to track group id.");
    }
    return 0;
  };
}

} // namespace xrt::tools::xbtracer
