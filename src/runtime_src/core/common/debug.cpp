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
#define XRT_CORE_COMMON_SOURCE
#include "debug.h"
#include <cstdarg>
#include <cstdio>

namespace xrt_core {

static std::recursive_mutex s_debug_mutex;

debug_lock::debug_lock()
  : m_lk(s_debug_mutex)
{}

void
debugf(const char* format,...)
{
  debug_lock lk;
  va_list args;
  va_start(args,format);
  printf("%llu: ",time_ns());
  vprintf(format,args);
  va_end(args);
}

} // xrt_core
