// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include "core/common/trace.h"
#include <memory>

// Implementation of trace infrastructure for Linux.
// TBD

namespace xrt_core::trace::detail {

// Trace logger class definition for Linux
class logger_linux : public logger
{
public:
  void
  add_event(const char* id, const char* value) override
  {}
};

// Create a trace object for current thread.  This function is called
// exactly once per thread that leverages tracing.
std::unique_ptr<xrt_core::trace::logger>
create_logger_object()
{
  return std::make_unique<logger_linux>();
}

} // xrt_core::detail::trace

