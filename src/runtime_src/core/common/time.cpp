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
#include <cstring>
#include <ctime>

#ifdef _WIN32
# pragma warning ( disable : 4996 )
#endif
namespace {
  
static std::tm*
get_gmtime(const std::time_t& time)
{
  std::tm* tm;
  tm = gmtime(&time); 
  return tm;
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
  return std::strftime(buf, sizeof(buf), "%c GMT", tm)
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

} // xrt_core
