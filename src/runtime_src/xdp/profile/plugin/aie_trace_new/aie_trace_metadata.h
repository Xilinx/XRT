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
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "core/common/device.h"

namespace xdp {

typedef std::vector<uint32_t>  ValueVector;

class AieTraceMetadata{
  private:
    
  //using module_type = xrt_core::edge::aie::module_type;

    bool useDelay = false;
    bool useUserControl = false;
    bool useGraphIterator = false;
    bool useOneDelayCtr = true;
    bool isValidMetrics = true;   
    bool runtimeMetrics;
    bool continuousTrace;

    uint32_t iterationCount = 0;
    uint64_t delayCycles = 0;
    uint64_t deviceID;
    uint64_t numAIETraceOutput;
    uint64_t offloadIntervalUs;
    unsigned int aie_trace_file_dump_int_s;

    std::string counterScheme;
    std::string metricSet;
    std::vector<std::string> metricSets;
    std::vector<std::string> memTileMetricSets;
    std::map<tile_type, std::string> configMetrics;
    std::map<tile_type, uint8_t> configChannel0;
    std::map<tile_type, uint8_t> configChannel1;

    void* handle;

  public:
    
    AieTraceMetadata(uint64_t deviceID, void* handle);

    std::string getMetricSet(const std::string& metricsStr);

    void checkSettings();
    int getHardwareGen();
    uint16_t getAIETileRowOffset();
    std::vector<std::string> getSettingsVector(std::string settingsString); 
    std::vector<tile_type> getMemTilesForTracing();

    static void read_aie_metadata(const char* data, size_t size, 
                                  boost::property_tree::ptree& aie_project);

    std::vector<tile_type> get_tiles(const xrt_core::device* device, 
                                     const std::string& graph_name,
                                     module_type type, 
                                     const std::string& kernel_name = "all");
    std::vector<tile_type> get_aie_tiles(const xrt_core::device* device,
                                         const std::string& graph_name);
    std::vector<tile_type> get_mem_tiles(const xrt_core::device* device, 
                                         const std::string& graph_name,
                                         const std::string& kernel_name = "all");
    std::vector<tile_type> get_event_tiles(const xrt_core::device* device, 
                                           const std::string& graph_name,
                                           module_type type);

    std::vector<std::string> get_graphs(const xrt_core::device* device);
    std::vector<std::string> get_kernels(const xrt_core::device* device);
    double get_clock_freq_mhz(const xrt_core::device* device);
    std::vector<gmio_type> get_trace_gmios(const xrt_core::device* device);
    aiecompiler_options get_aiecompiler_options(const xrt_core::device* device);

    void getConfigMetricsForTiles(std::vector<std::string>& metricsSettings,
                                  std::vector<std::string>& graphMetricsSettings,
                                  module_type type);
    void setTraceStartControl();
    uint8_t getMetricSetIndex(std::string metricString);
   
    std::string getMetricString(uint8_t index) {
      if (index < metricSets.size())
        return metricSets[index];
      else
        return metricSets[0];
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
    unsigned int getFileDumpIntS() {return aie_trace_file_dump_int_s;}
    std::string getMetricStr() {return metricSet;}
    std::map<tile_type, std::string> getConfigMetrics() {return configMetrics;}
    std::map<tile_type, uint8_t> getConfigChannel0() {return configChannel0;}
    std::map<tile_type, uint8_t> getConfigChannel1() {return configChannel1;}

    void setNumStreams(uint64_t newNumTraceStreams) {numAIETraceOutput = newNumTraceStreams;}
    void setDelayCycles(uint64_t newDelayCycles) {delayCycles = newDelayCycles;}
    void setRuntimeMetrics(bool metrics) {runtimeMetrics = metrics;}

    uint64_t getDelay() {
      if (useDelay)
        return delayCycles;
      return 0;
    }
    
  };

}

#endif
