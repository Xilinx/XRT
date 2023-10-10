// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#include "core/common/trace.h"

#include <memory>

#define SDT_USE_VARIADIC
#include <sys/sdt.h>

// % cat core/common/detail/linux/trace.h
#define XRT_DETAIL_TRACE_POINT_LOG(probe, ...) \
  STAP_PROBEV(xrt, probe##_log, ##__VA_ARGS__)

#define XRT_DETAIL_TRACE_POINT_SCOPE(probe)                             \
  struct xrt_trace_scope {                                              \
    xrt_trace_scope()                                                   \
    { DTRACE_PROBE(xrt, probe##_enter); }                               \
    ~xrt_trace_scope()                                                  \
    { DTRACE_PROBE(xrt, probe##_exit); }                                \
  } xrt_trace_scope_instance

#define XRT_DETAIL_TRACE_POINT_SCOPE1(probe, arg1)                      \
  struct xrt_trace_scope1 {                                             \
    decltype(arg1) a1;                                                  \
    xrt_trace_scope1(decltype(a1) aa1)                                  \
      : a1{aa1}                                                         \
    { DTRACE_PROBE1(xrt, probe##_enter, a1); }                          \
    ~xrt_trace_scope1()                                                 \
    { DTRACE_PROBE1(xrt, probe##_exit, a1); }                           \
  } xrt_trace_scope_instance{arg1}

#define XRT_DETAIL_TRACE_POINT_SCOPE2(probe, arg1, arg2)                \
  struct xrt_trace_scope2 {                                             \
    decltype(arg1) a1;                                                  \
    decltype(arg2) a2;                                                  \
    xrt_trace_scope2(decltype(a1) aa1, decltype(a2) aa2)                \
      : a1{aa1}, a2{aa2}                                                \
    { DTRACE_PROBE2(xrt, probe##_enter, a1, a2); }                      \
    ~xrt_trace_scope2()                                                 \
    { DTRACE_PROBE2(xrt, probe##_exit, a1, a2);  }                      \
  } xrt_trace_scope_instance{arg1, arg2}

namespace xrt_core::trace::detail {

// Trace logger class definition for Linux
class logger_linux : public logger
{
public:
};

// Create a trace object for current thread.  This function is called
// exactly once per thread that leverages tracing.
inline std::unique_ptr<xrt_core::trace::logger>
create_logger_object()
{
  return std::make_unique<logger_linux>();
}

template <typename ProbeType>
inline void
add_event(ProbeType&& p)
{
  throw std::runtime_error("xrt_core::trace::add_event() not supported on Linux");
}

template <typename ProbeType, typename A1>
inline void
add_event(ProbeType&& p, A1&& a1)
{
  throw std::runtime_error("xrt_core::trace::add_event() not supported on Linux");
}

template <typename ProbeType, typename A1, typename A2>
inline void
add_event(ProbeType&& p, A1&& a1, A2&& a2)
{
  throw std::runtime_error("xrt_core::trace::add_event() not supported on Linux");
}

template<typename ...Args>
inline void
add_event(Args&&... args)
{
  static_assert(sizeof...(args) < 4, "Max 3 arguments supported for add_event");
}

} // xrt_core::detail::trace

