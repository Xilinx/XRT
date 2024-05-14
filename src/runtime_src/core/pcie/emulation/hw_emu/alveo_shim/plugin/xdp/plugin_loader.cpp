/**
 * Copyright (C) 2022 Xilinx, Inc
 * Copyright (C) 2023-2024 Advanced Micro Devices, Inc. - All rights reserved
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
#include "core/common/utils.h"

#include "device_offload.h"
#include "hal_trace.h"
#include "pl_deadlock.h"
#include "sc_profile.h"

#include "plugin_loader.h"

namespace xdp::hw_emu {

  // This function is responsible for checking all of the relevant xrt.ini file
  //  options and loading the appropriate debug/profile plugins.
  void load()
  {
    try {
      if (xrt_core::config::get_xrt_trace() ||
          xrt_core::utils::load_host_trace())
        xdp::hw_emu::trace::load() ;
      if (xrt_core::config::get_device_trace() != "off" ||
          xrt_core::config::get_device_counters())
        xdp::hw_emu::device_offload::load() ;
      if (xrt_core::config::get_sc_profile())
        xdp::hw_emu::sc::load();
      if (xrt_core::config::get_pl_deadlock_detection())
        xdp::hw_emu::pl_deadlock::load();
    }
    catch (...) {
      // Boost property tree might throw an error.  If that happens
      // just keep going without loading additional plugins.
    }
  }

} // end namespace xdp::hw_emu
