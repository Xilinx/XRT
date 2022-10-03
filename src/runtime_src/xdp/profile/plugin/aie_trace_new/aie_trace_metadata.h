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

#include <iostream>
#include <memory>
#include "core/edge/common/aie_parser.h"
//#include "xaiefal/xaiefal.hpp"
#include <boost/property_tree/ptree.hpp>

//extern "C" {
//#include <xaiengine.h>
//}

namespace xdp {

using tile_type = xrt_core::edge::aie::tile_type;
using gmio_type = xrt_core::edge::aie::gmio_type;

typedef std::vector<uint32_t>  ValueVector;

class AieTraceMetadata{
  private:
    uint64_t deviceID;
    void* handle;

    uint64_t numAIETraceOutput;
    // Runtime or compile-time specified trace metrics?
    bool runtimeMetrics;
     // Trace metrics
    std::string metricSet;
    std::set<std::string> metricSets;
    // These flags are used to decide configuration at various points
    bool mUseDelay = false;
    bool mUseUserControl = false;
    bool mUseGraphIterator = false;
    uint32_t mIterationCount = 0;
    uint32_t mDelayCycles = 0;

    bool continuousTrace;
    uint64_t offloadIntervalUs;
    unsigned int aie_trace_file_dump_int_s;

  public:
    AieTraceMetadata(uint64_t deviceID, void* handle);
    std::string getMetricSet();
    std::vector<tile_type> getTilesForTracing();
    uint64_t getTraceStartDelayCycles();
    adf::aiecompiler_options get_aiecompiler_options(const xrt_core::device* device);
    static void read_aie_metadata(const char* data, size_t size, boost::property_tree::ptree& aie_project);
    std::vector<tile_type> get_tiles(const xrt_core::device* device, const std::string& graph_name);
    std::vector<std::string> get_graphs(const xrt_core::device* device);
    double get_clock_freq_mhz(const xrt_core::device* device);
    std::vector<gmio_type> get_trace_gmios(const xrt_core::device* device);

    bool getRuntimeMetrics() {return runtimeMetrics;}
    uint64_t getDeviceID() {return deviceID;}
    void* getHandle() {return handle;}
    void setNumStreams(uint64_t value) {numAIETraceOutput = value;}
    uint64_t getNumStreams() {return numAIETraceOutput;}
    uint64_t getContinuousTrace() {return continuousTrace;}
    unsigned int getFileDumpIntS() {return aie_trace_file_dump_int_s;}
    uint64_t getOffloadIntervalUs() {return offloadIntervalUs;}
    bool getUseDelay(){return mUseDelay;}
    bool getUseUserControl(){return mUseUserControl;}
    bool getUseGraphIterator(){return mUseGraphIterator;}
    
    void setUseDelay(bool delay){mUseDelay = delay;}
    void setUseUserControl(bool control){mUseUserControl = control;}
    void setUseGraphIterator(bool graphiterator){mUseGraphIterator = graphiterator;}

    uint32_t getIterationCount(){return mIterationCount;}
    void setIterationCount(uint32_t iterationCount){mIterationCount = iterationCount;}

    uint32_t getDelay() {
      if (mUseDelay)
        return mDelayCycles;
      return 0;
    }
    
    void setDelayCycles(uint32_t delayCycles) {
      mDelayCycles = delayCycles;
    }
  };

}

#endif
