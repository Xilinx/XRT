// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// Initialization of trace infrastructure for MSVC.
// This infrastructure leverage native TraceLogging infrastruture.
//
// ****************************************************************
// * This header is included in a single (core/common/trace.cpp) compilation
// * unit.  It CANNOT be included in multiple compilation units.
// ****************************************************************
//
// In order to start event tracing enable through xrt.ini or define env:
// % cat xrt.ini
// [Runtime]
// trace_logging = true
//
// % set XRT_TRACE_LOGGING_ENABLE=1
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

#include <windows.h>
#include <TraceLoggingProvider.h>

// [System.Diagnostics.Tracing.EventSource]::new("XRT").Guid
// e3e140bd-8a94-50be-2264-48e444a715db
TRACELOGGING_DEFINE_PROVIDER(
  g_logging_provider,
  "XRT",
  (0xe3e140bd, 0x8a94, 0x50be, 0x22, 0x64, 0x48, 0xe4, 0x44, 0xa7, 0x15, 0xdb));

namespace xrt_core::trace::detail {

// Initialize trace logging.  This function is called exactly once
// from common/trace.cpp during static initialization.
inline void
init_trace_logging()
{
  TraceLoggingRegister(g_logging_provider);
}
  
// Deinitialize trace logging.  This function is called exactly once
// from common/trace.cpp during static destruction.
inline void
deinit_trace_logging()
{
  TraceLoggingUnregister(g_logging_provider);
}

} // namespace xrt_core::trace::detail
