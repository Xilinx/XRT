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
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common

#include "exec.h"
#include "core/common/config_reader.h"
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
# pragma warning( disable : 4996 )
#endif

namespace {

static bool
is_windows()
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

static bool
is_sw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool swem = xem ? (std::strcmp(xem,"sw_emu")==0) : false;
  return swem;
}

inline bool
kds_enabled(bool forceoff=false)
{
  static bool iskdsemu = is_sw_emulation() ? (xrt_core::config::get_flag_kds_sw_emu() ? true : false) : true;
  static bool enabled = iskdsemu
    &&  xrt_core::config::get_kds()
    && !xrt_core::config::get_feature_toggle("Runtime.sws")
    && !is_windows();

  if (forceoff)
    enabled = false;

  return enabled;
}

}

namespace xrt_core {  namespace exec {

void
start()
{
  if (kds_enabled())
    kds::start();
  else
    sws::start();
}

void
stop()
{
  if (kds_enabled())
    kds::stop();
  else
    sws::stop();
}

/**
 * Schedule a command for execution on either sws or kds
 */
void
schedule(command* cmd)
{
  if (kds_enabled())
    kds::schedule(cmd);
  else
    sws::schedule(cmd);
}

void
init(xrt_core::device* device)
{
  struct X {
    ~X() { try { stop(); } catch (...) { } } // coverity
  };
  static X x;

  static bool started = false;
  if (!started) {
    start();
    started = true;
  }

  if (kds_enabled())
    kds::init(device);
  else
    sws::init(device);
}

}} // exec, xrt_core
