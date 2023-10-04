// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_TRACE_HANDLE_H
#define XRT_CORE_TRACE_HANDLE_H

namespace xrt_core::trace {

// class logger - base class for managing trace logging
//
// Implementation of class logging is platform specific. Logging objects
// are created per thread and logs to platform specific infrastructure.
//
// Tracing logging is instrusive and added specifically where needed.
// In order to enable trace logging define xrt.ini or set an environment
// variable
//
// % cat xrt.ini
// [Runtime]
// trace_logging = true
//
// To enable through environment variable make sure XRT_TRACE_LOGGING_ENABLE
// is defined.
class logger
{
public:
  virtual ~logger()
  {}

  // Log an event 
  virtual void
  add_event(const char* /*id*/, const char* /*value*/)
  {}
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
#endif
