/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#ifndef xocl_core_time_h_
#define xocl_core_time_h_

#include "xrt/util/time.h"

namespace xocl {

/**
 * @return
 *   nanoseconds since first call
 */
inline unsigned long long
time_ns() 
{ 
  return xrt_xocl::time_ns(); 
}

using time_guard = xrt_xocl::time_guard;

} // xocl

#endif


