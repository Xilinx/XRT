// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_USAGE_METRICS_H
#define XRT_CORE_USAGE_METRICS_H

#include <chrono>
#include <map>
#include <memory>
#include <cstdint>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "core/include/ert.h"
#include "core/include/xrt.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "core/include/xrt/xrt_kernel.h"
#include "core/include/xrt/xrt_uuid.h"

// forward declaration of xrt_core::device class
namespace xrt_core {
class device;
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

// class base_logger - class with no op calls
//
// when user doesn't set ini option logging should be no op
class base_logger
{
public:
  virtual void 
  log_device_info(std::shared_ptr<xrt_core::device>) {}

  virtual void
  log_hw_ctx_info(const xrt::hw_context_impl*) {}

  virtual void 
  log_buffer_info_construct(unsigned int, size_t, const xrt_core::hwctx_handle*) {}
  
  virtual void 
  log_buffer_info_destruct(unsigned int) {}

  virtual void
  log_buffer_sync(unsigned int, const xrt_core::hwctx_handle*, size_t, xclBOSyncDirection) {}

  virtual void
  log_kernel_info(std::shared_ptr<xrt_core::device>, const xrt::hw_context&, const std::string&, size_t) {}

  virtual void
  log_kernel_run_info(const xrt::kernel_impl*, const xrt::run_impl*, bool, ert_cmd_state) {}
};

// get_usage_metrics_logger() - Return logger object for current thread
//
// Creates the logger object as thread local object.
// It is undefined behavior to delete the returned object.
//
// Access to underlying logger object is to facilitate caching
// to avoid repeated calls to get_usage_metrics_logger() where applicable.
std::shared_ptr<base_logger>
get_usage_metrics_logger();

} // xrt_core::usage_metrics

#endif
