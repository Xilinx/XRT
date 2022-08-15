/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_SOURCE

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <iostream>
#include <memory>
#include <cstring>

#include "core/common/message.h"
#include "core/common/xrt_profiling.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"

#include "xdp/profile/plugin/aie_profile_new/aie_profile_metadata.h"

#include "xrt/xrt_kernel.h"

#include "aie_prfile_kernel_config.h"
#include "aie_profile.h"

constexpr uint32_t MAX_TILES = 400;
constexpr uint32_t MAX_LENGTH = 4096;

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  using module_type = xrt_core::edge::aie::module_type;

  void AieProfile_x86Impl::updateDevice() {
    // Set metrics for counters and trace events 
    if (!setMetrics(metadata->getDeviceID(), metadata->getHandle())) {
      std::string msg("Unable to configure AIE trace control and events. No trace will be generated.");
      xrt_core::message::send(severity_level::warning, "XRT", msg);
      return;
    }
  }


  bool AieProfile_x86Impl::setMetrics(uint64_t deviceId, void* handle) {
    int counterId = 0;
    bool runtimeCounters = false;
    constexpr int NUM_MODULES = 3;

    // Get AIE clock frequency
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);
    auto clockFreqMhz = xrt_core::edge::aie::get_clock_freq_mhz(device.get());

    auto interfaceMetricStr = xrt_core::config::get_aie_profile_interface_metrics();
    std::vector<std::string> interfaceVec;
    boost::split(interfaceVec, interfaceMetricStr, boost::is_any_of(":"));
    auto interfaceMetric = interfaceVec.at(0);
    try {mChannelId = std::stoi(interfaceVec.at(1));} catch (...) {mChannelId = -1;}

    int numCounters[NUM_MODULES] =
        {NUM_CORE_COUNTERS, NUM_MEMORY_COUNTERS, NUM_SHIM_COUNTERS};
    XAie_ModuleType falModuleTypes[NUM_MODULES] = 
        {XAIE_CORE_MOD, XAIE_MEM_MOD, XAIE_PL_MOD};
    std::string moduleNames[NUM_MODULES] = {"aie", "aie_memory", "interface_tile"};
    std::string metricSettings[NUM_MODULES] = 
        {xrt_core::config::get_aie_profile_core_metrics(),
         xrt_core::config::get_aie_profile_memory_metrics(),
         interfaceMetric};

    return true; //placeholder
  }

  
}