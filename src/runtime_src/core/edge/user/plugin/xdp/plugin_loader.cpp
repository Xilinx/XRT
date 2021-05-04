/**
 * Copyright (C) 2020-2021 Xilinx, Inc
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

#include "plugin_loader.h"
#include "hal_profile.h"
#include "hal_device_offload.h"
#include "aie_profile.h"
#include "noc_profile.h"
#include "aie_trace.h"
#include "vart_profile.h"
#include "sc_profile.h"

#include "core/common/config_reader.h"

namespace xdp {
namespace hal_hw_plugins {

// This function is responsible for loading all of the HAL level HW XDP plugins
bool load()
{
  if (xrt_core::config::get_xrt_trace() || xrt_core::config::get_xrt_profile()) {
    xdp::hal::load() ;
  }

  if (xrt_core::config::get_data_transfer_trace() != "off") {
    xdp::hal::device_offload::load() ;
  }

  if (xrt_core::config::get_aie_profile()) {
    xdp::aie::profile::load() ;
  }

  if (xrt_core::config::get_noc_profile()) {
    xdp::noc::profile::load() ;
  }

#if 0 
  // Not currently supported on edge
  if (xrt_core::config::get_power_profile()) {
    xdp::power::profile::load() ;
  }
#endif 

  if (xrt_core::config::get_sc_profile()) {
    xdp::sc::profile::load();
  }

  if (xrt_core::config::get_aie_trace()) {
    xdp::aie::trace::load() ;
  }

  if (xrt_core::config::get_vitis_ai_profile()) {
    xdp::vart::profile::load() ;
  }

  return true ;
}

} // end namespace hal_hw_plugins
} // end namespace xdp
