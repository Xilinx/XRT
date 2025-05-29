// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#include "core/common/time.h"
#include <chrono>
#include <tuple>
#include <windows.h>

namespace xrt_core {

class systime_impl
{
  using timepoint = xrt_core::systime::timepoint;

  FILETIME m_kernel_time, m_user_time;
  uint64_t m_start_time;

  static uint64_t
  to_uint64(const FILETIME& ft)
  {
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart;
  }

public:
  systime_impl()
  {
    start();
  }

  void
  start()
  {
    FILETIME dummy1, dummy2;
    GetProcessTimes(GetCurrentProcess(), &dummy1, &dummy2, &m_kernel_time, &m_user_time);
    m_start_time = xrt_core::time_ns();
  }

  std::tuple<timepoint, timepoint, timepoint>
  get_rusage()
  {
    FILETIME dummy1, dummy2;
    FILETIME kernel_time, user_time;
    GetProcessTimes(GetCurrentProcess(), &dummy1, &dummy2, &kernel_time, &user_time);
    return {
      xrt_core::time_ns() - m_start_time,
      to_uint64(user_time) - to_uint64(m_user_time),
      to_uint64(kernel_time) - to_uint64(m_kernel_time)
    };
  }
};

} // xrt_core



