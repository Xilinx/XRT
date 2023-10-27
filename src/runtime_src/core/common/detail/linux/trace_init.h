// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

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
// % export XRT_TRACE_LOGGING_ENABLE=1

namespace xrt_core::trace::detail {

// Initialize trace logging.  This function is called exactly once
// from common/trace.cpp during static initialization.
inline void
init_trace_logging()
{
}
  
// Deinitialize trace logging.  This function is called exactly once
// from common/trace.cpp during static destruction.
inline void
deinit_trace_logging()
{
}

} // namespace xrt_core::trace::detail
