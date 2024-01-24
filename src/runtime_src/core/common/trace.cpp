// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE
#include "detail/trace_init.h"
#include "config_reader.h"

namespace {

// Static global initialization of trace logging.  
struct init
{
  init()
  {
    if (!xrt_core::config::get_trace_logging())
      return;

    xrt_core::trace::detail::init_trace_logging();
  }

  ~init()
  {
    try {
      if (!xrt_core::config::get_trace_logging())
        return;

      xrt_core::trace::detail::deinit_trace_logging();
    }
    catch (...) {
    }
  }
};

static init s_init;

} // namespace

