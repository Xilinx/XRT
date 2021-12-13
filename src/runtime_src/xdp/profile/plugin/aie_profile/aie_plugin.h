/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef XDP_AIE_PLUGIN_DOT_H
#define XDP_AIE_PLUGIN_DOT_H

#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <iostream>

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "xdp/config.h"

#include "core/common/device.h"
#include "core/edge/common/aie_parser.h"
#include "xaiefal/xaiefal.hpp"

extern "C" {
#include <xaiengine.h>
}

namespace xdp {

  using tile_type = xrt_core::edge::aie::tile_type;
  using module_type = xrt_core::edge::aie::module_type;

  class AIEProfilingPlugin : public XDPPlugin
  {
  public:
    AIEProfilingPlugin();
    ~AIEProfilingPlugin();

    XDP_EXPORT
    void updateAIEDevice(void* handle);
    XDP_EXPORT
    void endPollforDevice(void* handle);

  private:
    void getPollingInterval();
    bool setMetrics(uint64_t deviceId, void* handle);

    std::string getMetricSet(const XAie_ModuleType mod, 
                             const std::string& metricsStr);
    std::vector<tile_type> getTilesForProfiling(const XAie_ModuleType mod,
                                                const std::string& metricsStr,
                                                void* handle);
    // Find minimum number of counters that are available across all tiles
    uint32_t getNumFreeCtr(xaiefal::XAieDev* aieDevice,
                           const std::vector<tile_type>& tiles,
                           const XAie_ModuleType mod,
                           const std::string& metricSet);
    void printTileModStats(xaiefal::XAieDev* aieDevice, 
                           const tile_type& tile, 
                           const XAie_ModuleType mod);
    void configGroupEvents(XAie_DevInst* aieDevInst,
                           const XAie_LocType loc,
                           const XAie_ModuleType mod,
                           const XAie_Events event,
                           const std::string metricSet);
    void configStreamSwitchPorts(XAie_DevInst* aieDevInst,
                                 const tile_type& tile,
                                 xaiefal::XAieTile& xaieTile,
                                 const XAie_LocType loc,
                                 const XAie_Events event,
                                 const std::string metricSet);

    void pollAIECounters(uint32_t index, void* handle);
    void endPoll();

  private:
    uint32_t mIndex = 0;
    uint32_t mPollingInterval;
    std::string mCoreMetricSet;
    std::string mMemoryMetricSet;
    std::string mShimMetricSet;

    std::map<void*,std::atomic<bool>> mThreadCtrlMap;
    std::map<void*,std::thread> mThreadMap;

    std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> mPerfCounters;

    std::set<std::string> mCoreMetricSets;
    std::map<std::string, std::vector<XAie_Events>> mCoreStartEvents;
    std::map<std::string, std::vector<XAie_Events>> mCoreEndEvents;
    std::map<std::string, std::vector<int>> broadcastCoreConfig;

    std::set<std::string> mMemoryMetricSets;
    std::map<std::string, std::vector<XAie_Events>> mMemoryStartEvents;
    std::map<std::string, std::vector<XAie_Events>> mMemoryEndEvents;

    std::set<std::string> mShimMetricSets;
    std::map<std::string, std::vector<XAie_Events>> mShimStartEvents;
    std::map<std::string, std::vector<XAie_Events>> mShimEndEvents;

    std::map<std::string, std::vector<std::string>> mCoreEventStrings;
    std::map<std::string, std::vector<std::string>> mMemoryEventStrings;
    std::map<std::string, std::vector<std::string>> mShimEventStrings;
  };

} // end namespace xdp

#endif
