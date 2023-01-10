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


#ifndef AIE_TRACE_DOT_H
#define AIE_TRACE_DOT_H

#include <cstdint>

#include "xdp/profile/plugin/aie_trace_new/aie_trace_impl.h"
#include "xaiefal/xaiefal.hpp"

namespace xdp {

  class AieTrace_EdgeImpl : public AieTraceImpl{
    public:
      XDP_EXPORT
      AieTrace_EdgeImpl(VPDatabase* database, std::shared_ptr<AieTraceMetadata> metadata);
        // : AieTraceImpl(database, metadata);
      ~AieTrace_EdgeImpl() = default;
      
      XDP_EXPORT
      virtual void updateDevice();

    private:
 
      typedef XAie_Events            EventType;
      typedef std::vector<EventType> EventVector;
      typedef std::vector<uint32_t>  ValueVector;

      bool setMetrics(uint64_t deviceId, void* handle);
      bool setMetricsSettings(uint64_t deviceId, void* handle);
      void releaseCurrentTileCounters(int numCoreCounters, int numMemoryCounters);
      bool tileHasFreeRsc(xaiefal::XAieDev* aieDevice, XAie_LocType& loc, const std::string& metricSet);
      void printTileStats(xaiefal::XAieDev* aieDevice, const tile_type& tile);
      bool configureStartIteration(xaiefal::XAieMod& core);
      bool configureStartDelay(xaiefal::XAieMod& core);
     
      bool checkAieDeviceAndRuntimeMetrics(uint64_t deviceId, void* handle);
      void setTraceStartControl(void* handle);
      uint64_t checkTraceBufSize(uint64_t size);                                   
      std::string getMetricSet(void* handle,
                               const std::string& metricsStr,
                               bool ignoreOldConfig);
      inline uint32_t bcIdToEvent(int bcId);

    private:
      XAie_DevInst*     aieDevInst = nullptr;
      xaiefal::XAieDev* aieDevice  = nullptr;

      std::set<std::string> metricSets;
      std::map<std::string, EventVector> mCoreEventSets;
      std::map<std::string, EventVector> mMemoryEventSets;

      // AIE profile counters
      std::vector<tile_type> mCoreCounterTiles;
      std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> mCoreCounters;
      std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> mMemoryCounters;

      // Counter metrics (same for all sets)
      EventType   mCoreTraceStartEvent;
      EventType   mCoreTraceEndEvent;
      EventVector mCoreCounterStartEvents;
      EventVector mCoreCounterEndEvents;
      ValueVector mCoreCounterEventValues;

      EventVector mMemoryCounterStartEvents;
      EventVector mMemoryCounterEndEvents;
      ValueVector mMemoryCounterEventValues;

  };

}   

#endif
