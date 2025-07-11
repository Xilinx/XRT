// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef xbreplay_common_h
#define xbreplay_common_h

#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <tuple>
#include <typeinfo>
#include <condition_variable>

#include <xrt.h>
#include <xrt/xrt_bo.h>
#include <xrt/xrt_aie.h>
#include <xrt/xrt_device.h>
#include <xrt/xrt_hw_context.h>
#include <xrt/xrt_kernel.h>
#include <xrt/xrt_uuid.h>
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
#include <xrt/experimental/xrt_version.h>

#include "func.pb.h"

#include <common/trace_utils.h>

namespace xrt::tools::xbtracer
{
class xbreplay_msg_queue
{
public:
  xbreplay_msg_queue();

  void
  push(const std::shared_ptr<xbtracer_proto::Func>& value);

  bool
  try_pop(std::shared_ptr<xbtracer_proto::Func>& result);

  void
  wait_and_pop(std::shared_ptr<xbtracer_proto::Func>& result);

  bool
  empty();

  void
  end_queue();

private:
  std::queue<std::shared_ptr<xbtracer_proto::Func>> queue{};
  std::mutex mlock;
  std::condition_variable cond;
  uint32_t ended;
};


class replayer
{
public:
  replayer();

  int
  replay(const xbtracer_proto::Func* entry_msg, const xbtracer_proto::Func* exit_msg);

  int
  track(std::shared_ptr<xrt::bo>& obj, uint64_t impl);

  int
  track(std::shared_ptr<xrt::device>& obj, uint64_t impl);

  int
  track(std::shared_ptr<xrt::hw_context>& obj, uint64_t impl);

  int
  track(std::shared_ptr<xrt::kernel>& obj, uint64_t impl);

  int
  track(std::shared_ptr<xrt::run>& obj, uint64_t impl);

  int
  track(std::shared_ptr<xrt::xclbin>& obj, uint64_t impl);

  int
  track_xclbin_uuid(uint64_t impl, std::string uuid_str);

  int
  track_kernel_group_id(int traced_id, int replay_id);

  int
  track_device_handle(uint64_t traced_h, uint64_t impl);

  std::shared_ptr<xrt::bo>
  get_tracked_bo(uint64_t impl);

  std::shared_ptr<xrt::device>
  get_tracked_device(uint64_t impl);

  std::shared_ptr<xrt::hw_context>
  get_tracked_hw_context(uint64_t impl);

  std::shared_ptr<xrt::kernel>
  get_tracked_kernel(uint64_t impl);

  std::shared_ptr<xrt::run>
  get_tracked_run(uint64_t impl);

  std::shared_ptr<xrt::xclbin>
  get_tracked_xclbin(uint64_t impl);

  std::shared_ptr<xrt::xclbin>
  get_tracked_xclbin(std::string uuid_str);

  int
  get_tracked_kernel_group_id(int traced_id);

  std::shared_ptr<xrt::device>
  get_tracked_device_from_handle(uint64_t traced_h);

  void
  add_xclbin_kernel(uint64_t impl, std::string name, const xrt::xclbin::kernel& kernel);

  // we need to explicitly delete all the tracked XRT objects, otherwise in Linux, the application
  // cleanup will crash due to "free(): invalid pointer" when it is cleaning up shared pointers during
  // application is ending.
  void
  untrack_all();

  void
  untrack_bo(uint64_t impl);

  void
  untrack_device(uint64_t impl);

  void
  untrack_hw_context(uint64_t impl);

  void
  untrack_kernel(uint64_t impl);

  void
  untrack_run(uint64_t impl);

  void
  untrack_xclbin(uint64_t impl);

private:
  void
  register_bo_func();

  void
  register_device_func();

  void
  register_hw_context_func();

  void
  register_kernel_func();

  void
  register_run_func();

  void
  register_xclbin_func();

  std::function<int(const xbtracer_proto::Func*, const xbtracer_proto::Func*)>
  get_func_from_signature(std::string func_s);

  template <typename T>
  int
  track(std::shared_ptr<T>& obj, uint64_t impl, std::map<uint64_t, std::shared_ptr<T>>& tracker)
  {
    std::lock_guard<std::mutex> lock(trackers_mlock);
    auto it = tracker.find(impl);
    if (it != tracker.end()) {
      const auto& t_obj = it->second;
      if (obj.get() == t_obj.get()) {
          const std::type_info& t_type_info = typeid(T);
          xbtracer_pcritical("failed to track pointer of ", t_type_info.name(), ", impl:",
                             reinterpret_cast<void *>(impl), ", ptr: ", obj.get(),
                             " already in tracker.");
      }
      return 0;
    }
    tracker[impl] = obj;
    return 0;
  }

  template <typename T>
  std::shared_ptr<T>
  get_tracked_obj(uint64_t impl, std::map<uint64_t, std::shared_ptr<T>>& tracker)
  {
    std::lock_guard<std::mutex> lock(trackers_mlock);
    auto it = tracker.find(impl);
    if (it != tracker.end()) {
      return it->second;
    }
    return std::shared_ptr<T>(nullptr);
  }

  template <typename T>
  void
  untrack(uint64_t impl, std::map<uint64_t, std::shared_ptr<T>>& tracker)
  {
    std::lock_guard<std::mutex> lock(trackers_mlock);
    auto it = tracker.find(impl);
    if (it != tracker.end()) {
      tracker.erase(it);
    }
  }

  std::mutex trackers_mlock;
  std::map<std::string,
           std::function<int(const xbtracer_proto::Func*,
                         const xbtracer_proto::Func*)>> xbreplay_funcs_map{};
  std::map<uint64_t, std::shared_ptr<xrt::bo>> bo_tracker{};
  std::map<uint64_t, std::shared_ptr<xrt::device>> dev_tracker{};
  std::map<uint64_t, std::shared_ptr<xrt::hw_context>> hw_context_tracker{};
  std::map<uint64_t, std::shared_ptr<xrt::kernel>> kernel_tracker{};
  std::map<uint64_t, std::shared_ptr<xrt::run>> run_tracker{};
  std::map<uint64_t, std::shared_ptr<xrt::xclbin>> xclbin_tracker{};
  std::vector<std::tuple<uint64_t, std::string, xrt::xclbin::kernel>> xclbin_kernels{};
  std::map<std::string, uint64_t> xclbin_uuids{};
  std::map<int, int> kernel_group_ids{};
  std::map<uint64_t, uint64_t> device_handles{};
};

int
get_impl_from_proto_arg(const xbtracer_proto::Arg& arg, uint64_t& impl);

template <typename T>
int
get_arg_from_proto_arg(const xbtracer_proto::Func* func_msg, uint32_t arg_id, T& obj)
{
  const xbtracer_proto::Arg& arg = func_msg->arg(arg_id);
  if (arg.value().length() != sizeof(obj)) {
    xbtracer_perror(func_msg->name(), ", arg[", arg_id, "]: size mismatch: ",
                    arg.value().length(), ",", sizeof(obj));
    return -1;
  }
  std::memcpy(&obj, arg.value().data(), sizeof(obj));
  return 0;
}

int
copy_data_from_proto_arg(const xbtracer_proto::Func& func_msg, uint32_t arg_id, void* buf,
                        size_t size);

const void*
get_data_from_proto_arg(const xbtracer_proto::Func& func_msg, uint32_t arg_id, size_t& size);

void
xbreplay_receive_msgs(std::shared_ptr<replayer>& replayer_sh,
                      std::shared_ptr<xbreplay_msg_queue>& queue);

} // namespace xrt::tools::xbtracer

#endif // xbreplay_common_h
