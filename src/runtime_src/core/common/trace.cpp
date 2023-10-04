// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE
#include "trace.h"
#include "detail/trace.h"

#include "config_reader.h"

#include <memory>
#include <thread>

namespace {

// Create specific logger if enabled
static std::unique_ptr<xrt_core::trace::logger>
get_logger_object()
{
  if (xrt_core::config::get_trace_logging())
    return xrt_core::trace::detail::create_logger_object();

  return std::make_unique<xrt_core::trace::logger>();
}

} // namespace

namespace xrt_core::trace {

// Per thread logger object  
logger*
get_logger()
{
  static thread_local auto logger_object = get_logger_object();
  return logger_object.get();
}

} // xrt_core::trace
