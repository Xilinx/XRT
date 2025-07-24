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
register_device_func()
{
  xbreplay_funcs_map["xrt::device::device(unsigned int)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::device::device(unsigned int) needs entry and exit, one of them is empty.");
    }
    if (entry_msg->arg_size() < 2) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 2, ".");
    }
    uint32_t id = 0;
    if (get_arg_from_proto_arg(entry_msg, 1, id)) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get input id from trace message.");
    }

    uint64_t impl;
    if (exit_msg->arg_size() < 1) {
      xbtracer_pcritical(exit_msg->name(), " invalid number of exit args, ", exit_msg->arg_size(),
                         ", ", 1, ".");
    }
    if (get_impl_from_proto_arg(exit_msg->arg(0), impl)) {
      xbtracer_pcritical(exit_msg->name(), " failed to get impl from exit message.");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(impl),
                   ", id: ", id, ".");
    std::shared_ptr<xrt::device> dev_sh = std::make_shared<xrt::device>(id);
    if (!dev_sh) {
      xbtracer_pcritical(entry_msg->name(), "failed to create device with id: ", id, ".");
    }
    track(dev_sh, impl);
    return 0;
  };

  xbreplay_funcs_map["xrt::device::~device()"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    (void)exit_msg;
    if (!entry_msg) {
      xbtracer_pcritical("xrt::device::~device() needs entry, one of them is empty.");
    }
    if (entry_msg->arg_size() < 1) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 5, ".");
    }

    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get impl from entry message.");
    }
    auto dev_sh = get_tracked_device(impl);
    if (!dev_sh) {
      xbtracer_pcritical(entry_msg->name(), "failed to get device from impl, ",
                         reinterpret_cast<void*>(impl), ".");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(impl), ".");
    untrack_device(impl);
    return 0;
  };

  xbreplay_funcs_map["xrt::device::register_xclbin(const xrt::xclbin&)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::device::register_xclbin(const xrt::xclbin&) needs entry and exit, one of them is empty.");
    }
    if (entry_msg->arg_size() < 2) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 2, ".");
    }

    uint64_t dev_impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), dev_impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get device impl from entry message.");
    }

    uint64_t xclbin_impl;
    if (get_impl_from_proto_arg(entry_msg->arg(1), xclbin_impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get xclbin impl from entry message.");
    }

    auto dev_sh = get_tracked_device(dev_impl);
    if (!dev_sh) {
      xbtracer_pcritical(entry_msg->name(), " failed to get device with ",
                         reinterpret_cast<void*>(dev_impl), ".");
    }

    auto xclbin_sh = get_tracked_xclbin(xclbin_impl);
    if (!xclbin_sh) {
      xbtracer_pcritical(entry_msg->name(), " failed to get xclbin with ",
                         reinterpret_cast<void*>(xclbin_impl), ".");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(dev_impl),
                   ", xclbin: ", reinterpret_cast<void*>(xclbin_impl), ".");
    auto uuid = dev_sh->register_xclbin(*xclbin_sh);

    const xbtracer_proto::Arg& uuid_arg = exit_msg->arg(1);
    if (uuid.to_string() != uuid_arg.value()) {
      xbtracer_pcritical(entry_msg->name(), ": ", reinterpret_cast<void*>(dev_impl), ",",
                         reinterpret_cast<void*>(xclbin_impl), ", uuid mismatched: ",
                         uuid.to_string(), uuid_arg.value());
    }
    xbtracer_pinfo(entry_msg->name(), ", xclbin uuid: ", uuid_arg.value(), ", ", uuid.to_string(),
                   ".");
    if (track_xclbin_uuid(xclbin_impl, uuid_arg.value())) {
      xbtracer_pcritical(entry_msg->name(), ", ", reinterpret_cast<void*>(dev_impl),
                         "failed to track xclbin uuid.");
    }
    return 0;
  };

  xbreplay_funcs_map["xrt::device::operator xclDeviceHandle(void)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::device::operator xclDeviceHandle(void) needs entry and exit, one of them is empty.");
    }
    if (entry_msg->arg_size() < 1) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 1, ".");
    }

    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get device impl from entry message.");
    }

    auto dev_sh = get_tracked_device(impl);
    if (!dev_sh) {
      xbtracer_pcritical(entry_msg->name(), " failed to get device with ",
                         reinterpret_cast<void*>(impl), ".");
    }

    if (exit_msg->arg_size() < 2) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of exit args, ", exit_msg->arg_size(),
                         ", ", 2, ".");
    }
    uint64_t dev_h_ret = 0;
    if (get_arg_from_proto_arg(exit_msg, 1, dev_h_ret)) {
      xbtracer_pcritical(entry_msg->name(),
                         ", failed to get returned device handle from trace message.");
    }
    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(impl),
                   ", dev handle: ",  reinterpret_cast<void*>(dev_h_ret));
    // this is a type conversion, no need to replay

    if (track_device_handle(dev_h_ret, impl)) {
      xbtracer_pcritical(entry_msg->name(), ", ", std::hex, impl, "failed to track device handle.");
    }
    return 0;
  };
}

} // namespace xrt::tools::xbtracer
