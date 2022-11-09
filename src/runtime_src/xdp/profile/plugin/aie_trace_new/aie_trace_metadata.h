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

#ifndef AIE_TRACE_METADATA_H
#define AIE_TRACE_METADATA_H

#include <boost/property_tree/ptree.hpp>
#include <set>
#include <map>
#include <vector>

#include "xdp/config.h"
#include "core/common/device.h"

namespace xdp {

typedef std::vector<uint32_t>  ValueVector;

  enum class module_type {
      core = 0,
      dma,
      shim
    };

    struct tile_type
    { 
      uint16_t row;
      uint16_t col;
      uint16_t itr_mem_row;
      uint16_t itr_mem_col;
      uint64_t itr_mem_addr;
      bool     is_trigger;
      
      bool operator==(const tile_type &tile) const {
        return (col == tile.col) && (row == tile.row);
      }
      bool operator<(const tile_type &tile) const {
        return (col < tile.col) || ((col == tile.col) && (row < tile.row));
      }
    };

    struct gmio_type
    {
      std::string     name;
      uint32_t        id;
      uint16_t        type;
      uint16_t        shimColumn;
      uint16_t        channelNum;
      uint16_t        streamId;
      uint16_t        burstLength;
    };

class AieTraceMetadata{
  private:
    
  //using module_type = xrt_core::edge::aie::module_type;

    bool useDelay = false;
    bool useUserControl = false;
    bool useGraphIterator = false;
    bool useOneDelayCtr = true;
    bool runtimeMetrics;
    bool continuousTrace;

    uint32_t iterationCount = 0;
    uint64_t delayCycles = 0;
    uint64_t deviceID;
    uint64_t numAIETraceOutput;
    uint64_t offloadIntervalUs;
    unsigned int aie_trace_file_dump_int_s;

    std::string metricSet;
    std::set<std::string> metricSets;
    std::map<tile_type, std::string> configMetrics;

    void* handle;

  public:
    
    AieTraceMetadata(uint64_t deviceID, void* handle);

    std::string getMetricSet(const std::string& metricsStr, bool ignoreOldConfig = false);

    std::vector<tile_type> getTilesForTracing();

    static void read_aie_metadata(const char* data, size_t size, boost::property_tree::ptree& aie_project);

    std::vector<tile_type> get_tiles(const xrt_core::device* device, const std::string& graph_name);

    std::vector<tile_type> get_event_tiles(const xrt_core::device* device, 
                                           const std::string& graph_name,
                                           module_type type);

    std::vector<std::string> get_graphs(const xrt_core::device* device);

    double get_clock_freq_mhz(const xrt_core::device* device);

    std::vector<gmio_type> get_trace_gmios(const xrt_core::device* device);

    void getConfigMetricsForTiles(std::vector<std::string>& metricsSettings,
                                  std::vector<std::string>& graphmetricsSettings);
    void setTraceStartControl();

    bool getUseDelay(){return useDelay;}
    bool getUseUserControl(){return useUserControl;}
    bool getUseGraphIterator(){return useGraphIterator;}
    bool getUseOneDelayCounter(){return useOneDelayCtr;}
    bool getRuntimeMetrics() {return runtimeMetrics;}

    uint32_t getIterationCount(){return iterationCount;}
    uint64_t getNumStreams() {return numAIETraceOutput;}
    uint64_t getContinuousTrace() {return continuousTrace;}
    uint64_t getOffloadIntervalUs() {return offloadIntervalUs;}
    uint64_t getDeviceID() {return deviceID;}

    void* getHandle() {return handle;}
    unsigned int getFileDumpIntS() {return aie_trace_file_dump_int_s;}
    std::map<tile_type, std::string> getConfigMetrics(){return configMetrics;}
    std::string getMetricStr(){return metricSet;}

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
