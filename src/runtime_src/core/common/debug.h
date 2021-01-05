/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef xrt_core_util_debug_h_
#define xrt_core_util_debug_h_

#include "config.h"
#include "time.h"
#include <ostream>
#include <iomanip>
#include <mutex>

namespace xrt_core {

struct debug_lock
{
  std::lock_guard<std::recursive_mutex> m_lk;
  XRT_CORE_COMMON_EXPORT
  debug_lock();
};

template <typename T>
void debug_notime(std::ostream& ostr, T&& t)
{
  debug_lock lk;
  ostr << t;
}
template <typename T>
void debug(std::ostream& ostr, T&& t)
{
  debug_lock lk;
  ostr << time_ns() << ": " << t;
}
template <typename T, typename ...Args>
void debug_notime(std::ostream& ostr, T&& t, Args&&... args)
{
  debug_lock lk;
  ostr << t;
  debug_notime(ostr,std::forward<Args>(args)...);
}
template <typename T, typename ...Args>
void debug(std::ostream& ostr, T&& t, Args&&... args)
{
  debug_lock lk;
  ostr << time_ns() << ": " << t;
  debug_notime(ostr,std::forward<Args>(args)...);
}
template <typename ...Args>
void sink(Args&&... args)
{}

/**
 * Format debug print to stdout
 */
XRT_CORE_COMMON_EXPORT
void
debugf(const char* format,...);

/**
 * Throw on error
 */
inline void
xassert(const std::string& file, const std::string& line, const std::string& function, const std::string& expr)
{
  std::string msg(file);
  msg
    .append(":").append(line)
    .append(":").append(function)
    .append(":").append(expr);
  throw std::runtime_error(msg);
}

} // xrt_core

#ifdef XRT_VERBOSE
# define XRT_DEBUG(...) xrt_core::debug(__VA_ARGS__)
# define XRT_PRINT(...) xrt_core::debug(__VA_ARGS__)
# define XRT_DEBUGF(format,...) xrt_core::debugf(format, ##__VA_ARGS__)
# define XRT_PRINTF(format,...) xrt_core::debugf(format, ##__VA_ARGS__)
# define XRT_DEBUG_CALL(...) xrt_core::sink(__VA_ARGS__);
# define XRT_CALL(...) xrt_core::sink(__VA_ARGS__);
#else
# define XRT_DEBUG(...)
# define XRT_PRINT(...) xrt_core::debug(__VA_ARGS__)
# define XRT_DEBUGF(...)
# define XRT_PRINTF(format,...) xrt_core::debugf(format, ##__VA_ARGS__)
# define XRT_DEBUG_CALL(...)
# define XRT_CALL(...) xrt_core::sink(__VA_ARGS__);
#endif

#define XRT_ASSERT(expr,msg) ((expr) ? ((void)0) : xrt_core::xassert(__FILE__,std::to_string(__LINE__),__FUNCTION__,#expr))

#endif
