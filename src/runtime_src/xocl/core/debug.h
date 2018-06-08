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

#ifndef xocl_util_debug_h_
#define xocl_util_debug_h_

#include "xrt/util/debug.h"

#include <CL/cl.h>
#include <vector>
#include <iosfwd>

namespace xocl {

#ifdef VERBOSE
void
logf(const char* format,...);
#endif

} // xocl

#ifdef XOCL_VERBOSE
# define XOCL_DEBUG(...) xrt::debug(__VA_ARGS__)
# define XOCL_DEBUGF(format,...) xrt::debugf(format, ##__VA_ARGS__)
# define XOCL_PRINT(...) xrt::debug(__VA_ARGS__)
# define XOCL_PRINTF(format,...) xrt::debugf(format, ##__VA_ARGS__)
#else
# define XOCL_DEBUG(...)
# define XOCL_DEBUGF(...)
# define XOCL_PRINT(...) xrt::debug(__VA_ARGS__)
# define XOCL_PRINTF(format,...) xrt::debugf(format, ##__VA_ARGS__)
#endif

#ifdef VERBOSE
# define XOCL_LOGF(format,...) ::xocl::logf(format, ##__VA_ARGS__)
# define XOCL_LOG(format,...) ::xocl::logf(format, ##__VA_ARGS__)
#else
# define XOCL_LOGF(...)
# define XOCL_LOG(...)
#endif

namespace xocl { 

class event;

namespace debug {

void 
time_log(event* ev, cl_int status, cl_ulong ns);

void 
time_log(event* ev, cl_int status);

void
add_dependencies(event* event, cl_uint num_deps, const cl_event* deps);

void
add_command_type(event* event, cl_uint ct);
}

} // xocl

#endif


