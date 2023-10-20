// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_TRACE_HANDLE_H
#define XRT_CORE_TRACE_HANDLE_H

#include <utility>

////////////////////////////////////////////////////////////////
// namespace xrt_core::trace
//
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
namespace xrt_core::trace {

// class logger - base class for managing trace logging
//
// Implementation of class logging is platform specific. Logging objects
// are created per thread and logs to platform specific infrastructure.
//
// The trace logging class collects statistics from all threads using
// XRT. The statistics are collected in a thread safe manner.
class logger
{
public:
  virtual ~logger()
  {}

  // TBD
};

// get_logger() - Return trace logger object for current thread
//
// Creates the logger object if necessary as thread local object.
// It is undefined behavior to delete the returned object.
//
// Access to underlying trace object is to facilitate caching
// to avoid repeated calls to get_logger() where applicable.  
logger*
get_logger();

} // xrt_core::trace


#include "core/common/detail/trace.h"

////////////////////////////////////////////////////////////////
// Platform specific trace point logging
//
// Windows:
// Uses WPP tracing infrastructure on Windows.  The trace infrastructure
// must be initialized before launching the application.
// Enable using xrt.ini or environment variable
// % cat xrt.ini
// [Runtime]
// trace_logging = true
//
// % set XRT_TRACE_LOGGING_ENABLE=1
//
// Linux:
// Uses DTRACE on Linux
// Enable and record with perf tool
////////////////////////////////////////////////////////////////
namespace xrt_core::trace {

// add_event() - Add a trace event
//
// @args - variadic arguments, where first argument is the event name
//
// Example:
// xrt_core::trace::add_event(__func__, ...);
//
// Platform specific implementation
// Integrated into WPP tracing on Windows
// Not implemented on Linux
//
// This function can only be used in windows specific code (shim)
template <typename ...Args>
void
add_event(Args&&... args)
{
  detail::add_event(std::forward<Args>(args)...);
}

} // xrt_core::trace

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
