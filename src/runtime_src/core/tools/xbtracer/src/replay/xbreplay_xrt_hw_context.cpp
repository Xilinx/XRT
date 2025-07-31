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
register_hw_context_func()
{
  xbreplay_funcs_map["xrt::hw_context::hw_context(const xrt::device&, const xrt::uuid&, xrt::hw_context::access_mode)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::hw_context::hw_context(const xrt::device&, const xrt::uuid&, xrt::hw_context::access_mode) needs entry and exit, one of them is empty.");
    }
    if (entry_msg->arg_size() < 4) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(), ", ", 2, ".");
    }
    uint64_t dev_impl;
    if (get_impl_from_proto_arg(entry_msg->arg(1), dev_impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get device impl from entry message.");
    }
    auto dev_sh = get_tracked_device(dev_impl);
    if (!dev_sh) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get tracked device from impl: ", std::hex,
                         dev_impl);
    }

    const xbtracer_proto::Arg& uuid_arg = entry_msg->arg(2);
    std::string uuid_str = uuid_arg.value();
    auto xclbin_sh = get_tracked_xclbin(uuid_str);
    if (!xclbin_sh) {
      xbtracer_pcritical(entry_msg->name(), ", not able get tracked xclbin from uuid: ", uuid_str, ".");
    }
    auto xclbin_uuid = xclbin_sh->get_uuid();

    xrt::hw_context::access_mode mode = xrt::hw_context::access_mode::exclusive;
    if (get_arg_from_proto_arg(entry_msg, 3, mode)) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get access mode from message.");
    }

    uint64_t impl;
    if (exit_msg->arg_size() < 1) {
      xbtracer_pcritical(exit_msg->name(), " invalid number of exit args, ", exit_msg->arg_size(), ", ", 1, ".");
    }
    if (get_impl_from_proto_arg(exit_msg->arg(0), impl)) {
      xbtracer_pcritical(exit_msg->name(), " failed to get impl from exit message.");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", std::hex, impl, ", dev: ", std::hex, dev_impl,
                   ", uuid: ", uuid_str, ", access_mode: ", std::hex, (uint32_t)mode, ".");
    std::shared_ptr<xrt::hw_context> hw_context_sh = std::make_shared<xrt::hw_context>(*dev_sh, xclbin_uuid, mode);
    track(hw_context_sh, impl);
    return 0;
  };

  xbreplay_funcs_map["xrt::hw_context::~hw_context()"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    (void)exit_msg;
    if (!entry_msg) {
      xbtracer_pcritical("xrt::hw_context::~hw_context() needs entry, one of them is empty.");
    }
    if (entry_msg->arg_size() < 1) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(), ", ", 5, ".");
    }

    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get impl from entry message.");
    }
    auto hw_context_sh = get_tracked_hw_context(impl);
    if (!hw_context_sh) {
      xbtracer_pcritical(entry_msg->name(), "failed to get hw_context from impl, ", std::hex, impl, ".");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", std::hex, impl, ".");
    untrack_hw_context(impl);
    return 0;
  };
}

} // namespace xrt::tools::xbtracer
