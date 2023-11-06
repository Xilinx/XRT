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

#include "core/include/xrt.h"
#include "core/include/xrt/xrt_uuid.h"
#include "core/include/xrt/xrt_hw_context.h"

// forward declaration of xrt_core::device class
namespace xrt_core {
class device;
}

namespace {
using tp = std::chrono::time_point<std::chrono::high_resolution_clock>;
constexpr tp TP_MIN = std::chrono::high_resolution_clock::time_point::min();
}

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
  size_t   total_size_in_bytes = 0;
  size_t   peak_size_in_bytes = 0;
  size_t   bytes_synced_to_device = 0;
  size_t   bytes_synced_from_device = 0;
};

struct kernel_metrics
{
  struct timestamp
  {
    tp start_time = TP_MIN;
    tp end_time = TP_MIN;
  };

  std::string name;
  std::vector<uint32_t> cu_index_vec;
  uint32_t total_runs = 0;
  double total_time = 0;
  std::unordered_map<uintptr_t, struct timestamp> exec_times;
  size_t num_args;
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
  std::string bdf = "";
  struct bo_metrics global_bos_met;
  uint32_t bo_active_count = 0;
  uint32_t bo_peak_count = 0;
  std::vector<struct hw_ctx_metrics> hw_ctx_vec;
};

using metrics_map = std::map<std::thread::id, std::map<unsigned int, device_metrics>>;

// class base_logger - class with no op calls
//
// when user doesn't set ini option logging should be no op
class base_logger
{
public:
  virtual void 
  log_device_info(std::shared_ptr<xrt_core::device>) {}

  virtual void
  log_hw_ctx_info(void*) {}

  virtual void 
  log_buffer_info_construct(unsigned int,size_t, void*) {}
  
  virtual void 
  log_buffer_info_destruct(unsigned int) {}

  virtual void
  log_buffer_sync(unsigned int, void*, size_t, xclBOSyncDirection) {}

  virtual void
  log_kernel_info(std::shared_ptr<xrt_core::device>, const xrt::hw_context&, const std::string&, size_t) {}

  virtual void
  log_kernel_start_info(void*, void*) {}

  virtual void
  log_kernel_end_info(void*) {}
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

  void
  log_device_info(std::shared_ptr<xrt_core::device>) override;

  void 
  log_hw_ctx_info(void*) override;

  void 
  log_buffer_info_construct(unsigned int, size_t, void*) override;

  void 
  log_buffer_info_destruct(unsigned int) override;

  virtual void
  log_buffer_sync(unsigned int, void*, size_t, xclBOSyncDirection) override;

  void
  log_kernel_info(std::shared_ptr<xrt_core::device>, const xrt::hw_context&, const std::string&, size_t) override;

  void
  log_kernel_start_info(void*, void*) override;

  virtual void
  log_kernel_end_info(void*) override;

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
