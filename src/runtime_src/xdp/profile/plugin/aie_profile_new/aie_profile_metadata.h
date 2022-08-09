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


namespace xdp {
  using tile_type = xrt_core::edge::aie::tile_type;
  using module_type = xrt_core::edge::aie::module_type;

  public:
      AieProfileMetadata(uint64_t deviceID, void* handle);
      void getPollingInterval();
      std::string getMetricSet(const XAie_ModuleType mod, 
                                const std::string& metricsStr, 
                                bool ignoreOldConfig);
      std::vector<tile_type> getTilesForProfiling(const XAie_ModuleType mod, 
                                                  const std::string& metricsStr,
                                                  void* handle);
      std::vector<tile_type> getAllTilesForShimProfiling(void* handle, 
                                                         const std::string &metricsStr);

  private:
    int16_t mChannelId = -1;
    uint32_t mIndex = 0;
    uint32_t mPollingInterval;
    std::string mCoreMetricSet;
    std::string mMemoryMetricSet;
    std::string mShimMetricSet;
        
    std::map<void*,std::atomic<bool>> mThreadCtrlMap;
    std::map<void*,std::thread> mThreadMap;

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
}

