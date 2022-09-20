/**
 * Copyright (C) 2020-2022 Xilinx, Inc
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

#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/utils.h"

#include "hal_profile.h"
#include "plugin_loader.h"

#include "aie_debug.h"
#include "aie_profile.h"

#ifndef __HWEM__
#include "aie_trace.h"
#include "hal_device_offload.h"
#include "noc_profile.h"
#include "pl_deadlock.h"
#include "sc_profile.h"
#include "vart_profile.h"
#else
// Only device offload supported for hardware emulation on edge
#include "hw_emu_device_offload.h"
#endif

namespace xdp {
namespace hal_hw_plugins {

// This function is responsible for loading all of the HAL level HW XDP plugins
bool load()
{
#ifndef __HWEM__
  if (xrt_core::config::get_xrt_trace() ||
      xrt_core::utils::load_host_trace())
    xdp::hal::load();

  if (xrt_core::config::get_device_trace() != "off" ||
      xrt_core::config::get_device_counters())
    xdp::hal::device_offload::load() ;

  if (xrt_core::config::get_aie_status())
    xdp::aie::debug::load();

  if (xrt_core::config::get_aie_profile())
    xdp::aie::profile::load();

  if (xrt_core::config::get_noc_profile())
    xdp::noc::profile::load();

#if 0 
  // Not currently supported on edge
  if (xrt_core::config::get_power_profile()) {
    xdp::power::profile::load() ;
  }
#endif 

  if (xrt_core::config::get_sc_profile())
    xdp::sc::profile::load();

  if (xrt_core::config::get_aie_trace())
    xdp::aie::trace::load();

  if (xrt_core::config::get_vitis_ai_profile())
    xdp::vart::profile::load();

  if (xrt_core::config::get_pl_deadlock_detection())
    xdp::pl_deadlock::load();

#endif
  return true ;
}

} // end namespace hal_hw_plugins

namespace hal_hw_emu_plugins {

// This function is responsible for loading all of the Hardware Emulation
//  profiling plugins based on the xrt.ini flags.
bool load()
{
#ifdef __HWEM__
  // Hardware emulation uses the same plugin as hardware for API trace
  if (xrt_core::config::get_xrt_trace() ||
      xrt_core::utils::load_host_trace())
    xdp::hal::load();

  if (xrt_core::config::get_device_trace() != "off" ||
      xrt_core::config::get_device_counters())
    xdp::hal::hw_emu::device_offload::load() ;

  if (xrt_core::config::get_aie_status())
    xdp::aie::debug::load();

  if (xrt_core::config::get_aie_profile())
    xdp::aie::profile::load();
#endif
  return true ;
}

} // end namespace hal_hw_emu_plugins
} // end namespace xdp
