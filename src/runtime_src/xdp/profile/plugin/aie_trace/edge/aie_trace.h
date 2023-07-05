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

#ifndef AIE_TRACE_DOT_H
#define AIE_TRACE_DOT_H

#include <cstdint>

#include "xaiefal/xaiefal.hpp"
#include "xdp/profile/plugin/aie_trace/aie_trace_impl.h"

namespace xdp {

  class AieTrace_EdgeImpl : public AieTraceImpl {
   public:
    XDP_EXPORT
    AieTrace_EdgeImpl(VPDatabase* database, std::shared_ptr<AieTraceMetadata> metadata);
    ~AieTrace_EdgeImpl() = default;

    XDP_EXPORT
    virtual void updateDevice();
    XDP_EXPORT
    virtual void flushAieTileTraceModule();
    void pollTimers(uint32_t index, void* handle);
    void freeResources();
    
   private:
    typedef XAie_Events EventType;
    typedef std::vector<EventType> EventVector;
    typedef std::vector<uint32_t> ValueVector;

    module_type getTileType(uint16_t row);
    uint16_t getRelativeRow(uint16_t absRow);
    module_type getModuleType(uint16_t absRow, XAie_ModuleType mod);
    bool isStreamSwitchPortEvent(const XAie_Events event);
    void configStreamSwitchPorts(XAie_DevInst* aieDevInst, const tile_type& tile,
                                 xaiefal::XAieTile& xaieTile, const XAie_LocType loc,
                                 const module_type type, const XAie_Events event,
                                 const int countnum, const std::string metricSet,
                                 const uint8_t channel);
    void configEventSelections(XAie_DevInst* aieDevInst, const XAie_LocType loc, 
                               const XAie_ModuleType mod, const module_type type, 
                               const std::string metricSet, const uint8_t channel0,
                               const uint8_t channel);
    void configEdgeEvents(XAie_DevInst* aieDevInst, const tile_type& tile,
                          const std::string metricSet, const XAie_Events event);
    bool setMetricsSettings(uint64_t deviceId, void* handle);
    void releaseCurrentTileCounters(int numCoreCounters, int numMemoryCounters);
    bool tileHasFreeRsc(xaiefal::XAieDev* aieDevice, XAie_LocType& loc, 
                        const module_type type, const std::string& metricSet);
    void printTileStats(xaiefal::XAieDev* aieDevice, const tile_type& tile);
    void printTraceEventStats(uint64_t deviceId);
    bool configureStartIteration(xaiefal::XAieMod& core);
    bool configureStartDelay(xaiefal::XAieMod& core);

    bool checkAieDeviceAndRuntimeMetrics(uint64_t deviceId, void* handle);
    void setTraceStartControl(void* handle);
    uint64_t checkTraceBufSize(uint64_t size);
    inline uint32_t bcIdToEvent(int bcId);

   private:
    XAie_DevInst* aieDevInst = nullptr;
    xaiefal::XAieDev* aieDevice = nullptr;

    std::map<std::string, EventVector> mCoreEventSets;
    std::map<std::string, EventVector> mMemoryEventSets;
    std::map<std::string, EventVector> mMemoryTileEventSets;
    std::map<std::string, EventVector> mInterfaceTileEventSets;

    // AIE profile counters
    std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> mPerfCounters;
    std::vector<std::shared_ptr<xaiefal::XAieStreamPortSelect>> mStreamPorts;

    // Counter metrics (same for all sets)
    EventType mCoreTraceStartEvent;
    EventType mCoreTraceEndEvent;
    EventType mMemoryTileTraceStartEvent;
    EventType mMemoryTileTraceEndEvent;
    EventType mInterfaceTileTraceStartEvent;
    EventType mInterfaceTileTraceEndEvent;

    EventVector mCoreCounterStartEvents;
    EventVector mCoreCounterEndEvents;
    ValueVector mCoreCounterEventValues;

    EventVector mMemoryCounterStartEvents;
    EventVector mMemoryCounterEndEvents;
    ValueVector mMemoryCounterEventValues;

    EventVector mInterfaceCounterStartEvents;
    EventVector mInterfaceCounterEndEvents;
    ValueVector mInterfaceCounterEventValues;

    // Tile locations to apply trace end and flush
    std::vector<XAie_LocType> mTraceFlushLocs;
    std::vector<XAie_LocType> mMemoryTileTraceFlushLocs;
    std::vector<XAie_LocType> mInterfaceTileTraceFlushLocs;

    // Keep track of number of events reserved per module and/or tile
    int mNumTileTraceEvents[static_cast<int>(module_type::num_types)][NUM_TRACE_EVENTS + 1];
  };

}  // namespace xdp

#endif
