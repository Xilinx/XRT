// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

// Implementation of trace infrastructure for MSVC
// This infrastructure leverage native TraceLogging infrastruture.
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
#include "core/common/trace.h"
#include <memory>
#include <windows.h>
#include <TraceLoggingProvider.h>

// Forward declare the logging provider object.  The provider
// is defined in a single compilation unit (core/common/trace.cpp).
TRACELOGGING_DECLARE_PROVIDER(g_logging_provider);

namespace xrt_core::trace::detail {

template <typename ProbeType>
inline void
add_event(ProbeType&& p)
{
  TraceLoggingWrite(g_logging_provider,
                    "XRTTraceEvent", // must be a string literal
                    TraceLoggingValue(std::forward<ProbeType>(p), "Event"));
}

template <typename ProbeType, typename A1>
inline void
add_event(ProbeType&& p, A1&& a1)
{
  TraceLoggingWrite(g_logging_provider,
                    "XRTTraceEvent", // must be a string literal
                    TraceLoggingValue(std::forward<ProbeType>(p), "Event"),
                    TraceLoggingValue(std::forward<A1>(a1), "arg1"));
}

template <typename ProbeType, typename A1, typename A2>
inline void
add_event(ProbeType&& p, A1&& a1, A2&& a2)
{
  TraceLoggingWrite(g_logging_provider,
                    "XRTTraceEvent", // must be a string literal
                    TraceLoggingValue(std::forward<ProbeType>(p), "Event"),
                    TraceLoggingValue(std::forward<A1>(a1), "arg1"),
                    TraceLoggingValue(std::forward<A2>(a1), "arg2"));
}

template<typename ...Args>
inline void
add_event(Args&&... args)
{
  static_assert(sizeof...(args) < 4, "Max 3 arguments supported for add_event");
}

} // xrt_core::detail

#define XRT_DETAIL_TOSTRING_(a) #a
#define XRT_DETAIL_PROBE(a, b) XRT_DETAIL_TOSTRING_(a ## b)

#define XRT_DETAIL_TRACE_POINT_LOG(probe, ...) \
  xrt_core::trace::detail::add_event(#probe, ##__VA_ARGS__)  // VS++ suppresses trailing comma

#define XRT_DETAIL_TRACE_POINT_SCOPE(probe)                                          \
  struct xrt_trace_scope {                                                           \
    xrt_trace_scope()                                                                \
    { xrt_core::trace::detail::add_event(XRT_DETAIL_PROBE(probe, _enter)); }         \
    ~xrt_trace_scope()                                                               \
    { xrt_core::trace::detail::add_event(XRT_DETAIL_PROBE(probe, _exit)); }          \
  } xrt_trace_scope_instance

#define XRT_DETAIL_TRACE_POINT_SCOPE1(probe, arg1)                                   \
  struct xrt_trace_scope1 {                                                          \
    decltype(arg1) a1;                                                               \
    xrt_trace_scope1(decltype(a1) aa1)                                               \
      : a1{aa1}                                                                      \
    { xrt_core::trace::detail::add_event(XRT_DETAIL_PROBE(probe, _enter), a1); }     \
    ~xrt_trace_scope1()                                                              \
    { xrt_core::trace::detail::add_event(XRT_DETAIL_PROBE(probe, _exit)), a1); }     \
  } xrt_trace_scope_instance{arg1}

#define XRT_DETAIL_TRACE_POINT_SCOPE2(probe, arg1, arg2)                             \
  struct xrt_trace_scope2 {                                                          \
    decltype(arg1) a1;                                                               \
    decltype(arg2) a2;                                                               \
    xrt_trace_scope2(decltype(a1) aa1, decltype(a2) aa2)                             \
      : a1{aa1}, a2{aa2}                                                             \
    { xrt_core::trace::detail::add_event(XRT_DETAIL_PROBE(probe, _enter), a1, a2); } \
    ~xrt_trace_scope2()                                                              \
    { xrt_core::trace::detail::add_event(XRT_DETAIL_PROBE(probe, _exit)), a1, a2); } \
  } xrt_trace_scope_instance{arg1, arg2}

