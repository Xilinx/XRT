/**
 * Copyright (C) 2021 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef XDP_AIE_STATUS_PLUGIN_DOT_H
#define XDP_AIE_STATUS_PLUGIN_DOT_H

#include <atomic>
#include <boost/property_tree/ptree.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/config.h"

#include "core/common/device.h"
#include "xaiefal/xaiefal.hpp"

extern "C" {
#include <xaiengine.h>
#include "xaiengine/xaie_helper.h"
}

namespace xdp {

  class AIEStatusPlugin : public XDPPlugin
  {
  public:
    AIEStatusPlugin();
    ~AIEStatusPlugin();

    XDP_EXPORT
    void updateAIEDevice(void* handle);
    XDP_EXPORT
    void endPollforDevice(void* handle);

    XDP_EXPORT
    static bool alive();

  private:
    void getTilesForStatus();
    void endPoll();
    std::string getCoreStatusString(uint32_t status);
    
    // Threads used by this plugin
    void pollDeadlock(uint64_t index, void* handle);
    void writeStatus(uint64_t index, void* handle, VPWriter* aieWriter);

  private:
    static bool live;
    uint32_t mPollingInterval;
    boost::property_tree::ptree mAieMeta;

    // Thread control flags for each device handle
    std::map<void*,std::atomic<bool>> mThreadCtrlMap;
    // Threads mapped to device handles
    std::map<void*,std::thread> mDeadlockThreadMap;
    std::map<void*,std::thread> mStatusThreadMap;
    // Graphname -> coretiles
    std::map<std::string,std::vector<tile_type>> mGraphCoreTilesMap;
  };

} // end namespace xdp

#endif
