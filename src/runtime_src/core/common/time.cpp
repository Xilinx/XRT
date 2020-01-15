/**
 * Copyright (C) 2016-2017 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#define XRT_CORE_COMMON_SOURCE
#include "time.h"

#include <chrono>
#include <string>
#include <ctime>
#include <cstring>

#ifdef _WIN32
# pragma warning ( disable : 4996 )
#endif
namespace {

// thread safe time conversion to localtime  
static std::tm
localtime(const std::time_t& time)
{
  std::tm tm;
#ifdef _WIN32
  localtime_s(&tm, &time);
#else
  localtime_r(&time, &tm); // POSIX
#endif
  return tm;
}

}

namespace xrt_core {

/**
 * @return
 *   nanoseconds since first call
 */
unsigned long
time_ns()
{
  static auto zero = std::chrono::high_resolution_clock::now();
  auto now = std::chrono::high_resolution_clock::now();
  auto integral_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now-zero).count();
  return static_cast<unsigned long>(integral_duration);
}

/**
 * @return formatted timestamp
 */
std::string
timestamp()
{
  auto time = std::chrono::system_clock::now();
  auto tm = localtime(std::chrono::system_clock::to_time_t(time));
  char buf[64] = {0};
  return std::strftime(buf, sizeof(buf), "%c", &tm)
    ? buf
    : "Time conversion failed";
}

} // xrt
