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

#ifndef xrtcore_util_time_h_
#define xrtcore_util_time_h_

#include "core/common/config.h"
#include <cstdint>
#include <string>

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

} // xrt_core

#endif
