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

// Debug of xocl 
// 
// Enabled via sdaccel.ini
//
// [Debug] xocl_debug       --- enable debugging  (false)
// [Debug] xocl_log         --- log file for debugging (xocl.log)
// [Debug] xocl_event_begin --- first event to log (0)
// [Debug] xocl_event_end   --- last event to log (999999)

#include "debug.h"
#include "time.h"
#include "event.h"

#include "xrt/config.h"

#include <mutex>
#include <vector>
#include <fstream>
#include <memory>

#include <cstdarg>
#include <cstdio>

namespace {

static bool s_debug_on = false;
static std::string s_debug_log;
static unsigned long s_zero = xocl::time_ns();

// Event information 
namespace event {

static unsigned int s_start_id = 0;
static unsigned int s_end_id = 999999;

struct info
{
  cl_command_type m_command_type = 0;

  // queue,submit,start,complete times are collected when
  // the event changes status. very tight coupling to cl.h
  unsigned long m_times[CL_QUEUED+1] = {0};

  // event dependencies are collected if any
  std::vector<unsigned long> m_dependencies;

  // print this record
  // event# commandtype queued submitted running complete [dependencies]*
  std::ostream& print(std::ostream& ostr, unsigned int id)
  {
    if (!m_times[CL_RUNNING])
        m_times[CL_RUNNING] = m_times[CL_COMPLETE];
    ostr << id << " " << m_command_type << " "
         << (m_times[CL_QUEUED]-s_zero) << " "
         << (m_times[CL_SUBMITTED]-s_zero) << " "
         << (m_times[CL_RUNNING]-s_zero) << " "
         << (m_times[CL_COMPLETE]-s_zero);
    for (auto d : m_dependencies)
      ostr << " " << d;
    return ostr;
  }
};

std::mutex s_mutex;
static std::vector<info> s_info;

static void
init()
{
  s_start_id = xrt::config::detail::get_uint_value("Debug.xocl_event_begin",0);
  s_end_id = xrt::config::detail::get_uint_value("Debug.xocl_event_end",1000);

  s_info.reserve(s_end_id-s_start_id+1);
}

inline unsigned int
id2idx(unsigned int id)
{
  auto idx = id - s_start_id;
  if (s_info.size() >= idx+1)
    return idx;

  if (s_info.capacity() < idx+1) {
    assert(0); // no reachable if inrange is used properly
    // this won't work unless all s_info access is proctected
    // but I don't want any overhead for logging
    std::lock_guard<std::mutex> lk(s_mutex);
    s_info.reserve(idx + 100);
  }

  s_info.resize(idx+1);
  return idx;
}

inline unsigned int
idx2id(unsigned int idx)
{
  return idx + s_start_id;
}

inline bool
inrange(unsigned int id)
{
  return (id>=s_start_id && id<=s_end_id);
}

inline void
log(unsigned int id, cl_int status, cl_ulong ns)
{
  if (!inrange(id))
    return;

  auto idx = id2idx(id);
  s_info[idx].m_times[status] = ns;
}

inline void
dependencies(unsigned int id, cl_uint num_deps, const cl_event* deps)
{
  if (!inrange(id))
    return;

  auto idx = id2idx(id);
  for (auto e : xocl::get_range(deps,deps+num_deps))
    s_info[idx].m_dependencies.push_back(xocl::xocl(e)->get_uid());
}

inline void
command(unsigned int id, cl_command_type cmd)
{
  if (!inrange(id))
    return;

  auto idx = id2idx(id);
  s_info[idx].m_command_type = cmd;
}

inline void
print()
{
  std::ofstream ostr(s_debug_log,std::ios::out);
  size_t count = 0;
  for (auto& ei : s_info) {
    ei.print(ostr,idx2id(count++)) << "\n";
  }
}

} // event

// Return true of debugging is enabled, per sdaccel.ini
//
// [Debug] xocl_debug --- enable debugging  (false)
// [Debug] xocl_log --- log file for debugging (xocl.log)
// 
// This function must be after main(), since it uses xrt::config
// which relies on static global initialization
static bool
init()
{
  static bool called = false;
  if (called)
    return s_debug_on;

  called = true;

  if (!(s_debug_on = xrt::config::get_xocl_debug()))
    return false;

  s_debug_log = xrt::config::detail::get_string_value("Debug.xocl_log","xocl.log");
  
  ::event::init();

  // reset time zero
  s_zero = xocl::time_ns();

  return s_debug_on;
}

struct shutdown
{
  ~shutdown()
  {
    if (!s_debug_on)
      return;

    s_debug_on = false;

    ::event::print();
  }
};

// global static destruction object
static shutdown sd;

}

// External API
namespace xocl { 

namespace debug {

void 
time_log(xocl::event* event, cl_int status, cl_ulong ns)
{
  static bool debug = init();
  if (!debug)
    return;

  ::event::log(event->get_uid(),status,ns);
}

void
time_log(xocl::event* event, cl_int status)
{
  static bool debug = init();
  if (!debug)
    return;

  ::event::log(event->get_uid(),status,xocl::time_ns());
}

void
add_dependencies(xocl::event* event, cl_uint num_deps, const cl_event* deps)
{
  static bool debug = init();
  if (!debug)
    return;

  ::event::dependencies(event->get_uid(),num_deps,deps);
}

void
add_command_type(xocl::event* event, cl_uint ct)
{
  static bool debug = init();
  if (!debug)
    return;

  ::event::command(event->get_uid(),ct);
}


} // debug

#ifdef VERBOSE
auto file_deleter = [](FILE* f) { fclose(f); };
static std::unique_ptr<FILE,decltype(file_deleter)> logfile(fopen("debug.log","w"),file_deleter);
void
logf(const char* format, ...)
{
  va_list args;
  va_start(args,format);
  vfprintf(logfile.get(),format,args);
  va_end(args);
}
#endif

} // xocl


