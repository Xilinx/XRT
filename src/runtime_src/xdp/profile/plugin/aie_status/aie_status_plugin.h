/**
 * Copyright (C) 2021 Xilinx, Inc
 * Copyright (C) 2022-2025 Advanced Micro Devices, Inc. - All rights reserved
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
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/common/device.h"
#include "xaiefal/xaiefal.hpp"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/static_info/filetypes/base_filetype_impl.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

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

    void updateAIEDevice(void* handle, bool hw_context_flow);
    void endPollforDevice(void* handle);

    static bool alive();

  private:
    void getTilesForStatus(void* handle);
    void endPoll();
    std::string getCoreStatusString(uint32_t status);
    uint64_t getDeviceIDFromHandle(void* handle, bool hw_context_flow);
    
    // Threads used by this plugin
    void pollDeadlock(uint64_t index, void* handle);
    void writeStatus(uint64_t index, void* handle, VPWriter* aieWriter);

  private:
    static bool live;
    uint32_t mPollingInterval;
    const aie::BaseFiletypeImpl* metadataReader = nullptr;
    std::shared_ptr<xrt_core::device> mXrtCoreDevice;
    std::mutex mtxWriterThread;

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
