// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.
#include "core/common/trace.h"

#define SDT_USE_VARIADIC
#include <sys/sdt.h>

#define XRT_DETAIL_TRACE_POINT_LOG(probe, ...) \
  STAP_PROBEV(xrt, probe##_log, ##__VA_ARGS__)

#define XRT_DETAIL_TRACE_POINT_SCOPE(probe)                             \
  struct xrt_trace_scope {                  /* NOLINT */                \
    xrt_trace_scope()                       /* NOLINT */                \
    { DTRACE_PROBE(xrt, probe##_enter); }   /* NOLINT */                \
    ~xrt_trace_scope()                      /* NOLINT */                \
    { DTRACE_PROBE(xrt, probe##_exit); }    /* NOLINT */                \
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


