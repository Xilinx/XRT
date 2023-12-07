// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_API_SOURCE
#define XCL_DRIVER_DLL_EXPORT
#define XRT_CORE_COMMON_SOURCE
#include "config_reader.h"
#include "usage_metrics.h"

#include "core/common/api/hw_context_int.h"
#include "core/common/api/kernel_int.h"
#include "core/common/device.h"
#include "core/common/query.h"
#include "core/common/query_requests.h"
#include "core/common/shim/buffer_handle.h"
#include "core/common/shim/hwctx_handle.h"
#include "core/common/utils.h"
#include "core/include/xrt/xrt_uuid.h"

#include <algorithm>
#include <atomic>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

#ifdef _WIN32
# pragma warning ( disable : 4996 )
#endif

namespace bpt = boost::property_tree;

namespace {
// global variables
static std::mutex m;
static std::atomic<uint32_t> thread_count {0};

template <typename MetType, typename FindType>
static MetType*
get_metrics(std::vector<MetType>& metrics_vec, const FindType& finder)
{
  auto it = std::find_if(metrics_vec.begin(), metrics_vec.end(), 
                        [finder](const auto& met) 
                        {
                          return met.handle == finder;
                        }
                        );
  return it == metrics_vec.end()
    ? nullptr
    : &(*it);
}

struct bo_metrics
{
  uint32_t total_count = 0;
  size_t   total_size_in_bytes = 0;
  size_t   peak_size_in_bytes = 0;
  size_t   bytes_synced_to_device = 0;
  size_t   bytes_synced_from_device = 0;
};

struct kernel_metrics
{
  using tp = std::chrono::time_point<std::chrono::high_resolution_clock>;
  struct timestamp
  {
    tp start_time;
    bool is_valid = false;
  };

  std::string handle; // kernel name is used as handle for identifying kernel
  std::vector<uint32_t> cu_index_vec;
  uint32_t total_runs = 0;
  std::chrono::microseconds total_time = {};
  std::unordered_map<const xrt::run_impl*, timestamp> exec_times; // run handle ptr is used for indexing
  size_t num_args;

  void
  log_kernel_exec_time(const xrt::run_impl* run_hdl, const tp& tp_now, ert_cmd_state state)
  {
    // state ERT_CMD_STATE_NEW indicates kernel start is called
    if (state == ERT_CMD_STATE_NEW) {
      // record start everytime because previous run may be finished, timeout, aborted or stopped
      exec_times[run_hdl].start_time = tp_now;
      exec_times[run_hdl].is_valid = true;
    }
    else {
      // make start time invalid so we can record for next run, add duration to total time and increment total runs
      if (exec_times[run_hdl].is_valid && state == ERT_CMD_STATE_COMPLETED) {
        // valid run increament run
        total_runs++;
        total_time += std::chrono::duration_cast<std::chrono::microseconds>(tp_now - exec_times[run_hdl].start_time);

        // invalidate start time, run may be finished, aborted or timed out
        exec_times[run_hdl].is_valid = false;
      }
    }
  }
};

struct hw_ctx_metrics
{
  const xrt_core::hwctx_handle* handle;  // using hw_ctx handle ptr as unique identifier for logging
  xrt::uuid xclbin_uuid;
  bo_metrics bos_met;
  std::vector<kernel_metrics> kernel_metrics_vec;

  void
  log_kernel(const std::string& name, size_t args)
  {
    kernel_metrics k;
    k.handle = name;
    k.num_args = args;
    kernel_metrics_vec.emplace_back(k);
  }
};

struct device_metrics
{
  std::string bdf = "";
  bo_metrics global_bos_met;
  uint32_t bo_active_count = 0;
  uint32_t bo_peak_count = 0;
  std::vector<hw_ctx_metrics> hw_ctx_vec;

  void
  log_hw_ctx(const xrt_core::hwctx_handle* handle, const xrt::uuid& uuid)
  {
    hw_ctx_vec.emplace_back(hw_ctx_metrics{handle, uuid, {}, {}});
  }
};

using metrics_map = std::map<std::thread::id, std::map<device_id, device_metrics>>;
using device_metrics_map = std::map<device_id, device_metrics>;
// Global map to store usage metrics of all threads
static auto usage_metrics_map = std::make_shared<metrics_map>();

// Helper functions to get metrics
static device_metrics*
get_device_metrics(device_metrics_map& dev_map, device_id dev_id)
{
  auto map_it = dev_map.find(dev_id);
  return map_it == dev_map.end()
    ? nullptr
    : &(*map_it).second;
}

static bo_metrics*
get_buffer_metrics(device_metrics* dev_metrics, const xrt_core::hwctx_handle* handle)
{
  if (!handle) {
    // global bo
    return &dev_metrics->global_bos_met;
  }
  else {
    auto hw_ctx_met = get_metrics(dev_metrics->hw_ctx_vec, handle);
    if (hw_ctx_met != nullptr)
      return &hw_ctx_met->bos_met;
  }
  return nullptr;
}

// Helper functions to print usage metrics as json
static void
print_json(const bpt::ptree& pt)
{
  auto current_time = std::chrono::system_clock::now();
  std::time_t time = std::chrono::system_clock::to_time_t(current_time);

  std::stringstream time_stamp;
  time_stamp << std::put_time(std::localtime(&time), "%Y-%m-%d_%H-%M-%S");

  // create json in pwd
  // file name format - XRT_usage_metrics_##pid_YY-MM-DD_H-M-S.json
  std::string file_name{"XRT_usage_metrics_" + std::to_string(xrt_core::utils::get_pid()) + "_" + time_stamp.str() + ".json"};
  std::ofstream out_file{file_name};

  if (out_file.is_open()) {
    std::ostringstream oss;
    bpt::json_parser::write_json(oss, pt);

    out_file << oss.str();
    out_file.close();
  }
  else {
    std::cerr << "Failed to create Usage metrics logger file : " << file_name << std::endl;
  }
}

static bpt::ptree
get_bos_ptree(const bo_metrics& bo_met)
{
  bpt::ptree bo_tree;

  bo_tree.add("total_count", bo_met.total_count);
  bo_tree.add("size", std::to_string(bo_met.total_size_in_bytes) + " bytes");

  auto avg_size = (bo_met.total_count > 0) ? (bo_met.total_size_in_bytes / bo_met.total_count) : 0;
  bo_tree.add("avg_size", std::to_string(avg_size) + " bytes");

  bo_tree.add("peak_size", std::to_string(bo_met.peak_size_in_bytes) + " bytes");
  bo_tree.add("bytes_synced_to_device", std::to_string(bo_met.bytes_synced_to_device) + " bytes");
  bo_tree.add("bytes_synced_from_device", std::to_string(bo_met.bytes_synced_from_device) + " bytes");

  return bo_tree;
}

static bpt::ptree
get_kernels_ptree(const std::vector<kernel_metrics>& kernels_vec)
{
  bpt::ptree kernel_array;

  for (const auto& kernel : kernels_vec) {
    bpt::ptree kernel_tree;

    kernel_tree.put("name", kernel.handle);
    kernel_tree.put("num_of_args", kernel.num_args);
    kernel_tree.put("num_total_runs", std::to_string(kernel.total_runs));

    auto avg_run_time = (kernel.total_runs > 0) ? (kernel.total_time.count() / kernel.total_runs) : 0;
    kernel_tree.put("avg_run_time", std::to_string(avg_run_time) + " us");

    kernel_array.push_back(std::make_pair("", kernel_tree));
  }

  return kernel_array; 
}

static bpt::ptree
get_hw_ctx_ptree(const std::vector<hw_ctx_metrics>& hw_ctx_vec)
{
  bpt::ptree hw_ctx_array;

  uint32_t ctx_count = 0;
  for (const auto& ctx : hw_ctx_vec) {
    bpt::ptree hw_ctx;
    hw_ctx.put("id", std::to_string(ctx_count));
    hw_ctx.put("xclbin_uuid", ctx.xclbin_uuid.to_string());

    // add buffer info
    hw_ctx.add_child("bos", get_bos_ptree(ctx.bos_met));

    // add kernel info
    hw_ctx.add_child("kernels", get_kernels_ptree(ctx.kernel_metrics_vec));

    hw_ctx_array.push_back(std::make_pair("", hw_ctx));
    ctx_count++;
  }

  return hw_ctx_array;
}

static void
print_usage_metrics()
{
  bpt::ptree thread_array;

  uint32_t t_count = 0;
  // iterate over all threads
  for (const auto& [thread_id, dev_metrics_map] : *usage_metrics_map) {
    bpt::ptree dev_array;
    // iterate over all devices
    for (const auto& [dev_id, dev_metrics] : dev_metrics_map) {
      bpt::ptree dev;
      dev.put("device_index", std::to_string(dev_id));
      dev.put("bdf", dev_metrics.bdf);
      dev.put("bos_peak_count", std::to_string(dev_metrics.bo_peak_count));

      // add global bos
      dev.add_child("global_bos", get_bos_ptree(dev_metrics.global_bos_met));

      // add hw ctx info
      dev.add_child("hw_context", get_hw_ctx_ptree(dev_metrics.hw_ctx_vec));

      dev_array.push_back(std::make_pair("device", dev));
    }

    thread_array.add_child("thread " + std::to_string(t_count), dev_array);
    t_count++;
  }

  print_json(thread_array);
}

// class usage_metrics_logger - class for logging usage metrics
//
// Logging objects are created per thread 
//
// This class collects metrics from all threads using XRT
// The metrics are collected in a thread safe manner.
class usage_metrics_logger : public xrt_core::usage_metrics::base_logger
{
public:
  usage_metrics_logger();

  ~usage_metrics_logger();

  void
  log_device_info(const xrt_core::device*) override;

  void 
  log_hw_ctx_info(const xrt::hw_context_impl*) override;

  void 
  log_buffer_info_construct(device_id, size_t, const xrt_core::hwctx_handle*) override;

  void 
  log_buffer_info_destruct(device_id) override;

  virtual void
  log_buffer_sync(device_id, const xrt_core::hwctx_handle*, size_t, xclBOSyncDirection) override;

  void
  log_kernel_info(const xrt_core::device*, const xrt::hw_context&, const std::string&, size_t) override;

  void
  log_kernel_run_info(const xrt::kernel_impl*, const xrt::run_impl*, ert_cmd_state) override;

private:
  device_metrics_map m_dev_map;
  std::shared_ptr<metrics_map> map_ptr;
};

usage_metrics_logger::
usage_metrics_logger() : map_ptr(usage_metrics_map)
{
  thread_count++;
}

usage_metrics_logger::
~usage_metrics_logger()
{
  thread_count--;
  {
    std::lock_guard<std::mutex> lk(m);
    // push this threads usage metrics to global map
    // in thread safe manner
    (*map_ptr)[std::this_thread::get_id()] = std::move(m_dev_map);
  }

  if (thread_count == 0) {
    // print usage metrics log after all threads are destroyed
    try {
      print_usage_metrics();
    }
    catch (const std::exception& e) {
      std::cerr << " Failed to dump Usage metrics, exception occured - " << e.what() << std::endl;
    }
  }
}

void
usage_metrics_logger::
log_device_info(const xrt_core::device* dev)
{
  auto dev_id = dev->get_device_id();
  if (!get_device_metrics(m_dev_map, dev_id)) {
    // initialize map with this device index
    m_dev_map[dev_id] = {};
    try {
      auto bdf = xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(dev));
      m_dev_map[dev_id].bdf = std::move(bdf);
    }   
    catch (...) {}
  }
}

void 
usage_metrics_logger::
log_hw_ctx_info(const xrt::hw_context_impl* hwctx_impl)
{
  try {
    auto hw_ctx = 
        xrt_core::hw_context_int::create_hw_context_from_implementation(const_cast<xrt::hw_context_impl*>(hwctx_impl));
    
    auto hwctx_handle = static_cast<xrt_core::hwctx_handle*>(hw_ctx);
    auto dev_id = xrt_core::hw_context_int::get_core_device(hw_ctx)->get_device_id();
    auto uuid = hw_ctx.get_xclbin_uuid();

    // dont log if device didn't match
    auto dev_metrics = get_device_metrics(m_dev_map, dev_id);
    if (!dev_metrics)
      return;

    // log if this entry is not logged before
    if (!get_metrics(dev_metrics->hw_ctx_vec, hwctx_handle))
      dev_metrics->log_hw_ctx(hwctx_handle, uuid);
  }
  catch(...) {
    // dont log anything
  }
}

void
usage_metrics_logger::
log_buffer_info_construct(device_id dev_id, size_t sz, const xrt_core::hwctx_handle* handle)
{
  auto dev_metrics = get_device_metrics(m_dev_map, dev_id);
  if (!dev_metrics)
    return;

  bo_metrics* bo_met = get_buffer_metrics(dev_metrics, handle);
  // don't log if bo not found
  if (!bo_met)
    return;

  bo_met->total_count++;
  bo_met->total_size_in_bytes += sz;
  bo_met->peak_size_in_bytes = std::max(bo_met->peak_size_in_bytes, sz);
  // increase active count in case of global or ctx bound bo
  dev_metrics->bo_active_count++;
  dev_metrics->bo_peak_count = 
      std::max(dev_metrics->bo_peak_count, dev_metrics->bo_active_count);
}

void
usage_metrics_logger::
log_buffer_info_destruct(device_id)
{
  // TODO :
  // This call is needed to decrement bo active count
  // This is used for reporting peak count
}

void
usage_metrics_logger::
log_buffer_sync(device_id dev_id, const xrt_core::hwctx_handle* handle, size_t sz, xclBOSyncDirection dir)
{
  auto dev_metrics = get_device_metrics(m_dev_map, dev_id);
  if (!dev_metrics)
    return;

  bo_metrics* bo_met = get_buffer_metrics(dev_metrics, handle);
  // don't log if bo not found
  if (!bo_met)
    return;

  if (dir == XCL_BO_SYNC_BO_TO_DEVICE)
    bo_met->bytes_synced_to_device += sz;
  else
    bo_met->bytes_synced_from_device += sz;
}

void
usage_metrics_logger::
log_kernel_info(const xrt_core::device* dev, const xrt::hw_context& ctx, const std::string& name, size_t args)
{
  auto dev_id = dev->get_device_id();
  auto hwctx_handle = static_cast<xrt_core::hwctx_handle*>(ctx);

  auto dev_metrics = get_device_metrics(m_dev_map, dev_id);
  if (!dev_metrics)
    return;

  auto hw_ctx_met = get_metrics(dev_metrics->hw_ctx_vec, hwctx_handle);
  // dont log if hw ctx didn't match existing ones
  if (!hw_ctx_met)
    return;
  
  // log if this entry is not logged before
  if (!get_metrics(hw_ctx_met->kernel_metrics_vec, name)) {
    hw_ctx_met->log_kernel(name, args);
  }
}

void
usage_metrics_logger::
log_kernel_run_info(const xrt::kernel_impl* krnl_impl, const xrt::run_impl* run_hdl, ert_cmd_state state)
{
  // collecting time at start of call as next calls will be overhead
  auto ts_now = std::chrono::high_resolution_clock::now();
  try {
    auto kernel =
        xrt_core::kernel_int::create_kernel_from_implementation(krnl_impl);

    auto hw_ctx = xrt_core::kernel_int::get_hw_ctx(kernel);
    auto hwctx_handle = static_cast<xrt_core::hwctx_handle*>(hw_ctx);

    auto dev_id = xrt_core::hw_context_int::get_core_device(hw_ctx)->get_device_id();
    auto name = kernel.get_name();

    auto dev_metrics = get_device_metrics(m_dev_map, dev_id);
    if (!dev_metrics)
      return;

    auto hw_ctx_met = get_metrics(dev_metrics->hw_ctx_vec, hwctx_handle);
    // dont log if hw ctx didn't match existing ones
    if (!hw_ctx_met)
      return;
  
    auto kernel_met = get_metrics(hw_ctx_met->kernel_metrics_vec, name);
    if (!kernel_met)
      return;

    kernel_met->log_kernel_exec_time(run_hdl, ts_now, state);
  }
  catch(...) {
    // dont log anything
  }
}

// Create specific logger if ini option is enabled
static std::shared_ptr<xrt_core::usage_metrics::base_logger>
get_logger_object()
{
  if (xrt_core::config::get_usage_metrics_logging())
    return std::make_shared<usage_metrics_logger>();

  return std::make_shared<xrt_core::usage_metrics::base_logger>();
}

} // namespace

namespace xrt_core::usage_metrics {
// Per thread logger object  
std::shared_ptr<base_logger>
get_usage_metrics_logger()
{
  static thread_local auto usage_logger_object = get_logger_object();
  return usage_logger_object;
}
} // xrt_core::usage_metrics
