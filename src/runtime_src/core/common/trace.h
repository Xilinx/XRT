// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_TRACE_HANDLE_H
#define XRT_CORE_TRACE_HANDLE_H

#include "core/common/detail/trace.h"

////////////////////////////////////////////////////////////////
// Trace logging for XRT.  Implementation is platform specific.
//
// Trace logging is instrusive and added specifically where needed.
//
// The trace infrastructure must be initialized before launching the
// application (platform specific requirement). Enable using xrt.ini
// or environment variable:
//
// % cat xrt.ini
// [Runtime]
// trace_logging = true
//
// % set XRT_TRACE_LOGGING_ENABLE=1
////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////
// Trace point macros
// Windows:
// Uses WPP tracing infrastructure on Windows.
// Requires initialization of trace infrastructure before launching.
//
// Linux:
// Uses DTRACE on Linux
// Enable and record with perf tool
////////////////////////////////////////////////////////////////

// Add a single trace point 
#define XRT_TRACE_POINT_LOG(probe, ...) \
  XRT_DETAIL_TRACE_POINT_LOG(probe, ##__VA_ARGS__)

// Scoped trace points
// Create a scoped object that that a tracepoint when created
// and when destroyed.  The variants support 0, 1, or 2 arguments.
#define XRT_TRACE_POINT_SCOPE(probe) \
  XRT_DETAIL_TRACE_POINT_SCOPE(probe)

#define XRT_TRACE_POINT_SCOPE1(probe, a1) \
  XRT_DETAIL_TRACE_POINT_SCOPE1(probe, a1)

#define XRT_TRACE_POINT_SCOPE2(probe, a1, a2) \
  XRT_DETAIL_TRACE_POINT_SCOPE2(probe, a1, a2)

#endif
