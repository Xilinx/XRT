// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#include "core/common/time.h"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <tuple>

#include <sys/resource.h>

namespace xrt_core {

class systime_impl
{
  using timepoint = xrt_core::systime::timepoint;

  timeval m_kernel_time, m_user_time;
  uint64_t m_start_time;

  static uint64_t
  to_nsec(const timeval& tv)
  {
    return tv.tv_sec * 1e9 + tv.tv_usec * 1e3;
  }

public:
  systime_impl()
  {
    start();
  }

  void
  start()
  {
    struct rusage usage {};
    getrusage(RUSAGE_SELF, &usage);
    std::memcpy(&m_user_time, &usage.ru_utime, sizeof(timeval));
    std::memcpy(&m_kernel_time, &usage.ru_stime, sizeof(timeval));
    m_start_time = xrt_core::time_ns();
  }

  std::tuple<timepoint, timepoint, timepoint>
  get_rusage()
  {
    struct rusage usage {};
    getrusage(RUSAGE_SELF, &usage);
    return {
      xrt_core::time_ns() - m_start_time,
      to_nsec(usage.ru_utime) - to_nsec(m_user_time),
      to_nsec(usage.ru_stime) - to_nsec(m_kernel_time)
    };
  }
};

} // xrt_core



