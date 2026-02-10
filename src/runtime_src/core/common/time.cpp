// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2017 Xilinx, Inc. All rights reserved.
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE
#include "time.h"
#include "detail/systime.h" // after time.h, order matters

#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
# pragma warning ( disable : 4996 )
#endif
namespace {

static std::tm
get_gmtime(const std::time_t& time)
{
  std::tm tm_buf{};
#ifdef _WIN32
  gmtime_s(&tm_buf, &time);
#else
  gmtime_r(&time, &tm_buf);
#endif
  return tm_buf;
}

}

namespace xrt_core {

/**
 * @return
 *   nanoseconds since first call
 */
unsigned long long
time_ns()
{
  static auto zero = std::chrono::high_resolution_clock::now();
  auto now = std::chrono::high_resolution_clock::now();
  auto integral_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now-zero).count();
  return static_cast<unsigned long long>(integral_duration);
}

/**
 * @return formatted timestamp
 */
std::string
timestamp()
{
  auto time = std::chrono::system_clock::now();
  auto tm = get_gmtime(std::chrono::system_clock::to_time_t(time));
  char buf[64] = {0};
  return std::strftime(buf, sizeof(buf), "%c GMT", &tm)
    ? buf : "Time conversion failed";
}

/**
 * @return formatted timestamp for epoch
 */
std::string
timestamp(uint64_t epoch)
{
  time_t rawtime = epoch;
  std::string tmp(ctime(&rawtime));
  return tmp.substr( 0, tmp.length() -1).append(" GMT");
}

// returns formatted timestamp string that can be concatenated with
// filename in thread safe and platform independent way
std::string
get_timestamp_for_filename()
{
  constexpr int microseconds_per_second = 1000000;
  constexpr int microsecond_width = 6;

  auto current_time = std::chrono::system_clock::now();
  std::time_t time = std::chrono::system_clock::to_time_t(current_time);

  // Extract microseconds
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(
      current_time.time_since_epoch()) % microseconds_per_second;

  std::tm tm_buf{};
  #ifdef _WIN32
    localtime_s(&tm_buf, &time);
  #else
    localtime_r(&time, &tm_buf);
  #endif

  std::stringstream time_stamp;
  time_stamp << std::put_time(&tm_buf, "%Y-%m-%d_%H-%M-%S")
             << '-' << std::setfill('0') << std::setw(microsecond_width) << us.count();
  return time_stamp.str();
}

////////////////////////////////////////////////////////////////
// systime implementation
// Implmentation is platform specific, the pimpl is inline
// included from detail/time.h
////////////////////////////////////////////////////////////////
systime::
systime()
  : m_impl{std::make_unique<systime_impl>()}
{}

systime::
~systime() = default;

void
systime::
restart()
{
  m_impl->start();
}

// real, user, sys
using timepoint = systime::timepoint;
std::tuple<timepoint, timepoint, timepoint>
systime::
get_rusage()
{
  return m_impl->get_rusage();
}

} // xrt_core
