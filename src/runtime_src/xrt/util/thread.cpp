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

#include "thread.h"
#include "debug.h"
#include "message.h"
#include "config_reader.h"

#include <thread>
#include <iostream>

#include <boost/algorithm/string/trim.hpp>
#include <boost/tokenizer.hpp>

#ifdef __GNUC__
# include <pthread.h>
# include <sched.h>
#endif

namespace {

namespace platform_specific {

#ifdef __GNUC__

static void
debug_thread_policy(const std::string& str, int policy, int priority)
{
  switch (policy) {
  case SCHED_FIFO:
    XRT_DEBUG(std::cout,str," thread policy=",policy," (fifo), priority=",priority,"\n");
    break;
  case SCHED_RR:
    XRT_DEBUG(std::cout,str," thread policy=",policy," (rr), priority=",priority,"\n");
    break;
  case SCHED_OTHER:
    XRT_DEBUG(std::cout,str," thread policy=",policy," (other), priority=",priority,"\n");
    break;
  default:
    XRT_DEBUG(std::cout,str," thread policy=",policy," (), priority=",priority,"\n");
    break;
  }
}

static void
set_thread_policy(std::thread& thread)
{
  static int policy = 0;
  static int priority = 0;
  static bool initialized = false;
  if (!initialized) {
    initialized=true;

    sched_param sch;
    pthread_getschedparam(pthread_self(),&policy,&sch);
    priority = sch.sched_priority;

    debug_thread_policy("default",policy,priority);

    static std::string config_policy = xrt::config::detail::get_string_value("Runtime.thread_policy","default");
    if (config_policy=="rr") {
      policy = SCHED_RR;
      priority = 1;
    }
    else if (config_policy=="fifo") {
      policy = SCHED_FIFO;
      priority = 1;
    }
    else if (config_policy=="other") {
      policy = SCHED_OTHER;
      priority = 0;
    }

    debug_thread_policy("config",policy,priority);
  }

  struct sched_param sch;
  sch.sched_priority = priority;
  pthread_setschedparam(thread.native_handle(), policy, &sch);
}

static void
set_cpu_affinity(std::thread& thread)
{
  static bool initialized = false;
  static cpu_set_t cpuset;
  static bool all=false;
  if (!initialized) {
    initialized = true;

    std::string cpus = xrt::config::detail::get_string_value("Runtime.cpu_affinity","default");
    if (cpus=="default")
      all=true;
    else {
      boost::trim_if(cpus,boost::is_any_of("{}"));
      using tokenizer=boost::tokenizer<boost::char_separator<char> >;
      boost::char_separator<char> sep(", ");
      auto max_cpus = std::thread::hardware_concurrency();
      CPU_ZERO(&cpuset);
      for (auto& tok : tokenizer(cpus,sep)) {
        auto cpu = std::stoul(tok);
        if (cpu < max_cpus) {
          XRT_DEBUG(std::cout,"adding cpu #",cpu," to affinity mask\n");
          CPU_SET(cpu,&cpuset);
        }
        else {
          xrt::message::send(xrt::message::severity_level::XRT_WARNING,"Ignoring cpu affinity since cpu #" + tok + " is out of range\n");
          all=true;
        }
      }
    }
  }

  if (all)
    return;

  if (pthread_setaffinity_np(thread.native_handle(),sizeof(cpu_set_t),&cpuset)) {
    throw std::runtime_error("error calling pthread_setaffinity_np");
  }
}

#else

static void
set_thread_policy(std::thread&)
{
}

static void
set_cpu_affinity(std::thread&)
{
}

#endif

} // platform_specific

}

namespace xrt {

namespace detail {

void set_thread_policy(std::thread& thread)
{
  ::platform_specific::set_thread_policy(thread);
}

void set_cpu_affinity(std::thread& thread)
{
  ::platform_specific::set_cpu_affinity(thread);
}

} // detail

} // xrt
