/**
 * Copyright (C) 2021 Xilinx, Inc
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

#ifndef XDP_AIE_DEBUG_PLUGIN_DOT_H
#define XDP_AIE_DEBUG_PLUGIN_DOT_H

#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "xdp/config.h"

#include "core/common/device.h"
#include "core/edge/common/aie_parser.h"
#include "xaiefal/xaiefal.hpp"

extern "C" {
#include <xaiengine.h>
#include "xaiengine/xaie_helper.h"
}

namespace xdp {

  using tile_type = xrt_core::edge::aie::tile_type;

  class AIEDebugPlugin : public XDPPlugin
  {
  public:
    AIEDebugPlugin();
    ~AIEDebugPlugin();

    XDP_EXPORT
    void updateAIEDevice(void* handle);
    XDP_EXPORT
    void endPollforDevice(void* handle);

  private:
    void getTilesForDebug(void* handle);
    void pollAIERegisters(uint32_t index, void* handle);
    void endPoll();
    std::string getCoreStatusString(uint32_t status);

  private:
    uint32_t mPollingInterval;

    // Thread control flags for each device handle
    std::map<void*,std::atomic<bool>> mThreadCtrlMap;
    // Threads mapped to device handles
    std::map<void*,std::thread> mThreadMap;
    // Graphname -> coretiles
    std::map<std::string,std::vector<tile_type>> mGraphCoreTilesMap;
  };

} // end namespace xdp

#endif
