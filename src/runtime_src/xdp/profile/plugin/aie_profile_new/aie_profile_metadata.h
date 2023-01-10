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
#include <vector>

#include "core/common/device.h"
#include "xdp/config.h"
#include "xdp/profile/database/static_info/aie_constructs.h"

namespace xdp {

constexpr unsigned int NUM_CORE_COUNTERS   = 4;
constexpr unsigned int NUM_MEMORY_COUNTERS = 2;
constexpr unsigned int NUM_SHIM_COUNTERS  =  2;

class AieProfileMetadata{
  private:
    // Currently supporting Core, Memory, Interface Tile metrics only. Need to add Memory Tile metrics
    static constexpr int NUM_MODULES = 3;

    const std::vector<std::string> moduleNames = {"aie", "aie_memory", "interface_tile"};
    const std::string defaultSets[NUM_MODULES] = {"all:heat_map", "all:conflicts", "all:input_bandwidths"};
    const int numCountersMod[NUM_MODULES] =
        {NUM_CORE_COUNTERS, NUM_MEMORY_COUNTERS, NUM_SHIM_COUNTERS};
    const module_type moduleTypes[NUM_MODULES] = 
        {module_type::core, module_type::dma, module_type::shim};

    uint32_t pollingInterval;
    uint64_t deviceID;
    double clockFreqMhz;
    void* handle;

    std::map <module_type, std::vector<std::string>> metricStrings {
      { module_type::core, {"heat_map", "stalls", "execution",           
                            "floating_point", "stream_put_get", "write_bandwidths",     
                            "read_bandwidths", "aie_trace", "events"} },
      { module_type::dma, {"conflicts", "dma_locks", "dma_stalls_s2mm",
                           "dma_stalls_mm2s", "write_bandwidths", "read_bandwidths"} },
      { module_type::shim, {"input_bandwidths", "output_bandwidths", "packets"} },
    };

    std::vector<std::map<tile_type, std::string>> configMetrics;

  public:
    AieProfileMetadata(uint64_t deviceID, void* handle);
    
    uint64_t getDeviceID() {return deviceID;}
    void* getHandle() {return handle;}
    uint32_t getPollingIntervalVal(){return pollingInterval;}

    std::vector<tile_type> getAllTilesForCoreMemoryProfiling(const module_type mod,
                                                      const std::string &graph,
                                                      void* handle);                                       
    std::vector<tile_type> getAllTilesForInterfaceProfiling(void* handle,
                            const std::string &metricStr,
                            int16_t channelId = -1,
                            bool useColumn = false, uint32_t minCol = 0, uint32_t maxCol = 0);
    void getConfigMetricsForTiles(int moduleIdx,
                                    const std::vector<std::string>& metricsSettings,
                                    const std::vector<std::string>& graphmetricsSettings,
                                    const module_type mod,
                                    void* handle);

    void getInterfaceConfigMetricsForTiles(int moduleIdx,
                                              const std::vector<std::string>& metricsSettings,
                                              /* std::vector<std::string> graphmetricsSettings, */
                                              void* handle);
    uint8_t getMetricSetIndex(std::string metricSet, module_type mod);
    static void read_aie_metadata(const char* data, size_t size, boost::property_tree::ptree& aie_project);

    std::vector<std::string> get_graphs(const xrt_core::device* device);
    std::unordered_map<std::string, plio_config> get_plios(const xrt_core::device* device);
    std::vector<tile_type> get_event_tiles(const xrt_core::device* device, const std::string& graph_name, module_type type);
    
    std::map<tile_type, std::string> getConfigMetrics(int module){ return configMetrics[module];}
    double getClockFreqMhz(){return clockFreqMhz;}
    std::string getModuleName(int module){return moduleNames[module];}
    int getNumCountersMod(int module){return numCountersMod[module];}
    module_type getModuleType(int module){return moduleTypes[module];}
    int getNumModules(){return NUM_MODULES;}
  };
}

#endif
 
