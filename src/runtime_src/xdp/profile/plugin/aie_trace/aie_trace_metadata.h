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

#ifndef AIE_TRACE_METADATA_H
#define AIE_TRACE_METADATA_H

#include <boost/property_tree/ptree.hpp>
#include <set>
#include <map>
#include <vector>

#include "xdp/config.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "core/common/device.h"
#include "core/common/system.h"

namespace xdp {

typedef std::vector<uint32_t>  ValueVector;

class AieTraceMetadata {
  public:
    AieTraceMetadata(uint64_t deviceID, void* handle);

    void checkSettings();
    void setTraceStartControl();
    std::vector<std::string> getSettingsVector(std::string settingsString);
    uint8_t getMetricSetIndex(std::string metricString);
    
    void getConfigMetricsForTiles(std::vector<std::string>& metricsSettings,
                                  std::vector<std::string>& graphMetricsSettings,
                                  module_type type);
    void getConfigMetricsForInterfaceTiles(const std::vector<std::string>& metricsSettings,
                                           const std::vector<std::string> graphMetricsSettings);
    
   public:
    int getHardwareGen() {
      return aie::getHardwareGeneration(aieMeta);
    }
    uint16_t getRowOffset() {
      return aie::getAIETileRowOffset(aieMeta);
    }
    std::unordered_map<std::string, io_config> 
    get_trace_gmios() {
      return aie::getTraceGMIOs(aieMeta);
    }
    std::string getMetricString(uint8_t index) {
      if (index < metricSets[module_type::core].size())
        return metricSets[module_type::core][index];
      else
        return metricSets[module_type::core][0];
    }

    bool getUseDelay(){return useDelay;}
    bool getUseUserControl(){return useUserControl;}
    bool getUseGraphIterator(){return useGraphIterator;}
    bool getUseOneDelayCounter(){return useOneDelayCtr;}
    bool getRuntimeMetrics() {return runtimeMetrics;}
    std::string getCounterScheme(){return counterScheme;}

    uint32_t getIterationCount(){return iterationCount;}
    uint64_t getNumStreams() {return numAIETraceOutput;}
    uint64_t getContinuousTrace() {return continuousTrace;}
    uint64_t getOffloadIntervalUs() {return offloadIntervalUs;}
    uint64_t getDeviceID() {return deviceID;}
    bool getIsValidMetrics() {return isValidMetrics;}

    void* getHandle() {return handle;}
    uint32_t getPollingIntervalVal(){return pollingInterval;}
    unsigned int getFileDumpIntS() {return aie_trace_file_dump_int_s;}
    std::string getMetricStr() {return metricSet;}
    std::map<tile_type, std::string> getConfigMetrics() {return configMetrics;}
    std::map<tile_type, uint8_t> getConfigChannel0() {return configChannel0;}
    std::map<tile_type, uint8_t> getConfigChannel1() {return configChannel1;}

    void setNumStreams(uint64_t newNumTraceStreams) {numAIETraceOutput = newNumTraceStreams;}
    void setDelayCycles(uint64_t newDelayCycles) {delayCycles = newDelayCycles;}
    void setRuntimeMetrics(bool metrics) {runtimeMetrics = metrics;}
    uint64_t getDelay() {return ((useDelay) ? delayCycles : 0);}

  private:
    bool useDelay = false;
    bool useUserControl = false;
    bool useGraphIterator = false;
    bool useOneDelayCtr = true;
    bool isValidMetrics = true;   
    bool runtimeMetrics;
    bool continuousTrace;
    bool invalidXclbinMetadata;

    uint32_t pollingInterval;
    uint32_t iterationCount = 0;
    uint64_t delayCycles = 0;
    uint64_t deviceID;
    uint64_t numAIETraceOutput = 0;
    uint64_t offloadIntervalUs = 0;
    unsigned int aie_trace_file_dump_int_s;
    
    std::string counterScheme;
    std::string metricSet;
    boost::property_tree::ptree aieMeta;
    std::map<tile_type, std::string> configMetrics;
    std::map<tile_type, uint8_t> configChannel0;
    std::map<tile_type, uint8_t> configChannel1;

    std::map<module_type, std::string> defaultSets {
      { module_type::core,     "functions"},
      { module_type::mem_tile, "input_channels"},
      { module_type::shim,     "ports"}
    };

    std::map <module_type, std::vector<std::string>> metricSets {
      { module_type::core,     {"functions", "functions_partial_stalls", 
                                "functions_all_stalls", "all"} },
      { module_type::mem_tile, {"input_channels", "input_channels_stalls", 
                                "output_channels", "output_channels_stalls"} },
      { module_type::shim,     {"ports", "input_ports", "output_ports",
                                "input_ports_stalls", "output_ports_stalls", 
                                "input_ports_details", "output_ports_details"} }
    };

    void* handle;
    
  };

}

#endif
