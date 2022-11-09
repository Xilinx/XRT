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

#ifndef AIE_PROFILE_METADATA_H
#define AIE_PROFILE_METADATA_H

#include "core/edge/common/aie_parser.h"

// #include "xaiefal/xaiefal.hpp"

// extern "C" {
// #include <xaiengine.h>
// #include <xaiengine/xaiegbl_params.h>
// }
namespace xdp {
  using tile_type = xrt_core::edge::aie::tile_type;
  using module_type = xrt_core::edge::aie::module_type;

class AieProfileMetadata{

  public:
    AieProfileMetadata(uint64_t deviceID, void* handle);
    void getPollingInterval();
    // std::vector<tile_type> getAllTilesForCoreMemoryProfiling(const XAie_ModuleType mod,
    //                                                     const std::string &graph,
    //                                                     void* handle);
    // std::vector<tile_type> getAllTilesForShimProfiling(void* handle,
    //                           const std::string &metricStr,
    //                           int16_t channelId = -1,
    //                           bool useColumn = false, uint32_t minCol = 0, uint32_t maxCol = 0);
    // void getConfigMetricsForTiles(int moduleIdx, std::vector<std::string> metricsSettings,
    //                                            std::vector<std::string> graphmetricsSettings,
    //                                            const XAie_ModuleType mod,
    //                                            void* handle);
    void getInterfaceConfigMetricsForTiles(int moduleIdx,
                                           std::vector<std::string> metricsSettings,
                                           /* std::vector<std::string> graphmetricsSettings, */
                                           void* handle);                
    std::string getMetricSet(const int mod, 
                              const std::string& metricsStr, 
                              bool ignoreOldConfig = false);
    std::vector<tile_type> getTilesForProfiling(const int mod, 
                                                const std::string& metricsStr,
                                                void* handle);
    uint64_t getDeviceID() {return deviceID;}
    void* getHandle() {return handle;}
    std::string getCoreMetricSet(){return mCoreMetricSet;}            
    std::string getMemoryMetricSet(){return mMemoryMetricSet;}
    std::string getShimMetricSet(){return mShimMetricSet;}
    int16_t getChannelId(){return mChannelId;}
  private:
    int16_t mChannelId = -1;
    uint32_t mIndex = 0;
    uint32_t mPollingInterval;
    uint64_t deviceID;
    void* handle;
    std::string mCoreMetricSet;
    std::string mMemoryMetricSet;
    std::string mShimMetricSet;

  };
}

#endif