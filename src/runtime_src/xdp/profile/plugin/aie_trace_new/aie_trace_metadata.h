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

// #include <memory>
#include <boost/property_tree/ptree.hpp>
#include <set>
#include <map>
#include <vector>

#include "xdp/config.h"

namespace xdp {

using tile_type = xrt_core::edge::aie::tile_type;
using gmio_type = xrt_core::edge::aie::gmio_type;

typedef std::vector<uint32_t>  ValueVector;

class AieTraceMetadata{
  private:
    bool useDelay = false;
    bool useUserControl = false;
    bool useGraphIterator = false;
    bool useOneDelayCtr = true;
    bool runtimeMetrics;
    bool continuousTrace;

    uint32_t iterationCount = 0;
    uint32_t delayCycles = 0;
    uint64_t deviceID;
    uint64_t numAIETraceOutput;
    uint64_t offloadIntervalUs;
    unsigned int aie_trace_file_dump_int_s;

    std::string metricSet;
    std::set<std::string> metricSets;
    std::map<tile_type, std::string> configMetrics;

    void* handle;
  public:
    XDP_EXPORT
    AieTraceMetadata(uint64_t deviceID, void* handle);

    XDP_EXPORT    
    std::string getMetricSet(const std::string& metricsStr, bool ignoreOldConfig = false);

    XDP_EXPORT
    std::vector<tile_type> getTilesForTracing();

    XDP_EXPORT
    adf::aiecompiler_options get_aiecompiler_options(const xrt_core::device* device);

    XDP_EXPORT
    static void read_aie_metadata(const char* data, size_t size, boost::property_tree::ptree& aie_project);

    XDP_EXPORT
    std::vector<tile_type> get_tiles(const xrt_core::device* device, const std::string& graph_name);

    XDP_EXPORT
    std::vector<std::string> get_graphs(const xrt_core::device* device);

    XDP_EXPORT
    double get_clock_freq_mhz(const xrt_core::device* device);

    XDP_EXPORT
    std::vector<gmio_type> get_trace_gmios(const xrt_core::device* device);

    XDP_EXPORT
    void getConfigMetricsForTiles(std::vector<std::string>& metricsSettings,
                                           std::vector<std::string>& graphmetricsSettings);
    XDP_EXPORT  
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

    void setNumStreams(uint64_t value) {numAIETraceOutput = value;}
    void setDelayCycles(uint32_t newDelayCycles) {delayCycles = newDelayCycles;}

    uint32_t getDelay() {
      if (useDelay)
        return delayCycles;
      return 0;
    }
    
  };

}

#endif
