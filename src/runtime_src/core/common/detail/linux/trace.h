// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include "core/common/trace.h"
#include <memory>

// Implementation of trace infrastructure for Linux.
// TBD

namespace xrt_core::detail::trace {

// Trace class definition for Linux
class trace_linux : public trace
{
public:
  void
  log_event(const char* id, const char* value) override
  {}
};

// Create a trace object for current thread.  This function is called
// exactly once per thread that leverages tracing.
std::unique_ptr<xrt_core::trace>
create_trace_object()
{
  return std::make_unique<trace_linux>();
}

} // xrt_core::detail

