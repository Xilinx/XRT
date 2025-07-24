// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <cstring>
#include <iostream>
#include <replay/xbreplay_common.h>

namespace xrt::tools::xbtracer
{
replayer::
replayer()
{
  register_bo_func();
  register_device_func();
  register_hw_context_func();
  register_kernel_func();
  register_run_func();
  register_xclbin_func();
}

std::function<int(const xbtracer_proto::Func*, const xbtracer_proto::Func*)>
replayer::
get_func_from_signature(std::string func_s)
{
  auto it = xbreplay_funcs_map.find(func_s);
  if (it == xbreplay_funcs_map.end()) {
    return nullptr;
  }
  return it->second;
}

int
replayer::
replay(const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg)
{
  if (!entry_msg) {
    xbtracer_pcritical("Entry function message is null.");
  }
  auto func = get_func_from_signature(entry_msg->name());
  if (!func) {
    xbtracer_pinfo("No map function: ", entry_msg->name(), ".");
    return 0;
  }
  return func(entry_msg, exit_msg);
}

int
replayer::
track(std::shared_ptr<xrt::bo>& obj, uint64_t impl)
{
  return track(obj, impl, bo_tracker);
}

int
replayer::
track(std::shared_ptr<xrt::device>& obj, uint64_t impl)
{
  return track(obj, impl, dev_tracker);
}

int
replayer::
track(std::shared_ptr<xrt::hw_context>& obj, uint64_t impl)
{
  return track(obj, impl, hw_context_tracker);
}

int
replayer::
track(std::shared_ptr<xrt::kernel>& obj, uint64_t impl)
{
  return track(obj, impl, kernel_tracker);
}

int
replayer::
track(std::shared_ptr<xrt::run>& obj, uint64_t impl)
{
  return track(obj, impl, run_tracker);
}

int
replayer::
track(std::shared_ptr<xrt::xclbin>& obj, uint64_t impl)
{
  return track(obj, impl, xclbin_tracker);
}

int
replayer::
track_xclbin_uuid(uint64_t impl, std::string uuid_str)
{
  std::lock_guard<std::mutex> lock(trackers_mlock);
  auto it = xclbin_uuids.find(uuid_str);
  if (it != xclbin_uuids.end()) {
    const auto& t_impl = it->second;
    if (impl != t_impl) {
      xbtracer_perror("xclbin uuid: ", uuid_str, "impl mismatch: ", std::hex, impl, ",", std::hex, impl, ".");
      return -1;
    }
  }
  xclbin_uuids[uuid_str] = impl;
  return 0;
}

int
replayer::
track_kernel_group_id(int traced_id, int replay_id)
{
  std::lock_guard<std::mutex> lock(trackers_mlock);
  auto it = kernel_group_ids.find(traced_id);
  if (it != kernel_group_ids.end()) {
    if (it->second != replay_id) {
      xbtracer_perror("kernel group id: ", std::hex, traced_id, " has been tracked as ",
                      std::hex, it->second, ", ", std::hex, replay_id, ".");
      return -1;
    }
    return 0;
  }
  kernel_group_ids[traced_id] = replay_id;
  return 0;
}

int
replayer::
track_device_handle(uint64_t traced_h, uint64_t impl)
{
  std::lock_guard<std::mutex> lock(trackers_mlock);
  auto it = device_handles.find(traced_h);
  if (it != device_handles.end()) {
    if (it->second != impl) {
      xbtracer_perror("device handle: ", std::hex, traced_h, " has been tracked as ",
                      std::hex, it->second, ", ", std::hex, impl, ".");
      return -1;
    }
    return 0;
  }
  device_handles[traced_h] = impl;
  return 0;
}

void
replayer::
untrack_all()
{
  bo_tracker.clear();
  run_tracker.clear();
  kernel_tracker.clear();
  hw_context_tracker.clear();
  xclbin_tracker.clear();
  dev_tracker.clear();
}

std::shared_ptr<xrt::bo>
replayer::
get_tracked_bo(uint64_t impl)
{
  return get_tracked_obj(impl, bo_tracker);
}

std::shared_ptr<xrt::device>
replayer::
get_tracked_device(uint64_t impl)
{
  return get_tracked_obj(impl, dev_tracker);
}

std::shared_ptr<xrt::hw_context>
replayer::
get_tracked_hw_context(uint64_t impl)
{
  return get_tracked_obj(impl, hw_context_tracker);
}

std::shared_ptr<xrt::kernel>
replayer::
get_tracked_kernel(uint64_t impl)
{
  return get_tracked_obj(impl, kernel_tracker);
}

std::shared_ptr<xrt::run>
replayer::
get_tracked_run(uint64_t impl)
{
  return get_tracked_obj(impl, run_tracker);
}

std::shared_ptr<xrt::xclbin>
replayer::
get_tracked_xclbin(uint64_t impl)
{
  return get_tracked_obj(impl, xclbin_tracker);
}

std::shared_ptr<xrt::xclbin>
replayer::
get_tracked_xclbin(std::string uuid_str)
{
  uint64_t xclbin_impl = 0;
  {
    std::lock_guard<std::mutex> lock(trackers_mlock);
    auto it = xclbin_uuids.find(uuid_str);
    if (it == xclbin_uuids.end()) {
      return std::shared_ptr<xrt::xclbin>(nullptr);
    }
    xclbin_impl = it->second;
  }
  return get_tracked_xclbin(xclbin_impl);
}

int
replayer::
get_tracked_kernel_group_id(int traced_id)
{
    std::lock_guard<std::mutex> lock(trackers_mlock);
    auto it = kernel_group_ids.find(traced_id);
    if (it == kernel_group_ids.end()) {
      return -1;
    }
    return it->second;
}

std::shared_ptr<xrt::device>
replayer::
get_tracked_device_from_handle(uint64_t traced_h)
{
  uint64_t dev_impl = 0;
  {
    std::lock_guard<std::mutex> lock(trackers_mlock);
    auto it = device_handles.find(traced_h);
    if (it == device_handles.end()) {
      return std::shared_ptr<xrt::device>(nullptr);
    }
    dev_impl = it->second;
  }
  return get_tracked_device(dev_impl);
}

void
replayer::
untrack_bo(uint64_t impl)
{
  untrack(impl, bo_tracker);
}

void
replayer::
untrack_device(uint64_t impl)
{
  untrack(impl, dev_tracker);
}

void
replayer::
untrack_hw_context(uint64_t impl)
{
  untrack(impl, hw_context_tracker);
}

void
replayer::
untrack_kernel(uint64_t impl)
{
  untrack(impl, kernel_tracker);
}

void
replayer::
untrack_run(uint64_t impl)
{
  untrack(impl, run_tracker);
}

void
replayer::
untrack_xclbin(uint64_t impl)
{
  untrack(impl, xclbin_tracker);
}

void
replayer::
add_xclbin_kernel(uint64_t impl, std::string name, const xrt::xclbin::kernel& kernel)
{
  std::lock_guard<std::mutex> lock(trackers_mlock);
  for (const auto& o: xclbin_kernels) {
    const auto& t_impl = std::get<0>(o);
    const auto& t_name = std::get<1>(o);
    if (impl == t_impl && name == t_name) {
      return;
    }
  }
  std::tuple<uint64_t, std::string, xrt::xclbin::kernel> t(impl, name, kernel);
  xclbin_kernels.push_back(t);
  return;
}

int
get_impl_from_proto_arg(const xbtracer_proto::Arg& arg, uint64_t& impl)
{
  if (arg.value().length() != sizeof(impl)) {
    xbtracer_perror("invalid pimpl size, ", arg.name(), ", size: ", arg.value().size(),
                    ", expected: ", sizeof(impl), ".");
    return -1;
  }
  std::memcpy(&impl, arg.value().data(), sizeof(impl));
  return 0;
}

int
copy_data_from_proto_arg(const xbtracer_proto::Func& func_msg, uint32_t arg_id, void* buf,
                        size_t size)
{
  const xbtracer_proto::Arg& arg = func_msg.arg(arg_id);
  const std::string& data_str = arg.value();
  size_t data_size = data_str.size();
  if (data_size != size) {
    xbtracer_perror(func_msg.name(), ", arg[", arg_id, "] buf size mismatched: ", size, ",",
                    data_size);
    return -1;
  }
  std::memcpy(buf, data_str.data(), size);
  return 0;
}

const void*
get_data_from_proto_arg(const xbtracer_proto::Func& func_msg, uint32_t arg_id, size_t& size)
{
  const xbtracer_proto::Arg& arg = func_msg.arg(arg_id);
  size = arg.value().size();
  return arg.value().data();
}

} //namespace xrt::tools::xbtracer
