/*
 * Copyright (C) 2020-2022, Xilinx Inc - All rights reserved
 * Copyright (C) 2022-2025 Advanced Micro Devices, Inc. - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

// This file implements XRT kernel APIs as declared in 
// core/include/experimental/xrt_profile.h

#define XCL_DRIVER_DLL_EXPORT 
#define XRT_CORE_COMMON_SOURCE
#include "core/include/xrt/experimental/xrt_profile.h"

#include "core/common/dlfcn.h"
#include "core/common/error.h"
#include "core/common/module_loader.h"
#include "core/common/utils.h"

#include <functional>
#include <mutex>

namespace xrt::profile {

user_range::
user_range(const char* label, const char* tooltip)
: id(static_cast<uint32_t>(xrt_core::utils::issue_id()))
, active(true)
{
  xrtURStart(id, label, tooltip);
}

user_range::
user_range()
: id(0)
, active(false)
{}

user_range::
~user_range()
{
  if (active)
    xrtUREnd(id);
}

void user_range::
start(const char* label, const char* tooltip)
{
  // Handle case where start is called while started
  if (active)
    xrtUREnd(id);

  id = static_cast<uint32_t>(xrt_core::utils::issue_id());
  xrtURStart(id, label, tooltip);
  active = true;
}

void user_range::
end()
{
  // Handle case when end when not tracking time
  if (!active)
    return;

  xrtUREnd(id);
  active = false;
}

user_event::
user_event() = default;

user_event::
~user_event() = default;

void user_event::
mark(const char* label)
{
  xrtUEMark(label);
}

void user_event::
mark_time_ns(const std::chrono::nanoseconds& time_ns, const char* label)
{
  xrtUEMarkTimeNs(static_cast<unsigned long long int>(time_ns.count()), label);
}

} // xrt::profile


// Anonymous namespace for dynamic loading and connection
namespace {
  
std::function<void (unsigned int, const char*, const char*)> user_range_start_cb;
std::function<void (unsigned int)> user_range_end_cb;
std::function<void (const char*)> user_event_cb;
std::function<void (unsigned long long int, const char*)> user_event_time_ns_cb;

static void
register_user_functions(void* handle)
{
  using startType = void(*)(unsigned int, const char*, const char*);
  using endType   = void(*)(unsigned int);
  using pipeType  = void(*)(const char*);
  using nsType    = void(*)(unsigned long long int, const char*);

  user_range_start_cb =
    reinterpret_cast<startType>(xrt_core::dlsym(handle, "user_event_start_cb"));
  user_range_end_cb =
    reinterpret_cast<endType>(xrt_core::dlsym(handle, "user_event_end_cb"));
  user_event_cb =
    reinterpret_cast<pipeType>(xrt_core::dlsym(handle, "user_event_happened_cb"));
  user_event_time_ns_cb =
    reinterpret_cast<nsType>(xrt_core::dlsym(handle, "user_event_time_ns_cb"));
}

#ifdef __linux__
__attribute__((unused))
#endif
static void
register_callbacks_empty(void*)
{
}

static void
warning_callbacks_empty()
{
}

static void
load_user_profiling_plugin()
{

#ifdef _WIN32
  static xrt_core::module_loader xdp_core_loader("xdp_core",
                                                register_callbacks_empty,
                                                warning_callbacks_empty);
#endif

  static xrt_core::module_loader user_event_loader("xdp_user_plugin",
                                                   register_user_functions,
                                                   warning_callbacks_empty);
}

} // end anonymous

extern "C" {

void
xrtURStart(unsigned int id, const char* label, const char* tooltip)
{
  try {
    load_user_profiling_plugin();
    if (user_range_start_cb != nullptr)
      user_range_start_cb(id, label, tooltip);
  }
  catch (const std::exception& ex)
  {
    xrt_core::send_exception_message(ex.what());
  }
}

void
xrtUREnd(unsigned int id)
{
  try {
    load_user_profiling_plugin();
    if (user_range_end_cb != nullptr)
      user_range_end_cb(id);
  }
  catch (const std::exception& ex)
  {
    xrt_core::send_exception_message(ex.what());
  }
}

void
xrtUEMark(const char* label)
{
  try {
    load_user_profiling_plugin();
    if (user_event_cb != nullptr)
      user_event_cb(label);
  }
  catch (const std::exception& ex)
  {
    xrt_core::send_exception_message(ex.what());
  }
}

void
xrtUEMarkTimeNs(unsigned long long int time_ns, const char* label)
{
  try {
    load_user_profiling_plugin();
    if (user_event_time_ns_cb != nullptr)
      user_event_time_ns_cb(time_ns, label);
  }
  catch (const std::exception& ex)
  {
    xrt_core::send_exception_message(ex.what());
  }
}

} // extern C
