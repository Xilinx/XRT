// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_USAGE_METRICS_H
#define XRT_CORE_USAGE_METRICS_H

#include <chrono>
#include <map>
#include <memory>
#include <stdint.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

//#include "core/include/xrt/xrt_hw_context.h"
#include "core/include/xrt/xrt_uuid.h"

////////////////////////////////////////////////////////////////
// namespace xrt_core::usage_metrics
//
// Used for printing XRT usage metrics of Application.
//
// This information is printed at the end of the application run
// and the infrastructure must be initialized before launching the
// application using xrt.ini
//
// % cat xrt.ini
// [Runtime]
// usage_metrics_logging = true
////////////////////////////////////////////////////////////////
namespace xrt_core::usage_metrics {

struct bo_metrics
{
  uint32_t total_count = 0;
  uint32_t active_count = 0;
  size_t   total_size_in_bytes = 0;
  uint32_t peak_count = 0;
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
    tp end_time;
  };

  std::string kernel_name;
  std::vector<uint32_t> cu_index_vec;
  uint32_t total_runs;
  std::unordered_map<uintptr_t, struct timestamp> exec_times;
  uint32_t num_args;
};

struct hw_ctx_metrics
{
  uintptr_t id;  // using hw_ctx handle ptr as unique identifier for logging
  xrt::uuid xclbin_uuid;
  struct bo_metrics bos_met;
  std::vector<struct kernel_metrics> kernel_metrics_vec;
};

struct device_metrics
{
  std::string bdf;
  struct bo_metrics global_bos_met;
  std::vector<struct hw_ctx_metrics> hw_ctx_vec;
};

using metrics_map = std::map<std::thread::id, std::map<unsigned int, device_metrics>>;

// class base_logger - class with no op calls
//
// when user doesn't set ini option logging should be no op
class base_logger
{
  public:
    virtual inline void log_device_info(unsigned int) {}
    //virtual inline void log_hw_ctx_info(xrt::hw_context_impl*) {}
    virtual inline void log_hw_ctx_info(unsigned int, uintptr_t, const xrt::uuid&) {}
    virtual inline void log_buffer_info_construct(unsigned int,size_t, uintptr_t) {}
    virtual inline void log_buffer_info_destruct() {}
};

// class usage_metrics_logger - class for logging usage metrics
//
// Logging objects are created per thread 
//
// This class collects metrics from all threads using XRT
// The metrics are collected in a thread safe manner.
class usage_metrics_logger : public base_logger
{
public:
  usage_metrics_logger();
  ~usage_metrics_logger();

  void log_device_info(unsigned int) override;
  //void log_hw_ctx_info(xrt::hw_context_impl*) override;
  void log_hw_ctx_info(unsigned int, uintptr_t, const xrt::uuid&) override;
  void log_buffer_info_construct(unsigned int, size_t, uintptr_t) override;
  void log_buffer_info_destruct() override;

private:
  std::map<unsigned int, device_metrics> m_dev_map;
  std::shared_ptr<metrics_map> map_ptr;
};

// get_logger() - Return trace logger object for current thread
//
// Creates the logger object if necessary as thread local object.
// It is undefined behavior to delete the returned object.
//
// Access to underlying trace object is to facilitate caching
// to avoid repeated calls to get_logger() where applicable.
std::shared_ptr<base_logger>
get_usage_metrics_logger();

} // xrt_core::usage_metrics

#endif
