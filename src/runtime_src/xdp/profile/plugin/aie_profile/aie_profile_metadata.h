/**
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

#ifndef AIE_PROFILE_METADATA_H
#define AIE_PROFILE_METADATA_H

#include <boost/property_tree/ptree.hpp>
#include <memory>
#include <vector>
#include <set>

#include "core/common/device.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "xdp/config.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/static_info/filetypes/base_filetype_impl.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"

namespace xdp {

// Forwadr declarations of XDP constructs
struct LatencyConfig;

constexpr unsigned int NUM_CORE_COUNTERS = 4;
constexpr unsigned int NUM_MEMORY_COUNTERS = 2;
constexpr unsigned int NUM_SHIM_COUNTERS = 2;
constexpr unsigned int NUM_MEM_TILE_COUNTERS = 4;
constexpr unsigned int NUM_UC_EVENT_COUNTERS = 5;
constexpr unsigned int NUM_UC_LATENCY_COUNTERS = 1;

class AieProfileMetadata {
  private:
    // Currently supporting core modules, memory modules, interface tiles, 
    // memory tiles, and microcontrollers
    static constexpr int NUM_MODULES = static_cast<int>(module_type::num_types);

    const std::map <module_type, std::vector<std::string>> metricStrings {
      {
        module_type::core, {
          "heat_map", "stalls", "execution", "floating_point", 
          "stream_put_get", "aie_trace", "events",
          "write_throughputs", "read_throughputs", 
          "s2mm_throughputs", "mm2s_throughputs"}
      },
      {
        module_type::dma, {
          "conflicts", "dma_locks", "dma_stalls_s2mm",
          "dma_stalls_mm2s", "s2mm_throughputs", "mm2s_throughputs"}
      },
      { 
        module_type::shim, {
          "input_throughputs", "output_throughputs", 
          "s2mm_throughputs", "mm2s_throughputs",
          "input_stalls", "output_stalls", "s2mm_stalls", 
          "mm2s_stalls", "packets", METRIC_BYTE_COUNT,
          "uc_dma_activity", "uc_axis_throughputs", "uc_core"}
      },
      {
        module_type::mem_tile, {
          "input_channels", "input_channels_details", "input_throughputs",
          "s2mm_channels", "s2mm_channels_details", "s2mm_throughputs", 
          "output_channels", "output_channels_details", "output_throughputs",
          "mm2s_channels", "mm2s_channels_details", "mm2s_throughputs",
          "memory_stats", "mem_trace", "conflict_stats1", "conflict_stats2", 
          "conflict_stats3", "conflict_stats4"}
      },
      {
        module_type::uc, {
          "execution", "interrupt_stalls", "mmu_activity"}
      }
    };

    const std::vector<std::string> moduleNames =
      {"aie", "aie_memory", "interface_tile", "memory_tile", "microcontroller"};
    const std::string defaultSets[NUM_MODULES] =
      {"s2mm_throughputs", "s2mm_throughputs", "s2mm_throughputs", 
       "s2mm_throughputs", "execution"};
    const int numCountersMod[NUM_MODULES] =
      {NUM_CORE_COUNTERS, NUM_MEMORY_COUNTERS, NUM_SHIM_COUNTERS, 
       NUM_MEM_TILE_COUNTERS, NUM_UC_EVENT_COUNTERS+NUM_UC_LATENCY_COUNTERS};
    const module_type moduleTypes[NUM_MODULES] =
      {module_type::core, module_type::dma, module_type::shim, 
       module_type::mem_tile, module_type::uc};

    uint32_t pollingInterval;
    uint64_t deviceID;
    double clockFreqMhz;
    void* handle;
    xrt::hw_context hwContext;
    bool useGraphIterator = false;
    uint32_t iterationCount = 0;
    
    std::vector<std::map<tile_type, std::string>> configMetrics;
    std::map<tile_type, std::string> pairConfigMetrics;
    std::map<tile_type, uint8_t> configChannel0;
    std::map<tile_type, uint8_t> configChannel1;
    std::map<tile_type, LatencyConfig> latencyConfigMap;
    std::vector<std::pair<tile_type, std::string>> configMetricLatencyVec; // configuration order vector
    std::map<tile_type, uint32_t> bytesTransferConfigMap;
    std::map<std::string, LatencyCache> keysCache;
    uint32_t defaultTransferBytes = 1;

    const aie::BaseFiletypeImpl* metadataReader = nullptr;

  public:
    AieProfileMetadata(uint64_t deviceID, void* handle);

    uint64_t getDeviceID() {return deviceID;}
    void* getHandle() {return handle;}
    uint32_t getPollingIntervalVal() {return pollingInterval;}
    void checkSettings();

    std::vector<std::string> getSettingsVector(std::string settingsString);

    void getConfigMetricsForTiles(const int moduleIdx,
                                  const std::vector<std::string>& metricsSettings,
                                  const std::vector<std::string>& graphMetricsSettings,
                                  const module_type mod);
    void getConfigMetricsForInterfaceTiles(const int moduleIdx,
                                           const std::vector<std::string>& metricsSettings,
                                           const std::vector<std::string> graphMetricsSettings);
    void getConfigMetricsForMicrocontrollers(const int moduleIdx,
                                             const std::vector<std::string>& metricsSettings,
                                             const std::vector<std::string> graphMetricsSettings);
    int getPairModuleIndex(const std::string& metricSet, module_type mod);
    uint8_t getMetricSetIndex(const std::string& metricSet, module_type mod);
    bool isSupported(const std::string metricSet, bool isTileBased);
    bool profileAPIMetricSet(const std::string metricSet);

    std::map<tile_type, std::string> getConfigMetrics(const int module){ return configMetrics[module];}
     std::vector<std::pair<tile_type, std::string>> getConfigMetricsVec(const int module);

    std::map<tile_type, uint8_t> getConfigChannel0() {return configChannel0;}
    std::map<tile_type, uint8_t> getConfigChannel1() {return configChannel1;}
    xdp::aie::driver_config getAIEConfigMetadata();

    bool checkModule(const int module) { return (module >= 0 && module < NUM_MODULES);}
    std::string getModuleName(const int module) { return moduleNames[module]; }
    int getNumCountersMod(const int module){ return numCountersMod[module]; }
    module_type getModuleType(const int module) { return moduleTypes[module]; }

    uint8_t getAIETileRowOffset() const { return metadataReader == nullptr ? 0 : metadataReader->getAIETileRowOffset(); }
    int getHardwareGen() const { return  metadataReader == nullptr ? 0 : metadataReader->getHardwareGeneration(); }

    double getClockFreqMhz() {return clockFreqMhz;}
    int getNumModules() {return NUM_MODULES;}
    xrt::hw_context getHwContext(){return hwContext;}
    void setHwContext(xrt::hw_context c) {
      hwContext = std::move(c);
    }

    inline std::vector<uint8_t> getPartitionOverlayStartCols() const {
      return metadataReader->getPartitionOverlayStartCols();
    }
    bool aieMetadataEmpty() { return metadataReader==nullptr; }
    std::vector<tile_type> getInterfaceTiles(const std::string graph, const std::string port, const std::string metric) {
      if (aieMetadataEmpty())
        return {};
      return metadataReader->getInterfaceTiles(graph, port, metric);
    }

    void getConfigMetricsForintfTilesLatencyConfig(xdp::module_type module,
                       const std::vector<std::string>& intfTilesLatencyConfigs);
    void setProfileStartControl(bool graphIteratorEvent);
    uint32_t processUserSpecifiedBytes(const std::string& strTotalBytes);
    uint32_t getUserSpecifiedThreshold(const tile_type& tile, const std::string& metricSet);
    void setUserSpecifiedBytes(const tile_type& tile, const uint32_t& threshold);
    bool getUseGraphIterator(){return useGraphIterator;}
    uint32_t getIterationCount(){return iterationCount;}

    bool isSourceTile(const tile_type& tile);
    bool isValidLatencyTile(const tile_type& tile) const;
    uint64_t getIntfLatencyPayload(const tile_type& tile);
    uint64_t createPayload(uint8_t col1, uint8_t row1, uint8_t portID1,
                         uint8_t col2, uint8_t row2, uint8_t portID2);
    bool getSourceTile(const tile_type& pairTyle, tile_type& sourceTile) const;
    bool getDestTile(const tile_type& pairTyle, tile_type& destTile) const;
    std::string getSrcDestPairKey(uint8_t col, uint8_t row);
    GraphPortPair getSrcDestGraphPair(const std::string& srcDestKey) const;

    std::vector<tile_type>
    getTiles(const std::string& graph_name, module_type type, const std::string& kernel_name) const;

    const AIEProfileFinalConfig& getAIEProfileConfig() const ;
};

} // end XDP namespace

#endif
