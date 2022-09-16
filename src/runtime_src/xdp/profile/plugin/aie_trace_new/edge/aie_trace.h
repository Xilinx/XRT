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

#ifndef AIE_TRACE_H
#define AIE_TRACE_H

#include <cstdint>

#include "xdp/profile/plugin/aie_trace_new/aie_trace_impl.h"

namespace xdp {

  class AieTraceEdgeImpl : public AieTraceImpl{
    public:
      AieTraceEdgeImpl(VPDatabase* database, std::shared_ptr<AieTraceMetadata> metadata);
        // : AieTraceImpl(database, metadata);
      ~AieTraceEdgeImpl();

      void updateDevice();
      void flushDevice();
      void finishFlushDevice();

      bool setMetrics(uint64_t deviceId, void* handle);
      bool setMetricsSettings(uint64_t deviceId, void* handle);
      void releaseCurrentTileCounters(int numCoreCounters, int numMemoryCounters);
      bool tileHasFreeRsc(xaiefal::XAieDev* aieDevice, XAie_LocType& loc, const std::string& metricSet);
      void printTileStats(xaiefal::XAieDev* aieDevice, const tile_type& tile);
      inline uint32_t bcIdToEvent(int bcId);

    private:
      std::set<std::string> metricSets;
      std::map<std::string, EventVector> coreEventSets;
      std::map<std::string, EventVector> memoryEventSets;

      // AIE profile counters
      std::vector<xrt_core::edge::aie::tile_type> mCoreCounterTiles;
      std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> mCoreCounters;
      std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> mMemoryCounters;

      // Counter metrics (same for all sets)
      EventType   coreTraceStartEvent;
      EventType   coreTraceEndEvent;
      EventVector coreCounterStartEvents;
      EventVector coreCounterEndEvents;
      ValueVector coreCounterEventValues;

      EventVector memoryCounterStartEvents;
      EventVector memoryCounterEndEvents;
      EventVector memoryCounterResetEvents;
      ValueVector memoryCounterEventValues;

      /* Currently only "aie" tile metrics is supported for graph/tile based trace.
      * So, a single map for tile and resolved metric is sufficient.
      * In future, when mem_tile and interface_tile metrics will be supported, we will
      * need separate map for each type or a vector of maps for all types together.
      */
      std::map<tile_type, std::string> mConfigMetrics;
      // std::vector<std::map<tile_type, std::string>> mConfigMetrics;
  };

}   

#endif
