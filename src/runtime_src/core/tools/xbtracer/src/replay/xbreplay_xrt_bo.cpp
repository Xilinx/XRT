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
register_bo_func()
{
  xbreplay_funcs_map["xrt::bo::bo(xclDeviceHandle, size_t, xrt::bo::flags, xrt::memory_group)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::bo::bo(xclDeviceHandle, size_t, xrt::bo::flags, xrt::memory_group) needs entry and exit, one of them is empty.");
    }
    if (entry_msg->arg_size() < 5) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(), ", ", 5, ".");
    }

    xclDeviceHandle dev_h = nullptr;
    if (get_arg_from_proto_arg(entry_msg, 1, dev_h)) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get device handle from trace message.");
    }
    auto dev_sh = get_tracked_device_from_handle((uint64_t)(uintptr_t)dev_h);
    if (!dev_sh) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get device from traced handle: ", dev_h);
    }
    size_t size = 0;
    if (get_arg_from_proto_arg(entry_msg, 2, size)) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get size from trace message.");
    }
    xrt::bo::flags flags = xrt::bo::flags::normal;
    if (get_arg_from_proto_arg(entry_msg, 3, flags)) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get flags from trace message.");
    }
    xrt::memory_group traced_grp = 0;
    if (get_arg_from_proto_arg(entry_msg, 4, traced_grp)) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get memory group from trace message.");
    }
    xrt::memory_group grp = (xrt::memory_group)get_tracked_kernel_group_id(traced_grp);
    uint64_t impl;
    if (exit_msg->arg_size() < 1) {
      xbtracer_pcritical(exit_msg->name(), " invalid number of exit args, ", exit_msg->arg_size(),
                         ", ", 1, ".");
    }
    if (get_impl_from_proto_arg(exit_msg->arg(0), impl)) {
      xbtracer_pcritical(exit_msg->name(), " failed to get impl from exit message.");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(impl),
                   ", dev_h: ",  reinterpret_cast<void*>(dev_h), ", size: ", std::dec, size,
                   ", flags: ", std::hex, (uint32_t)flags,
                   ", group: ", (uint32_t)grp, ".");
    std::shared_ptr<xrt::bo> bo_sh = std::make_shared<xrt::bo>((xclDeviceHandle)(*dev_sh), size,
                                                               flags, grp);
    track(bo_sh, impl);
    return 0;
  };

  xbreplay_funcs_map["xrt::bo::~bo()"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    (void)exit_msg;
    if (!entry_msg) {
      xbtracer_pcritical("xrt::bo::~bo() needs entry, one of them is empty.");
    }
    if (entry_msg->arg_size() < 1) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 5, ".");
    }

    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get impl from entry message.");
    }
    auto bo_sh = get_tracked_bo(impl);
    if (!bo_sh) {
      xbtracer_pcritical(entry_msg->name(), "failed to get bo from impl, ",
                         reinterpret_cast<void*>(impl), ".");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(impl), ".");
    untrack_bo(impl);
    return 0;
  };

  xbreplay_funcs_map["xrt::bo::map(void)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::bo::map(void) needs entry and exit, one of them is empty.");
    }
    if (entry_msg->arg_size() < 1) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 1, ".");
    }

    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get impl from entry message.");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(impl), ".");
    auto bo_sh = get_tracked_bo(impl);
    if (!bo_sh) {
      xbtracer_pcritical(entry_msg->name(), "failed to get bo from impl, ",
                         reinterpret_cast<void*>(impl), ".");
    }
    (void)bo_sh->map();
    return 0;
  };

  xbreplay_funcs_map["xrt::bo::size(void)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::bo::map(void) needs entry and exit, one of them is empty.");
    }
    if (entry_msg->arg_size() < 1) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 1, ".");
    }

    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get impl from entry message.");
    }
    auto bo_sh = get_tracked_bo(impl);
    if (!bo_sh) {
      xbtracer_pcritical(entry_msg->name(), "failed to get bo from impl, ",
                         reinterpret_cast<void*>(impl), ".");
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(impl), ".");
    auto size = bo_sh->size();
    if (exit_msg->arg_size() < 2) {
      xbtracer_pcritical(exit_msg->name(), " invalid number of args of exit message, ",
                         exit_msg->arg_size(), ", ", 2, ".");
    }
    size_t traced_size = 0;
    if (get_arg_from_proto_arg(exit_msg, 1, traced_size)) {
      xbtracer_pcritical(exit_msg->name(), ", failed to get size from trace message.");
    }
    if (size != traced_size) {
      xbtracer_pcritical(entry_msg->name(), ", size mismatched: ", traced_size, ",", size);
    }
    return 0;
  };

  xbreplay_funcs_map["xrt::bo::sync(xclBOSyncDirection, size_t, size_t)"] =
  [this](const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
  {
    if (!entry_msg || !exit_msg) {
      xbtracer_pcritical("xrt::bo::map(void) needs entry and exit, one of them is empty.");
    }
    if (entry_msg->arg_size() < 4) {
      xbtracer_pcritical(entry_msg->name(), " invalid number of args, ", entry_msg->arg_size(),
                         ", ", 1, ".");
    }

    uint64_t impl;
    if (get_impl_from_proto_arg(entry_msg->arg(0), impl)) {
      xbtracer_pcritical(entry_msg->name(), " failed to get impl from entry message.");
    }
    auto bo_sh = get_tracked_bo(impl);
    if (!bo_sh) {
      xbtracer_pcritical(entry_msg->name(), "failed to get bo from impl, ",
                         reinterpret_cast<void*>(impl), ".");
    }
    uint32_t dir32 = 0;
    if (get_arg_from_proto_arg(entry_msg, 1, dir32)) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get direction from trace message.");
    }
    xclBOSyncDirection dir = static_cast<xclBOSyncDirection>(dir32);
    size_t size = 0;
    if (get_arg_from_proto_arg(entry_msg, 2, size)) {
      xbtracer_pcritical(entry_msg->name(), ", failed to get size from trace message.");
    }
    size_t offset = 0;
    if (get_arg_from_proto_arg(entry_msg, 3, offset))
      xbtracer_pcritical(entry_msg->name(), ", failed to get offset from trace message.");
    if (entry_msg->arg_size() == 5) {
      // entry message contains input data
      void* data = bo_sh->map();
      if (!data) {
        xbtracer_pcritical(entry_msg->name(), ", failed to get input data, failed to map.");
      }
      if (copy_data_from_proto_arg(*entry_msg, 4, data, size)) {
        xbtracer_pcritical(entry_msg->name(), ", failed to get input data from trace message.");
      }
    }

    xbtracer_pinfo("Replaying: ", entry_msg->name(), ", ", reinterpret_cast<void*>(impl),
                   ", dir: ", dir32, ", size: ", std::dec, size, ", offset: ", offset, ".");
    bo_sh->sync(dir, size, offset);
    return 0;
  };
}

} // namespace xrt::tools::xbtracer
