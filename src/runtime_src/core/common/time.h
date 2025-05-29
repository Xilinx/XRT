// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2017 Xilinx, Inc. All rights reserved.
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrtcore_util_time_h_
#define xrtcore_util_time_h_

#include "core/common/config.h"
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>

namespace xrt_core {

/**
 * @return
 *   nanoseconds since first call
 */
XRT_CORE_COMMON_EXPORT
unsigned long long
time_ns();

/**
 * @return formatted timestamp
 */
XRT_CORE_COMMON_EXPORT
std::string
timestamp();

/**
 * @return timestamp for epoch
 */
XRT_CORE_COMMON_EXPORT
std::string
timestamp(uint64_t epoch);

/**
 * Simple time guard to accumulate scoped time
 */
class time_guard
{
  unsigned long long zero = 0;
  unsigned long long& tally;
public:
  explicit
  time_guard(unsigned long long& t)
    : zero(time_ns()), tally(t)
  {}

  ~time_guard()
  {
    tally += time_ns() - zero;
  }
};


class systime_impl;
class systime
{
  std::unique_ptr<systime_impl> m_impl;
public:
  class timepoint
  {
    uint64_t m_nanoseconds;

  public:
    timepoint(uint64_t nsec)
      : m_nanoseconds{nsec}
    {}

    double
    to_nsec() const { return static_cast<double>(m_nanoseconds); }

    double
    to_usec() const { return static_cast<double>(m_nanoseconds) / 1e3; }

    double
    to_msec() const { return static_cast<double>(m_nanoseconds) / 1e6; }

    double
    to_sec() const { return static_cast<double>(m_nanoseconds) / 1e9; }
  };

  using real_time = timepoint;
  using user_time = timepoint;
  using system_time = timepoint;

  XRT_CORE_COMMON_EXPORT
  systime();

  XRT_CORE_COMMON_EXPORT
  ~systime();

  XRT_CORE_COMMON_EXPORT
  void
  restart();

  // real, user, sys
  XRT_CORE_COMMON_EXPORT
  std::tuple<real_time, user_time, system_time>
  get_rusage();
}; // class systime

} // xrt_core

#endif
