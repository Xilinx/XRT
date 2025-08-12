// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef AIE_TRACE_DOT_H
#define AIE_TRACE_DOT_H

#include <cstdint>

#include "xaiefal/xaiefal.hpp"
#include "xdp/profile/plugin/aie_trace/aie_trace_impl.h"
#include "xdp/profile/plugin/aie_trace/util/aie_trace_config.h"

namespace xdp {

  class AieTrace_EdgeImpl : public AieTraceImpl {
  public:
    AieTrace_EdgeImpl(VPDatabase* database, std::shared_ptr<AieTraceMetadata> metadata);
    ~AieTrace_EdgeImpl() = default;

    void updateDevice() override;
    void flushTraceModules() override;
    void pollTimers(uint64_t index, void* handle) override;
    void freeResources() override;
    void* setAieDeviceInst(void* handle, uint64_t deviceID) override;

  private:
    uint64_t checkTraceBufSize(uint64_t size) override;
    bool tileHasFreeRsc(xaiefal::XAieDev* aieDevice, XAie_LocType& loc, 
                        const module_type type, const std::string& metricSet);
    bool setMetricsSettings(uint64_t deviceId, void* handle);

  private:
    typedef XAie_Events EventType;
    typedef std::vector<EventType> EventVector;
    typedef std::vector<uint32_t> ValueVector;
    XAie_DevInst* aieDevInst = nullptr;
    xaiefal::XAieDev* aieDevice = nullptr;

    // AIE resources
    std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> perfCounters;
    std::vector<std::shared_ptr<xaiefal::XAieStreamPortSelect>> streamPorts;

    std::map<std::string, EventVector> coreEventSets;
    std::map<std::string, EventVector> memoryEventSets;
    std::map<std::string, EventVector> memoryTileEventSets;
    std::map<std::string, EventVector> interfaceTileEventSets;

    // Counter metrics (same for all sets)
    EventType coreTraceStartEvent;
    EventType coreTraceEndEvent;
    EventType memoryTileTraceStartEvent;
    EventType memoryTileTraceEndEvent;
    EventType interfaceTileTraceStartEvent;
    EventType interfaceTileTraceEndEvent;

    EventVector coreCounterStartEvents;
    EventVector coreCounterEndEvents;
    ValueVector coreCounterEventValues;

    EventVector memoryCounterStartEvents;
    EventVector memoryCounterEndEvents;
    ValueVector memoryCounterEventValues;

    EventVector interfaceCounterStartEvents;
    EventVector interfaceCounterEndEvents;
    ValueVector interfaceCounterEventValues;

    // Tile locations to apply trace end and flush
    std::vector<XAie_LocType> traceFlushLocs;
    std::vector<XAie_LocType> memoryTileTraceFlushLocs;
    std::vector<XAie_LocType> interfaceTileTraceFlushLocs;

    // Keep track of number of events reserved per module and/or tile
    int mNumTileTraceEvents[static_cast<int>(module_type::num_types)][NUM_TRACE_EVENTS + 1];
  };

}  // namespace xdp

#endif
