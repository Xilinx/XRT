/**
 * Copyright (C) 2022 Xilinx, Inc
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

#include "device_offload.h"
#include "hal_trace.h"
#include "plugin_loader.h"

namespace xdp {
namespace hw_emu {

  // This function is responsible for checking all of the relevant xrt.ini file
  //  options and loading the appropriate debug/profile plugins.
  void load()
  {
    if (xrt_core::config::get_xrt_trace())
      xdp::hw_emu::trace::load() ;
    if (xrt_core::config::get_data_transfer_trace() != "off" ||
        xrt_core::config::get_device_trace() != "off" ||
        xrt_core::config::get_device_counters())
      xdp::hw_emu::device_offload::load() ;
  }

} // end namespace hw_emu
} // end namespace xdp
