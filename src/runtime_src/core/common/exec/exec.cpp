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
emulation_mode()
{
  static bool val = (std::getenv("XCL_EMULATION_MODE") != nullptr);
  return val;
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
  static bool enabled = !is_sw_emulation()
    &&  xrt_core::config::get_kds()
    && !xrt_core::config::get_feature_toggle("Runtime.sws")
    && !is_windows();

  if (forceoff)
    enabled = false;

  return enabled;
}

inline bool
pts_enabled()
{
  static bool enabled = false; // xrt_core::config::get_feature_toggle("Runtime.pts");
  return enabled;
}

// Force disabling of kds if emulation and 5.0 DSA
static void
emu_50_disable_kds(const xrt_core::device*)
{
  static bool done = false;
  if (!done) {
    done = true;

    if (!kds_enabled())
      return;

    if (!emulation_mode())
      return;

    // stop kds thread
    xrt_core::exec::stop();

    // force kds off
    kds_enabled(true/*forceoff*/);

    // restart scheduler thread
    xrt_core::exec::start();
  }
}

}

namespace xrt_core {  namespace exec {

void
start()
{
  if (pts_enabled())
    pts::start();
  else if (kds_enabled())
    kds::start();
  else
    sws::start();
}

void
stop()
{
  if (pts_enabled())
    pts::stop();
  else if (kds_enabled())
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
  if (pts_enabled())
    pts::schedule(cmd);
  else if (kds_enabled())
    kds::schedule(cmd);
  else
    sws::schedule(cmd);
}

void
init(xrt_core::device* device, const axlf* top)
{
  struct X {
    ~X() { stop(); }
  };
  static X x;

  emu_50_disable_kds(device);

  static bool started = false;
  if (!started) {
    start();
    started = true;
  }

  if (pts_enabled())
    pts::init(device,top);
  if (kds_enabled())
    kds::init(device,top);
  else
    sws::init(device,top);
}

}} // exec, xrt_core
