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

#ifndef xrt_util_thread_h_
#define xrt_util_thread_h_

#include <thread>

namespace xrt { 

namespace detail {

/**
 * Set a threads policy as specified in sdaccel.ini, or default 
 * if not specified.
 * 
 * This function is not for public use
 */
void
set_thread_policy(std::thread& thread);

/**
 * Pin a thread to specified cpus per sdaccel.ini, or all if not specified
 */
void
set_cpu_affinity(std::thread& thread);

}

/**
 * Construct a thread and set policy according to sdaccel.ini
 * 
 * This function has exactly the same inteface as std::thread 
 * constructor, all arguments are forwarded to std::thread ctor.
 *
 * If thread policy is not specified, then use default policy
 * Policies supported in .ini file are rr,fifo, or other.  For 
 * example:
 *  [Runtime]
 *   thread_policy = rr 
 */
template <typename ...Args>
std::thread
thread(Args&&... args)
{
  auto t = std::thread(std::forward<Args>(args)...);
  detail::set_thread_policy(t);
  detail::set_cpu_affinity(t);
  return t;
}

  
} // xrt


#endif


