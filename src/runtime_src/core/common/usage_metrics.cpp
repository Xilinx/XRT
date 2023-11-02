// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE
#include "config_reader.h"
#include "usage_metrics.h"

#include "core/common/query.h"
#include "core/common/query_requests.h"

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

void
print_json(boost::property_tree::ptree pt)
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
    boost::property_tree::json_parser::write_json(oss, pt);

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
        std::cout << "\t\t\t\t peak count : " << ctx.bos_met.peak_count << std::endl;
        std::cout << "\t\t\t\t total size : " << ctx.bos_met.total_size_in_bytes << std::endl;
      }
    }
  }
}

void
print_usage_metrics()
{
  print_json({});
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
log_device_info(unsigned int index)
{
  if (m_dev_map.find(index) == m_dev_map.end()) {
    m_dev_map[index] = {};
  }
}

#if 0
void
usage_metrics_logger::
log_hw_ctx_info(xrt::hw_context_impl* hw_ctx)
{
  auto dev_id = hw_ctx->get_core_device()->get_device_id();
  uintptr_t ctx_id = std::reinterpret_cast<uintptr_t>(hw_ctx);

  // this condition is not required as device will be present
  if (auto map_it = m_dev_map.find(dev_id); map_it != m_dev_map.end()) {
    if (auto it = std::find_if(
        map_it->hw_ctx_vec.begin(), map_it->hw_ctx_vec.end(), 
            [ctx_id](hw_ctx_metrics& ctx) {return ctx.id == ctx_id}); it == map_it->hw_ctx_vec.end()) {
      map_it->hw_ctx_vec.emplace_back(ctx_id, hw_ctx->get_uuid(), {}, {});
    }
  }
}
#endif

void
usage_metrics_logger::
log_hw_ctx_info(unsigned int dev_id, uintptr_t ctx_id, const xrt::uuid& uuid)
{
  // this condition is not required as device will be present
  if (auto map_it = m_dev_map.find(dev_id); map_it != m_dev_map.end()) {
    if (auto ctx_it = std::find_if(
        map_it->second.hw_ctx_vec.begin(), map_it->second.hw_ctx_vec.end(), 
            [ctx_id](hw_ctx_metrics& ctx) {return ctx.id == ctx_id;}); ctx_it == map_it->second.hw_ctx_vec.end()) {
      map_it->second.hw_ctx_vec.emplace_back(hw_ctx_metrics{ctx_id, uuid, {}, {}});
    }
  }
}

void
usage_metrics_logger::
log_buffer_info_construct(unsigned int dev_id, size_t sz, uintptr_t ctx_id)
{
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
      bo_met->active_count++;
      bo_met->total_size_in_bytes += sz;
      bo_met->peak_count = std::max(bo_met->peak_count, bo_met->active_count);
      bo_met->peak_size_in_bytes = std::max(bo_met->peak_size_in_bytes, sz);
    }
  }
}

void
usage_metrics_logger::
log_buffer_info_destruct()
{
}

} // xrt_core::trace
