// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE
#include "config_reader.h"
#include "usage_metrics.h"

#include "core/common/api/hw_context_int.h"
#include "core/common/api/kernel_int.h"
#include "core/common/device.h"
#include "core/common/query.h"
#include "core/common/query_requests.h"
#include "core/common/shim/buffer_handle.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "core/include/xrt/xrt_kernel.h"

#include <algorithm>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

#ifdef _WIN32
# pragma warning ( disable : 4996 )
#endif

namespace bpt = boost::property_tree;
namespace {

// global variables
std::mutex m;
static uint32_t thread_count;

// Create specific logger if enabled
static std::shared_ptr<xrt_core::usage_metrics::base_logger>
get_logger_object()
{
  if (xrt_core::config::get_usage_metrics_logging())
    return std::make_shared<xrt_core::usage_metrics::usage_metrics_logger>();

  return std::make_shared<xrt_core::usage_metrics::base_logger>();
}

} // namespace

namespace xrt_core::usage_metrics {

std::shared_ptr<metrics_map> usage_metrics_map = std::make_shared<metrics_map>();

static void
print_json(const bpt::ptree& pt)
{
  // create json in pwd
  auto current_time = std::chrono::system_clock::now();
  std::time_t time = std::chrono::system_clock::to_time_t(current_time);

  std::stringstream time_stamp;
  time_stamp << std::put_time(std::localtime(&time), "%Y-%m-%d_%H-%M-%S");

  std::string file_name{"XRT_usage_metrics_" + time_stamp.str() + ".json"};
  std::ofstream out_file{file_name};

  if (out_file.is_open()) {
    std::ostringstream oss;
    bpt::json_parser::write_json(oss, pt);

    out_file << oss.str();
    out_file.close();
  }
  else {
    std::cerr << "Failed to create file : " << file_name << std::endl;
  }

  // debug print
  for (auto it : *usage_metrics_map) {
    std::cout << "printing for thread id : " << it.first << std::endl;
    for (auto dev_it : it.second) {
      std::cout << "\t device id : " << dev_it.first << std::endl;
      for (auto ctx : dev_it.second.hw_ctx_vec) {
        std::cout << "\t\t ctx id : " << ctx.id << std::endl;
        std::cout << "\t\t\t uuid : " << ctx.xclbin_uuid.to_string() << std::endl;
        std::cout << "\t\t\t bos info :" << std::endl;
        std::cout << "\t\t\t\t total count : " << ctx.bos_met.total_count << std::endl;
        //std::cout << "\t\t\t\t peak count : " << ctx.bos_met.peak_count << std::endl;
        std::cout << "\t\t\t\t total size : " << ctx.bos_met.total_size_in_bytes << std::endl;
        std::cout << "\n\t\t\t kernels : " << std::endl;
        for (auto k : ctx.kernel_metrics_vec) {
            std::cout << "\t\t\t\t name : " << k.name << std::endl;
            std::cout << "\t\t\t\t num args : " << k.num_args << std::endl;
        }
      }
    }
  }
}

static bpt::ptree
get_bos_ptree(const struct bo_metrics& bo_met)
{
  bpt::ptree bo_tree;

  bo_tree.add("total_count", bo_met.total_count);
  bo_tree.add("size", std::to_string(bo_met.total_size_in_bytes) + " bytes");
  bo_tree.add("avg_size", std::to_string(bo_met.total_size_in_bytes / bo_met.total_count) + "bytes");
  bo_tree.add("peak_size", std::to_string(bo_met.peak_size_in_bytes) + "bytes");
  bo_tree.add("bytes_synced_to_device", std::to_string(bo_met.bytes_synced_to_device) + "bytes");
  bo_tree.add("bytes_synced_from_device", std::to_string(bo_met.bytes_synced_from_device) + "bytes");

  return bo_tree;
}

static bpt::ptree
get_hw_ctx_ptree(const std::vector<struct hw_ctx_metrics>&)
{
  bpt::ptree hw_ctx_array;

  return hw_ctx_array;
}

static void
print_usage_metrics()
{
  bpt::ptree thread_array;

  uint32_t t_count = 0;
  // iterate over all threads
  for (auto thread_it : *usage_metrics_map) {
    bpt::ptree dev_array;
    //iterate over all devices
    for (auto dev_it : thread_it.second) {
      bpt::ptree dev;
      dev.put("device index", std::to_string(dev_it.first));
      dev.put("bdf", dev_it.second.bdf);
      dev.put("bos_peak_count", std::to_string(dev_it.second.bo_peak_count));

      // add global bos
      dev.add_child("global_bos", get_bos_ptree(dev_it.second.global_bos_met));

      // add hw ctx info
      dev.add_child("hw context", get_hw_ctx_ptree(dev_it.second.hw_ctx_vec));

      dev_array.push_back(std::make_pair("device " + std::to_string(dev_it.first), dev));
    }

    //thread_array.push_back(std::make_pair(std::to_string(thread_count), dev_array));
    thread_array.add_child("thread " + std::to_string(t_count), dev_array);
    t_count++;
  }

  print_json(thread_array);
}

// Per thread logger object  
std::shared_ptr<base_logger>
get_usage_metrics_logger()
{
  static thread_local auto usage_logger_object = get_logger_object();
  return usage_logger_object;
}

usage_metrics_logger::
usage_metrics_logger() : map_ptr(usage_metrics_map)
{
  thread_count++;
}

usage_metrics_logger::
~usage_metrics_logger()
{
  {
    std::lock_guard<std::mutex> lk(m);
    thread_count--;

    // push this threads usage metrics to global map
    // in thread safe manner
    (*map_ptr)[std::this_thread::get_id()] = std::move(m_dev_map);
  }

  if (thread_count == 0) {
    // print usage metrics log
    print_usage_metrics();
  }
}

void
usage_metrics_logger::
log_device_info(std::shared_ptr<xrt_core::device> dev)
{
  auto dev_id = dev->get_device_id();
  if (m_dev_map.find(dev_id) == m_dev_map.end()) {
    m_dev_map[dev_id] = {};
    m_dev_map[dev_id].bdf = dev->get_xclbin_uuid();
  }
}

void 
usage_metrics_logger::
log_hw_ctx_info(void* hwctx_impl)
{
  try {
    auto hw_ctx = 
        xrt_core::hw_context_int::create_hw_context_from_implementation(hwctx_impl);
    
    auto handle = xrt_core::hw_context_int::get_hwctx_handle(hw_ctx);
    uintptr_t ctx_id = reinterpret_cast<uintptr_t>(handle);
    auto dev_id = xrt_core::hw_context_int::get_core_device(hw_ctx)->get_device_id();
    auto uuid = xrt_core::hw_context_int::get_xclbin_uuid(hw_ctx);

    // this condition is not required as device will be present
    if (auto map_it = m_dev_map.find(dev_id); map_it != m_dev_map.end()) {
      if (auto ctx_it = std::find_if(
          map_it->second.hw_ctx_vec.begin(), map_it->second.hw_ctx_vec.end(), 
              [ctx_id](hw_ctx_metrics& ctx) {return ctx.id == ctx_id;}); ctx_it == map_it->second.hw_ctx_vec.end()) {
        map_it->second.hw_ctx_vec.emplace_back(hw_ctx_metrics{ctx_id, uuid, {}, {}});
      }
    }
  }
  catch(...) {
    // dont log anything
  }
}

void
usage_metrics_logger::
log_buffer_info_construct(unsigned int dev_id, size_t sz, void* ctx)
{
  auto ctx_id = ctx ? reinterpret_cast<uintptr_t>(ctx) : 0;
  struct bo_metrics* bo_met = nullptr;

  if (auto map_it = m_dev_map.find(dev_id); map_it != m_dev_map.end()) {
    if (!ctx_id) {
      // global bo
      bo_met = &map_it->second.global_bos_met;
    }

    if (auto ctx_it = std::find_if(
        map_it->second.hw_ctx_vec.begin(), map_it->second.hw_ctx_vec.end(), 
            [ctx_id](hw_ctx_metrics& ctx) {return ctx.id == ctx_id;}); ctx_it != map_it->second.hw_ctx_vec.end()) {
      bo_met = &ctx_it->bos_met;
    }

    if (bo_met) {
      bo_met->total_count++;
      bo_met->total_size_in_bytes += sz;
      bo_met->peak_size_in_bytes = std::max(bo_met->peak_size_in_bytes, sz);
      // increase active count in case of global or ctx bound bo
      map_it->second.bo_active_count++;
      map_it->second.bo_peak_count = 
          std::max(map_it->second.bo_peak_count, map_it->second.bo_active_count);
    }
  }
}

void
usage_metrics_logger::
log_buffer_info_destruct(unsigned int)
{
  // This call is needed only to decrement bo active count
  // add changes in all shims, buffer handle to hold device index
  // when buffer_handle is destroyed pass device index so we can 
  // decrement active count
}

void
usage_metrics_logger::
log_buffer_sync(unsigned int dev_id, void* ctx, size_t sz, xclBOSyncDirection dir)
{
  auto ctx_id = ctx ? reinterpret_cast<uintptr_t>(ctx) : 0;
  struct bo_metrics* bo_met = nullptr;

  if (auto map_it = m_dev_map.find(dev_id); map_it != m_dev_map.end()) {
    if (!ctx_id) {
      // global bo
      bo_met = &map_it->second.global_bos_met;
    }

    if (auto ctx_it = std::find_if(
        map_it->second.hw_ctx_vec.begin(), map_it->second.hw_ctx_vec.end(), 
            [ctx_id](hw_ctx_metrics& ctx) {return ctx.id == ctx_id;}); ctx_it != map_it->second.hw_ctx_vec.end()) {
      bo_met = &ctx_it->bos_met;
    }

    if (bo_met) {
      if (dir == XCL_BO_SYNC_BO_TO_DEVICE)
        bo_met->bytes_synced_to_device += sz;
      else
        bo_met->bytes_synced_from_device += sz;
    }
  }
}

void
usage_metrics_logger::
log_kernel_info(std::shared_ptr<xrt_core::device> dev, const xrt::hw_context& ctx, const std::string& name, size_t args)
{
  auto dev_id = dev->get_device_id();
  auto handle = xrt_core::hw_context_int::get_hwctx_handle(ctx);
  uintptr_t ctx_id = reinterpret_cast<uintptr_t>(handle);

  // this condition is not required as device will be present
  if (auto map_it = m_dev_map.find(dev_id); map_it != m_dev_map.end()) {
    if (auto ctx_it = std::find_if(
        map_it->second.hw_ctx_vec.begin(), map_it->second.hw_ctx_vec.end(), 
        [ctx_id](hw_ctx_metrics& ctx) {return ctx.id == ctx_id;}); ctx_it != map_it->second.hw_ctx_vec.end()) {
      if (auto kernel_it = std::find_if(
          ctx_it->kernel_metrics_vec.begin(), ctx_it->kernel_metrics_vec.end(),
          [name](kernel_metrics& kernel) {return kernel.name == name;}); kernel_it == ctx_it->kernel_metrics_vec.end()) {
        struct kernel_metrics k;
        k.name = name;
        k.num_args = args;
        ctx_it->kernel_metrics_vec.emplace_back(k);
      } 
    }
  }
}

void
usage_metrics_logger::
log_kernel_start_info(void* krnl_impl, void* run_hdl)
{
  try {
    auto kernel =
        xrt_core::kernel_int::create_kernel_from_implementation(krnl_impl);

    auto hw_ctx = xrt_core::kernel_int::get_hw_ctx(kernel);
    auto handle = xrt_core::hw_context_int::get_hwctx_handle(hw_ctx);

    uintptr_t ctx_id = reinterpret_cast<uintptr_t>(handle);
    auto dev_id = xrt_core::hw_context_int::get_core_device(hw_ctx)->get_device_id();
    auto name = xrt_core::kernel_int::get_kernel_name(kernel);
    uintptr_t r_hdl = reinterpret_cast<uintptr_t>(run_hdl);

    // this condition is not required as device will be present
    if (auto map_it = m_dev_map.find(dev_id); map_it != m_dev_map.end()) {
      if (auto ctx_it = std::find_if(
          map_it->second.hw_ctx_vec.begin(), map_it->second.hw_ctx_vec.end(), 
              [ctx_id](hw_ctx_metrics& ctx) {return ctx.id == ctx_id;}); ctx_it != map_it->second.hw_ctx_vec.end()) {
        if (auto kernel_it = std::find_if(
            ctx_it->kernel_metrics_vec.begin(), ctx_it->kernel_metrics_vec.end(),
            [name](kernel_metrics& kernel) {return kernel.name == name;}); kernel_it != ctx_it->kernel_metrics_vec.end()) {
          kernel_it->exec_times[r_hdl].start_time = std::chrono::high_resolution_clock::now();
        }
      }
    }
  }
  catch(...) {
    // dont log anything
  }
}

void
usage_metrics_logger::
log_kernel_end_info(void*)
{
  // increment num runs here as we completed one valid run
  // add time duration to total time and make start time to zero
}

} // xrt_core::trace
