// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_TRACE_HANDLE_H
#define XRT_CORE_TRACE_HANDLE_H

namespace xrt_core {

// class trace - base class for managing event tracing
//
// Implementation of class trace is platform specific. Trace objects
// are created per thread and logs to platform specific infrastructure.
//
// Tracing is instrusive and added specifically where needed.  In order
// to enable tracing define xrt.ini or set an environment variable
//
// % cat xrt.ini
// [Runtime]
// trace_logging = true
//
// To enable through environment variable make sure XRT_TRACE_LOGGING_ENABLE
// is defined.
class trace
{
public:
  virtual ~trace()
  {}

  // Log an event 
  virtual void
  log_event(const char* id, const char* value) = 0;
};

// Get trace object for current thread, creates the object if
// necessary
trace*
get_trace();

} // xrt_core
#endif
