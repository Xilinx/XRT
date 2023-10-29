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

#include "core/common/device.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "xdp/config.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/aie_util.h"

namespace xdp {

constexpr unsigned int NUM_CORE_COUNTERS = 4;
constexpr unsigned int NUM_MEMORY_COUNTERS = 2;
constexpr unsigned int NUM_SHIM_COUNTERS = 2;
constexpr unsigned int NUM_MEM_TILE_COUNTERS = 4;

class AieProfileMetadata {
  private:
    // Currently supporting Core, Memory, Interface Tiles, and MEM Tiles
    static constexpr int NUM_MODULES = 4;

    std::map <module_type, std::vector<std::string>> metricStrings {
      {
        module_type::core, {
          "heat_map", "stalls", "execution",
          "floating_point", "stream_put_get", "write_throughputs",
          "read_throughputs", "aie_trace", "events"}
      },
      {
        module_type::dma, {
          "conflicts", "dma_locks", "dma_stalls_s2mm",
          "dma_stalls_mm2s", "write_throughputs", "read_throughputs"}
      },
      { 
        module_type::shim, {"input_throughputs", "output_throughputs", "packets"}

      },
      {
        module_type::mem_tile, {
          "input_channels", "input_channels_details",
          "output_channels", "output_channels_details",
          "memory_stats", "mem_trace"
        }
      }
    };

    const std::vector<std::string> moduleNames =
    {"aie", "aie_memory", "interface_tile", "memory_tile"};
    const std::string defaultSets[NUM_MODULES] =
    {"write_throughputs", "write_throughputs", "input_throughputs", "input_channels"};
    const int numCountersMod[NUM_MODULES] =
    {NUM_CORE_COUNTERS, NUM_MEMORY_COUNTERS, NUM_SHIM_COUNTERS, NUM_MEM_TILE_COUNTERS};
    const module_type moduleTypes[NUM_MODULES] =
    {module_type::core, module_type::dma, module_type::shim, module_type::mem_tile};

    uint32_t pollingInterval;
    uint64_t deviceID;
    double clockFreqMhz;
    void* handle;
    xrt::hw_context hwContext;

    std::vector<std::map<tile_type, std::string>> configMetrics;
    std::map<tile_type, uint8_t> configChannel0;
    std::map<tile_type, uint8_t> configChannel1;
    boost::property_tree::ptree aie_meta; 

  public:
    AieProfileMetadata(uint64_t deviceID, void* handle);

    uint64_t getDeviceID() {return deviceID;}
    void* getHandle() {return handle;}
    uint32_t getPollingIntervalVal() {return pollingInterval;}
    void checkSettings();

    std::vector<std::string> getSettingsVector(std::string settingsString);

    void getConfigMetricsForTiles(int moduleIdx,
                                  const std::vector<std::string>& metricsSettings,
                                  const std::vector<std::string>& graphMetricsSettings,
                                  const module_type mod);
    void getConfigMetricsForInterfaceTiles(int moduleIdx,
                                            const std::vector<std::string>& metricsSettings,
                                            const std::vector<std::string> graphMetricsSettings);
    uint8_t getMetricSetIndex(std::string metricSet, module_type mod);
    
    std::map<tile_type, std::string> getConfigMetrics(int module){ return configMetrics[module];}
    std::map<tile_type, uint8_t> getConfigChannel0() {return configChannel0;}
    std::map<tile_type, uint8_t> getConfigChannel1() {return configChannel1;}
    boost::property_tree::ptree getAIEConfigMetadata(std::string config_name);

    bool checkModule(int module) { return (module >= 0 && module < NUM_MODULES);}
    std::string getModuleName(int module) { return moduleNames[module]; }
    int getNumCountersMod(int module){ return numCountersMod[module]; }
    module_type getModuleType(int module) { return moduleTypes[module]; }

    uint16_t getAIETileRowOffset() { return aie::getAIETileRowOffset(aie_meta);}
    int getHardwareGen() { return aie::getHardwareGeneration(aie_meta);}

    double getClockFreqMhz() {return clockFreqMhz;}
    int getNumModules() {return NUM_MODULES;}
    xrt::hw_context getHwContext(){return hwContext;}
    void setHwContext(xrt::hw_context c) {
      hwContext = std::move(c);
    }
};

} // end XDP namespace

#endif
