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

#ifndef AIE_PROFILE_H
#define AIE_PROFILE_H

#include <cstdint>

#include "core/edge/common/aie_parser.h"
#include "xdp/profile/plugin/aie_profile_new/aie_profile_impl.h"
#include "xaiefal/xaiefal.hpp"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

namespace xdp {
  using tile_type = xrt_core::edge::aie::tile_type;
  
  class AieProfile_EdgeImpl : public AieProfileImpl{
    public:
      // AieProfile_EdgeImpl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata)
      //   : AieProfileImpl(database, metadata){}
      AieProfile_EdgeImpl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata);

      ~AieProfile_EdgeImpl() = default;

      void updateDevice();
      void poll(uint32_t index, void* handle);
      bool checkAieDevice(uint64_t deviceId, void* handle);

      bool setMetricsSettings(uint64_t deviceId, void* handle);
      std::vector<tile_type> getAllTilesForCoreMemoryProfiling(const XAie_ModuleType mod,
                                                        const std::string &graph,
                                                        void* handle);
                                                        
      std::vector<tile_type> getAllTilesForInterfaceProfiling(void* handle,
                              const std::string &metricStr,
                              int16_t channelId = -1,
                              bool useColumn = false, uint32_t minCol = 0, uint32_t maxCol = 0);

     void getConfigMetricsForTiles(int moduleIdx,
                                    const std::vector<std::string>& metricsSettings,
                                    const std::vector<std::string>& graphmetricsSettings,
                                    const XAie_ModuleType mod,
                                    void* handle);

      void getInterfaceConfigMetricsForTiles(int moduleIdx,
                                              const std::vector<std::string>& metricsSettings,
                                              /* std::vector<std::string> graphmetricsSettings, */
                                              void* handle);
    
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
      uint32_t getCounterPayload(XAie_DevInst* aieDevInst,
                                const tile_type& tile,
                                uint16_t column, 
                                uint16_t row, 
                                uint16_t startEvent);
    private:
      XAie_DevInst*     aieDevInst = nullptr;
      xaiefal::XAieDev* aieDevice  = nullptr;    

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
      std::vector<std::map<tile_type, std::string>> mConfigMetrics;                  
    
  };

}   

#endif
