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

#include "scheduler.h"
#include "xrt/config.h"
#include "xrt/device/device.h"
#include <cstdlib>

namespace {

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
  static bool enabled = !is_sw_emulation() && xrt::config::get_kds() && !xrt::config::get_feature_toggle("Runtime.sws");
  if (forceoff)
    enabled = false;
  return enabled;
}

// Force disabling of kds if emulation and 5.0 DSA
static void
emu_50_disable_kds(const xrt::device* device)
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
    xrt::scheduler::stop();

    // force kds off
    kds_enabled(true/*forceoff*/);

    // restart scheduler thread
    xrt::scheduler::start();
  }
}

  // Force disabling of kds if aws 5.0 device
XRT_UNUSED static void
aws_50_disable_kds(const xrt::device* device)
{
  static bool done = false;
  if (!done) {
    done = true;

    if (!kds_enabled())
      return;

    if (device->getName().find("xilinx_aws")==std::string::npos)
      return;

    if (device->getName().find("_5_0")==std::string::npos)
      return;

    // stop kds thread
    xrt::scheduler::stop();

    // force kds off
    kds_enabled(true/*forceoff*/);

    // restart scheduler thread
    xrt::scheduler::start();
  }
}

}

namespace xrt {  namespace scheduler {

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
init(xrt::device* device, size_t regmap_size, bool cu_isr, size_t num_cus, size_t cu_offset, size_t cu_base_addr, const std::vector<uint32_t>& cu_addr_map)
{
  emu_50_disable_kds(device);
  //  aws_50_disable_kds(device);

  if (kds_enabled())
    kds::init(device,regmap_size,cu_isr,num_cus,cu_offset,cu_base_addr,cu_addr_map);
  else
    sws::init(device,cu_addr_map);
}

}} // scheduler,xrt
