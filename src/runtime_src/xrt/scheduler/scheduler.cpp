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

#include "scheduler.h"
#include "xrt/config.h"
#include "xrt/device/device.h"
#include <cstdlib>

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
// TODO check for only sw_emu. Some github examples are using "true", Remove this check once all github examples are updated
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool swem = xem ? (std::strcmp(xem,"sw_emu")==0) : false;
  return swem;
}

inline bool
kds_enabled(bool forceoff=false)
{  
  bool iskdsemu = is_sw_emulation() ? (xrt_core::config::get_flag_kds_sw_emu() ? false : true) : false;
  static bool enabled = !iskdsemu && xrt_xocl::config::get_kds() && !xrt_xocl::config::get_feature_toggle("Runtime.sws") && !is_windows() ;
  if (forceoff)
    enabled = false;
  return enabled;
}

// Force disabling of kds if emulation and 5.0 DSA
static void
emu_50_disable_kds(const xrt_xocl::device* device)
{
  static bool done = false;
  if (!done) {
    done = true;

    if (!kds_enabled())
      return;

    if (!emulation_mode())
      return;

    if (device->getName().find("_5_0")==std::string::npos)
      return;

    // stop kds thread
    xrt_xocl::scheduler::stop();

    // force kds off
    kds_enabled(true/*forceoff*/);

    // restart scheduler thread
    xrt_xocl::scheduler::start();
  }
}

}

namespace xrt_xocl {  namespace scheduler {

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

  purge_command_freelist();
}

/**
 * Schedule a command for execution on either sws or kds
 */
void
schedule(const command_type& cmd)
{
  if (kds_enabled())
    kds::schedule(cmd);
  else
    sws::schedule(cmd);
}

void
init(xrt_xocl::device* device)
{
  emu_50_disable_kds(device);

  if (kds_enabled())
    kds::init(device);
  else
    sws::init(device);
}

}} // scheduler,xrt
