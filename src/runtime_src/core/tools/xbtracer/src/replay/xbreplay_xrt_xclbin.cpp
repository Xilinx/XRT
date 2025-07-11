// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include <common/trace_utils.h>
#include <replay/xbreplay_common.h>
#ifdef _WIN32
#include <core/common/windows/win_utils.h>
#else
#include <core/common/linux/linux_utils.h>
#endif

namespace xrt::tools::xbtracer
{
void
replayer::
register_xclbin_func()
{
  xbreplay_funcs_map["xrt::xclbin::xclbin(const std::string&)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::xclbin::xclbin(const std::string&) needs entry and exit, one of them is empty.");
    }
    if (entry_msg->arg_size() < 3) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 3, ".");
    }

    std::string xclbin_file = xbtracer_get_timestamp_str() + ".xclbin";
    const xbtracer_proto::Arg& xclbin_arg = entry_msg->arg(2);
    const std::string& xclbin_data = xclbin_arg.value();

    std::ofstream ofile(xclbin_file, std::ios::out | std::ios::binary);
    if (!ofile.is_open()) {
      xbtracer_pcritical(entry_msg->name(), "failed to open file to store xclbin data, ",
                         sys_dep_get_last_err_msg(), ".");
    }

    ofile.write(xclbin_data.data(), xclbin_data.size());
    ofile.close();

    uint64_t impl;
    if (get_impl_from_proto_arg(exit_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get impl from exit message.");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(impl), ".");
    std::shared_ptr<xrt::xclbin> xclbin_sh = std::make_shared<xrt::xclbin>(xclbin_file);
    if (!xclbin_sh) {
      xbtracer_pcritical(entry_msg->name(), "failed to create xclbin with: ", xclbin_file, ".");
    }
    track(xclbin_sh, impl);
    return 0;
  };

  xbreplay_funcs_map["xrt::xclbin::~xclbin()"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    (void)exit_msg;
    if (!entry_msg) {
      xbtracer_pcritical("xrt::xclbin::~xclbin() needs entry, one of them is empty.");
    }
    if (entry_msg->arg_size() < 1) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 1, ".");
    }

    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get impl from entry message.");
    }
    auto xclbin_sh = get_tracked_xclbin(impl);
    if (!xclbin_sh) {
      xbtracer_pcritical(entry_msg->name(), "failed to get xclbin from impl, ",
                         reinterpret_cast<void*>(impl), ".");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(impl), ".");
    untrack_xclbin(impl);
    return 0;
  };

  xbreplay_funcs_map["xrt::xclbin::get_kernels(void)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::xclbin::get_kernels(void) needs entry and exit, one of them is empty.");
    }

    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get impl from exit message.");
    }

    auto xclbin_sh = get_tracked_xclbin(impl);
    if (!xclbin_sh) {
      xbtracer_pcritical(entry_msg->name(), "Not able to find matched xclbin with impl: ",
                         reinterpret_cast<void*>(impl), ".");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(impl), ".");
    auto kernels = xclbin_sh->get_kernels();
    if (kernels.empty()) {
      xbtracer_pcritical(entry_msg->name(), "impl: ", reinterpret_cast<void*>(impl),
                         " empty kernels.");
    }
    for (const auto& k: kernels) {
      auto kname = k.get_name();
      add_xclbin_kernel(impl, kname, k);
      xbtracer_pinfo(entry_msg->name(), ", ", reinterpret_cast<void*>(impl), ", added kernel: ",
                     kname, ".");
    }

    return 0;
  };

  xbreplay_funcs_map["xrt::xclbin::get_uuid(void)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::xclbin::get_kernels(void) needs entry and exit, one of them is empty.");
    }

    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get impl from exit message.");
    }

    auto xclbin_sh = get_tracked_xclbin(impl);
    if (!xclbin_sh) {
      xbtracer_pcritical(entry_msg->name(), " failed to get xclbin with impl: ",
                         reinterpret_cast<void*>(impl), ".");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ",  reinterpret_cast<void*>(impl), ".");
    auto uuid = xclbin_sh->get_uuid();
    const xbtracer_proto::Arg& uuid_arg = exit_msg->arg(1);
    if (uuid.to_string() != uuid_arg.value()) {
      xbtracer_pcritical(entry_msg->name(), ": ", reinterpret_cast<void*>(impl), ",",
                         " uuid mismatched: ", uuid.to_string(), uuid_arg.value());
    }

    return 0;
  };
}

} // namespace xrt::tools::xbtracer
