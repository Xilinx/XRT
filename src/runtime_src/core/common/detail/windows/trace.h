// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// Implementation of trace infrastructure for MSVC
// This infrastructure leverage native TraceLogging infrastruture.
//
// This header is included in a single (core/common/trace.cpp) compilation
// unit.  It cannot be included in multiple compilation units.
//
// In order to start event tracing enable through xrt.ini or define env:
//
// % set XRT_TRACE_LOGGING_ENABLE
// % tracelog -start <tracename> -guid <guids> -flags <flags> -level <level> -f <file>
// E.g.
// % tracelog -start mytrace -guid guids.guid -flags 0x10 -level 5 -f mytrace.etl
// <run program>
// % tracelog -stop
// % tracefmt mytrace.etl -o mytrace.txt
//
// % cat guids.guid
// e3e140bd-8a94-50be-2264-48e444a715db
// ...

#include "core/common/trace.h"
#include <memory>
#include <windows.h>
#include <TraceLoggingProvider.h>

// [System.Diagnostics.Tracing.EventSource]::new("XRT").Guid
// e3e140bd-8a94-50be-2264-48e444a715db
TRACELOGGING_DEFINE_PROVIDER(
  g_logging_provider,
  "XRT",
  (0xe3e140bd, 0x8a94, 0x50be, 0x22, 0x64, 0x48, 0xe4, 0x44, 0xa7, 0x15, 0xdb));

namespace xrt_core::detail::trace {

// Trace class definition for windows to tie into TraceLogging infrastructure  
class trace_windows : public trace
{
public:
  void
  log_event(const char* id, const char* value) override
  {
    TraceLoggingWrite(g_logging_provider,
                      "XRTTraceEvent", // must be a string literal
                      TraceLoggingValue(id, "Event"),
                      TraceLoggingValue(value, "State"));
  }
};

// Create a trace object for current thread.  This function is called
// exactly once per thread that leverages tracing.
std::unique_ptr<xrt_core::trace>
create_trace_object()
{
  // Globally initialize windows tracing infrastructure
  struct init
  {
    init()
    {
      TraceLoggingRegister(g_logging_provider);
    }
    ~init()
    {
      TraceLoggingUnregister(g_logging_provider);
    }
  };

  static init s_init;
  return std::make_unique<trace_windows>();
}

} // xrt_core::detail

