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

#ifndef xrt_util_time_h_
#define xrt_util_time_h_

#include "xrt/config.h"

namespace xrt {

/**
 * @return
 *   nanoseconds since first call
 */
XRT_EXPORT
unsigned long
time_ns();

/**
 * Simple time guard to accumulate scoped time
 */
class time_guard
{
  unsigned long zero = 0;
  unsigned long& tally;
public:
  explicit
  time_guard(unsigned long& t)
    : zero(time_ns()), tally(t)
  {}

  ~time_guard()
  {
    tally += time_ns() - zero;
  }
};

} // xocl

#endif
