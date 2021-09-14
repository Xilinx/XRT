/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef AIE_TRACE_PLUGIN_H
#define AIE_TRACE_PLUGIN_H

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "core/edge/common/aie_parser.h"
#include "xaiefal/xaiefal.hpp"

extern "C" {
#include <xaiengine.h>
}

namespace xdp {

  class DeviceIntf;
  class AIETraceOffload;
  class AIETraceLogger;

  using tile_type = xrt_core::edge::aie::tile_type;

  class AieTracePlugin : public XDPPlugin
  {
    public:
      XDP_EXPORT
      AieTracePlugin();

      XDP_EXPORT
      ~AieTracePlugin();

      XDP_EXPORT
      void updateAIEDevice(void* handle);

      XDP_EXPORT
      void flushAIEDevice(void* handle);

      XDP_EXPORT
      void finishFlushAIEDevice(void* handle);

      XDP_EXPORT
      virtual void writeAll(bool openNewFiles);

    private:
      inline uint32_t bcIdToEvent(int bcId);
      void releaseCurrentTileCounters(int numCoreCounters, int numMemoryCounters);
      bool setMetrics(uint64_t deviceId, void* handle);
      void setFlushMetrics(uint64_t deviceId, void* handle);

      // Aie resource manager utility functions
      bool tileHasFreeRsc(xaiefal::XAieDev* aieDevice, XAie_LocType& loc, const std::string& metricSet);
      void printTileStats(xaiefal::XAieDev* aieDevice, const tile_type& tile);

      // Utility functions
      std::string getMetricSet(void* handle);
      std::vector<tile_type> getTilesForTracing(void* handle);

    private:
      // Runtime or compile-time specified trace metrics?
      bool runtimeMetrics = true;

      // Trace Runtime Status
      AieRC mConfigStatus = XAIE_OK;

      std::vector<void*> deviceHandles;
      std::map<uint64_t, void*> deviceIdToHandle;

      typedef std::tuple<AIETraceOffload*, 
                         AIETraceLogger*,
                         DeviceIntf*> AIEData;

      std::map<uint32_t, AIEData>  aieOffloaders;

      // Types
      typedef XAie_Events            EventType;
      typedef std::vector<EventType> EventVector;
      typedef std::vector<uint32_t>  ValueVector;

      // Trace metrics
      std::string metricSet;    
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
      EventVector coreCounterResetEvents;
      ValueVector coreCounterEventValues;

      EventVector memoryCounterStartEvents;
      EventVector memoryCounterEndEvents;
      EventVector memoryCounterResetEvents;
      ValueVector memoryCounterEventValues;
  };
    
}   
    
#endif
