/* Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#include <cstring>
#include <vector>

#include "core/edge/include/pscontext.h"
#include "core/edge/user/shim.h"
#include "event_configuration.h"
#include "xaiefal/xaiefal.hpp"
#include "xaiengine.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_trace/x86/aie_trace_kernel_config.h"

// User private data structure container (context object) definition
class xrtHandles : public pscontext {
 public:
  XAie_DevInst* aieDevInst = nullptr;
  xaiefal::XAieDev* aieDev = nullptr;
  xclDeviceHandle handle = nullptr;
  std::vector<XAie_LocType> traceFlushLocs;

  xrtHandles() = default;
  ~xrtHandles()
  {
    // aieDevInst is not owned by xrtHandles, so don't delete here
    if (aieDev != nullptr)
      delete aieDev;
    // handle is not owned by xrtHandles, so don't close or delete here
  }
};

// Anonymous namespace for helper functions used in this file
namespace {
  using Messages = xdp::built_in::Messages;

  void addMessage(xdp::built_in::MessageConfiguration* msgcfg, xdp::built_in::Messages ERROR_MSG,
                  std::vector<uint32_t>& paramsArray)
  {
    static int messageCounter = 0;

    if (messageCounter < xdp::built_in::MessageConfiguration::MAX_NUM_MESSAGES) {
      msgcfg->packets[messageCounter].messageCode = static_cast<uint8_t>(ERROR_MSG);
      std::copy(std::begin(paramsArray), std::end(paramsArray), std::begin(msgcfg->packets[messageCounter].params));
      messageCounter++;
      msgcfg->numMessages = messageCounter;
    }
  }

  inline uint32_t bcIdToEvent(int bcId)
  {
    // Core broadcast event base defined on AIE1 as 107 in architecture
    constexpr int core_broadcast_event_base = 107;
    return static_cast<uint32_t>(bcId + core_broadcast_event_base);
  }

  bool tileHasFreeRsc(xaiefal::XAieDev* aieDevice, XAie_LocType& loc, EventConfiguration& config,
                      const xdp::built_in::TraceInputConfiguration* params, xdp::built_in::MessageConfiguration* msgcfg,
                      const xdp::built_in::MetricSet metricSet)
  {
    auto stats = aieDevice->getRscStat(XAIEDEV_DEFAULT_GROUP_AVAIL);
    uint32_t available = 0;
    uint32_t required = 0;

    // Core Module perf counters
    available = stats.getNumRsc(loc, XAIE_CORE_MOD, xaiefal::XAIE_PERFCOUNT);
    required = config.coreCounterStartEvents.size();
    if (params->useDelay) {
      ++required;
      if (params->useOneDelayCounter)
        ++required;
    } else if (params->useGraphIterator)
      ++required;

    if (available < required) {
      std::vector<uint32_t> src = {available, required, 0, 0};
      addMessage(msgcfg, Messages::NO_CORE_MODULE_PCS, src);
      return false;
    }

    // Core Module trace slots
    available = stats.getNumRsc(loc, XAIE_CORE_MOD, xaiefal::XAIE_TRACEEVENT);
    required = config.coreCounterStartEvents.size() + config.coreEventsBase[metricSet].size();
    if (available < required) {
      std::vector<uint32_t> src = {available, required, 0, 0};
      addMessage(msgcfg, Messages::NO_CORE_MODULE_TRACE_SLOTS, src);
      return false;
    }

    // Core Module broadcasts. 2 events for starting/ending trace
    available = stats.getNumRsc(loc, XAIE_CORE_MOD, xaiefal::XAIE_BROADCAST);
    required = config.memoryCrossEventsBase[metricSet].size() + 2;
    if (available < required) {
      std::vector<uint32_t> src = {available, required, 0, 0};
      addMessage(msgcfg, Messages::NO_CORE_MODULE_BROADCAST_CHANNELS, src);
      return false;
    }

    // Memory Module perf counters
    available = stats.getNumRsc(loc, XAIE_MEM_MOD, xaiefal::XAIE_PERFCOUNT);
    required = config.memoryCounterStartEvents.size();
    if (available < required) {
      std::vector<uint32_t> src = {available, required, 0, 0};
      addMessage(msgcfg, Messages::NO_MEM_MODULE_PCS, src);
      return false;
    }

    // Memory Module trace slots
    available = stats.getNumRsc(loc, XAIE_MEM_MOD, xaiefal::XAIE_TRACEEVENT);
    required = config.memoryCounterStartEvents.size() + config.memoryCrossEventsBase[metricSet].size();
    if (available < required) {
      std::vector<uint32_t> src = {available, required, 0, 0};
      addMessage(msgcfg, Messages::NO_MEM_MODULE_TRACE_SLOTS, src);
      return false;
    }

    // No need to check memory module broadcast

    return true;
  }

  void releaseCurrentTileCounters(EventConfiguration& config)
  {
    while (!config.mCoreCounters.empty()) {
      config.mCoreCounters.back()->stop();
      config.mCoreCounters.back()->release();
      config.mCoreCounters.pop_back();
    }

    while (!config.mMemoryCounters.empty()) {
      config.mMemoryCounters.back()->stop();
      config.mMemoryCounters.back()->release();
      config.mMemoryCounters.pop_back();
    }
  }

  bool configureStartIteration(xaiefal::XAieMod& core, EventConfiguration& config,
                               const xdp::built_in::TraceInputConfiguration* params)
  {
    XAie_ModuleType mod = XAIE_CORE_MOD;
    // Count up by 1 for every iteration
    auto pc = core.perfCounter();
    if (pc->initialize(mod, XAIE_EVENT_INSTR_EVENT_0_CORE, mod, XAIE_EVENT_INSTR_EVENT_0_CORE) != XAIE_OK)
      return false;
    if (pc->reserve() != XAIE_OK)
      return false;
    pc->changeThreshold(params->iterationCount);
    XAie_Events counterEvent;
    pc->getCounterEvent(mod, counterEvent);
    // Reset when done counting
    pc->changeRstEvent(mod, counterEvent);
    if (pc->start() != XAIE_OK)
      return false;

    config.coreTraceStartEvent = counterEvent;

    return true;
  }

  bool configureStartDelay(xaiefal::XAieMod& core, EventConfiguration& config,
                           const xdp::built_in::TraceInputConfiguration* params)
  {
    if (!params->useDelay)
      return false;

    // This algorithm daisy chains counters to get an effective 64 bit delay
    // counterLow -> counterHigh -> trace start
    uint32_t delayCyclesHigh = 0;
    uint32_t delayCyclesLow = 0;
    XAie_ModuleType mod = XAIE_CORE_MOD;

    if (!params->useOneDelayCounter) {
      // ceil(x/y) where x and y are  positive integers
      delayCyclesHigh = static_cast<uint32_t>(1 + ((params->delayCycles - 1) / std::numeric_limits<uint32_t>::max()));
      delayCyclesLow = static_cast<uint32_t>(params->delayCycles / delayCyclesHigh);
    } else {
      delayCyclesLow = static_cast<uint32_t>(params->delayCycles);
    }

    // Configure lower 32 bits
    auto pc = core.perfCounter();
    if (pc->initialize(mod, XAIE_EVENT_ACTIVE_CORE, mod, XAIE_EVENT_DISABLED_CORE) != XAIE_OK)
      return false;
    if (pc->reserve() != XAIE_OK)
      return false;
    pc->changeThreshold(delayCyclesLow);
    XAie_Events counterEvent;
    pc->getCounterEvent(mod, counterEvent);
    // Reset when done counting
    pc->changeRstEvent(mod, counterEvent);
    if (pc->start() != XAIE_OK)
      return false;

    // Configure upper 32 bits if necessary
    // Use previous counter to start a new counter
    if (!params->useOneDelayCounter && delayCyclesHigh) {
      auto pc = core.perfCounter();
      // Count by 1 when previous counter generates event
      if (pc->initialize(mod, counterEvent, mod, counterEvent) != XAIE_OK)
        return false;
      if (pc->reserve() != XAIE_OK)
        return false;
      pc->changeThreshold(delayCyclesHigh);
      pc->getCounterEvent(mod, counterEvent);
      // Reset when done counting
      pc->changeRstEvent(mod, counterEvent);
      if (pc->start() != XAIE_OK)
        return false;
    }

    config.coreTraceStartEvent = counterEvent;

    return true;
  }

  xdp::module_type getTileType(uint16_t absRow, uint16_t offset)
  {
    if (absRow == 0)
      return xdp::module_type::shim;
    if (absRow < offset)
      return xdp::module_type::mem_tile;
    return xdp::module_type::core;
  }

  bool setMetricsSettings(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice, EventConfiguration& config,
                          const xdp::built_in::TraceInputConfiguration* params,
                          xdp::built_in::TraceOutputConfiguration* tilecfg, xdp::built_in::MessageConfiguration* msgcfg,
                          std::vector<XAie_LocType>& traceFlushLocs)
  {
    xaiefal::Logger::get().setLogLevel(xaiefal::LogLevel::FAL_DEBUG);

    // Keep track of number of events reserved per tile
    int numTileCoreTraceEvents[params->NUM_CORE_TRACE_EVENTS + 1] = {0};
    int numTileMemoryTraceEvents[params->NUM_MEMORY_TRACE_EVENTS + 1] = {0};

    std::map<xdp::tile_type, xdp::built_in::MetricSet> configMetrics;
    for (int i = 0; i < params->numTiles; i++) {
      auto tile = xdp::tile_type();
      tile.row = params->tiles[i].row;
      tile.col = params->tiles[i].col;
      configMetrics.insert({tile, static_cast<xdp::built_in::MetricSet>(params->tiles[i].metricSet)});
    }

    int tile_idx = 0;

    // Decide when to use user event for trace end to enable flushing
    bool useTraceFlush = false;
    if ((params->useUserControl) || (params->useGraphIterator) || (params->useDelay)) {
      if (params->useUserControl)
        config.coreTraceStartEvent = XAIE_EVENT_INSTR_EVENT_0_CORE;
      config.coreTraceEndEvent = config.traceFlushEndEvent;
      useTraceFlush = true;

      std::vector<uint32_t> src = {0, 0, 0, 0};
      addMessage(msgcfg, Messages::ENABLE_TRACE_FLUSH, src);
      // if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(severity_level::info))
      //   xrt_core::message::send(severity_level::info, "XRT", "Enabling trace flush");
    }

    // Iterate over all used/specified tiles
    for (auto& tileMetric : configMetrics) {
      auto tile = tileMetric.first;
      auto col = tile.col;
      auto row = tile.row;
      auto type = getTileType(row, params->offset);

      auto& metricSet = tileMetric.second;
      // NOTE: resource manager requires absolute row number
      auto& core = aieDevice->tile(col, row).core();
      auto& memory = aieDevice->tile(col, row).mem();
      auto loc = XAie_TileLoc(col, row);

      // Store location to flush at end of run
      if (useTraceFlush) {
        if (type == xdp::module_type::core)
          traceFlushLocs.push_back(loc);
      }

      // AIE config object for this tile
      auto cfgTile = xdp::built_in::TileData(col, row);
      cfgTile.trace_metric_set = static_cast<uint8_t>(metricSet);
      cfgTile.type = static_cast<uint8_t>(type);

      // Get vector of pre-defined metrics for this set
      // NOTE: these are local copies as we are adding tile/counter-specific events
      std::vector<XAie_Events> coreEvents = config.coreEventsBase[metricSet];
      std::vector<XAie_Events> memoryCrossEvents = config.memoryCrossEventsBase[metricSet];
      std::vector<XAie_Events> memoryEvents;

      // Check Resource Availability
      // For now only counters are checked
      if (!tileHasFreeRsc(aieDevice, loc, config, params, msgcfg, metricSet)) {
        std::vector<uint32_t> src = {0, 0, 0, 0};
        addMessage(msgcfg, Messages::NO_RESOURCES, src);
        return 1;
      }
      //
      // 1. Reserve and start core module counters (as needed)
      //
      int numCoreCounters = 0;
      {
        XAie_ModuleType mod = XAIE_CORE_MOD;

        for (int i = 0; i < config.coreCounterStartEvents.size(); ++i) {
          auto perfCounter = core.perfCounter();
          if (perfCounter->initialize(mod, config.coreCounterStartEvents.at(i), mod,
                                      config.coreCounterEndEvents.at(i)) != XAIE_OK)
            break;
          if (perfCounter->reserve() != XAIE_OK)
            break;

          // NOTE: store events for later use in trace
          XAie_Events counterEvent;
          perfCounter->getCounterEvent(mod, counterEvent);
          int idx = static_cast<int>(counterEvent) - static_cast<int>(XAIE_EVENT_PERF_CNT_0_CORE);
          perfCounter->changeThreshold(config.coreCounterEventValues.at(i));

          // Set reset event based on counter number
          perfCounter->changeRstEvent(mod, counterEvent);
          coreEvents.push_back(counterEvent);

          // If no memory counters are used, then we need to broadcast the core counter
          if (config.memoryCounterStartEvents.empty())
            memoryCrossEvents.push_back(counterEvent);

          if (perfCounter->start() != XAIE_OK)
            break;

          // mCoreCounterTiles.push_back(tile);
          config.mCoreCounters.push_back(perfCounter);
          numCoreCounters++;

          // Update config file
          uint16_t phyEvent = 0;
          auto& cfg = cfgTile.core_trace_config.pc[idx];
          XAie_EventLogicalToPhysicalConv_16(aieDevInst, loc, mod, config.coreCounterStartEvents[i], &phyEvent);
          cfg.start_event = phyEvent;
          XAie_EventLogicalToPhysicalConv_16(aieDevInst, loc, mod, config.coreCounterEndEvents[i], &phyEvent);
          cfg.stop_event = phyEvent;
          XAie_EventLogicalToPhysicalConv_16(aieDevInst, loc, mod, counterEvent, &phyEvent);
          cfg.reset_event = phyEvent;
          cfg.event_value = config.coreCounterEventValues[i];
        }
      }

      //
      // 2. Reserve and start memory module counters (as needed)
      //
      int numMemoryCounters = 0;
      {
        XAie_ModuleType mod = XAIE_MEM_MOD;

        for (int i = 0; i < config.memoryCounterStartEvents.size(); ++i) {
          auto perfCounter = memory.perfCounter();
          if (perfCounter->initialize(mod, config.memoryCounterStartEvents.at(i), mod,
                                      config.memoryCounterEndEvents.at(i)) != XAIE_OK)
            break;
          if (perfCounter->reserve() != XAIE_OK)
            break;

          // Set reset event based on counter number
          XAie_Events counterEvent;
          perfCounter->getCounterEvent(mod, counterEvent);
          int idx = static_cast<int>(counterEvent) - static_cast<int>(XAIE_EVENT_PERF_CNT_0_MEM);
          perfCounter->changeThreshold(config.memoryCounterEventValues.at(i));

          perfCounter->changeRstEvent(mod, counterEvent);
          memoryEvents.push_back(counterEvent);

          if (perfCounter->start() != XAIE_OK)
            break;

          config.mMemoryCounters.push_back(perfCounter);
          numMemoryCounters++;

          // Update config file
          uint16_t phyEvent = 0;
          auto& cfg = cfgTile.memory_trace_config.pc[idx];
          XAie_EventLogicalToPhysicalConv_16(aieDevInst, loc, mod, config.memoryCounterStartEvents[i], &phyEvent);
          cfg.start_event = phyEvent;
          XAie_EventLogicalToPhysicalConv_16(aieDevInst, loc, mod, config.memoryCounterEndEvents[i], &phyEvent);
          cfg.stop_event = phyEvent;
          XAie_EventLogicalToPhysicalConv_16(aieDevInst, loc, mod, counterEvent, &phyEvent);
          cfg.reset_event = phyEvent;
          cfg.event_value = config.memoryCounterEventValues[i];
        }
      }

      // Catch when counters cannot be reserved: report, release, and return
      if ((numCoreCounters < config.coreCounterStartEvents.size()) ||
          (numMemoryCounters < config.memoryCounterStartEvents.size())) {
        std::vector<uint32_t> src = {static_cast<uint32_t>(config.coreCounterStartEvents.size()),
                                     static_cast<uint32_t>(config.memoryCounterStartEvents.size()), col,
                                     static_cast<uint32_t>(row)};
        addMessage(msgcfg, Messages::COUNTERS_NOT_RESERVED, src);
        releaseCurrentTileCounters(config);
        return 1;
      }

      //
      // 3. Configure Core Tracing Events
      //
      {
        XAie_ModuleType mod = XAIE_CORE_MOD;
        uint16_t phyEvent = 0;
        auto coreTrace = core.traceControl();

        // Delay cycles and user control are not compatible with each other
        if (params->useGraphIterator) {
          if (!configureStartIteration(core, config, params))
            break;
        } else if (params->useDelay) {
          if (!configureStartDelay(core, config, params))
            break;
        }

        // Set overall start/end for trace capture
        // Wendy said this should be done first
        if (coreTrace->setCntrEvent(config.coreTraceStartEvent, config.coreTraceEndEvent) != XAIE_OK)
          break;

        auto ret = coreTrace->reserve();
        if (ret != XAIE_OK) {
          std::vector<uint32_t> src = {col, static_cast<uint32_t>(row), 0, 0};
          addMessage(msgcfg, Messages::CORE_MODULE_TRACE_NOT_RESERVED, src);
          releaseCurrentTileCounters(config);
          return 1;
        }

        int numTraceEvents = 0;
        for (int i = 0; i < coreEvents.size(); i++) {
          uint8_t slot;
          if (coreTrace->reserveTraceSlot(slot) != XAIE_OK)
            break;
          if (coreTrace->setTraceEvent(slot, coreEvents[i]) != XAIE_OK)
            break;
          numTraceEvents++;

          // Update config file
          XAie_EventLogicalToPhysicalConv_16(aieDevInst, loc, mod, coreEvents[i], &phyEvent);
          cfgTile.core_trace_config.traced_events[slot] = phyEvent;
        }
        // Update config file
        XAie_EventLogicalToPhysicalConv_16(aieDevInst, loc, mod, config.coreTraceStartEvent, &phyEvent);
        cfgTile.core_trace_config.start_event = phyEvent;
        XAie_EventLogicalToPhysicalConv_16(aieDevInst, loc, mod, config.coreTraceEndEvent, &phyEvent);
        cfgTile.core_trace_config.stop_event = phyEvent;

        coreEvents.clear();
        numTileCoreTraceEvents[numTraceEvents]++;

        std::vector<uint32_t> src = {static_cast<uint32_t>(numTraceEvents), col, row, 0};
        addMessage(msgcfg, Messages::CORE_TRACE_EVENTS_RESERVED, src);

        if (coreTrace->setMode(XAIE_TRACE_EVENT_PC) != XAIE_OK)
          break;
        XAie_Packet pkt = {0, 0};
        if (coreTrace->setPkt(pkt) != XAIE_OK)
          break;
        if (coreTrace->start() != XAIE_OK)
          break;
      }

      //
      // 4. Configure Memory Tracing Events
      //
      // TODO: Configure group or combo events where applicable
      uint32_t coreToMemBcMask = 0;
      {
        auto memoryTrace = memory.traceControl();
        // Set overall start/end for trace capture
        // Wendy said this should be done first
        if (memoryTrace->setCntrEvent(config.coreTraceStartEvent, config.coreTraceEndEvent) != XAIE_OK)
          break;

        auto ret = memoryTrace->reserve();
        if (ret != XAIE_OK) {
          std::vector<uint32_t> src = {col, static_cast<uint32_t>(row), 0, 0};
          addMessage(msgcfg, Messages::MEMORY_MODULE_TRACE_NOT_RESERVED, src);
          releaseCurrentTileCounters(config);
          return 1;
        }

        int numTraceEvents = 0;

        // Configure cross module events
        for (int i = 0; i < memoryCrossEvents.size(); i++) {
          uint32_t bcBit = 0x1;
          auto TraceE = memory.traceEvent();
          TraceE->setEvent(XAIE_CORE_MOD, memoryCrossEvents[i]);
          if (TraceE->reserve() != XAIE_OK)
            break;

          int bcId = TraceE->getBc();
          coreToMemBcMask |= (bcBit << bcId);

          if (TraceE->start() != XAIE_OK)
            break;
          numTraceEvents++;

          // Update config file
          uint32_t S = 0;
          XAie_LocType L;
          XAie_ModuleType M;
          TraceE->getRscId(L, M, S);
          cfgTile.memory_trace_config.traced_events[S] = bcIdToEvent(bcId);
          auto mod = XAIE_CORE_MOD;
          uint16_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv_16(aieDevInst, loc, mod, memoryCrossEvents[i], &phyEvent);
          cfgTile.core_trace_config.internal_events_broadcast[bcId] = phyEvent;
        }

        // Configure same module events
        for (int i = 0; i < memoryEvents.size(); i++) {
          auto TraceE = memory.traceEvent();
          TraceE->setEvent(XAIE_MEM_MOD, memoryEvents[i]);
          if (TraceE->reserve() != XAIE_OK)
            break;
          if (TraceE->start() != XAIE_OK)
            break;
          numTraceEvents++;

          // Update config file
          // Get Trace slot
          uint32_t S = 0;
          XAie_LocType L;
          XAie_ModuleType M;
          TraceE->getRscId(L, M, S);
          // Get Physical event
          auto mod = XAIE_MEM_MOD;
          uint16_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv_16(aieDevInst, loc, mod, memoryEvents[i], &phyEvent);
          cfgTile.memory_trace_config.traced_events[S] = phyEvent;
        }

        // Update config file
        {
          // Add Memory module trace control events
          uint32_t bcBit = 0x1;
          auto bcId = memoryTrace->getStartBc();
          coreToMemBcMask |= (bcBit << bcId);
          auto mod = XAIE_CORE_MOD;
          uint16_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv_16(aieDevInst, loc, mod, config.coreTraceStartEvent, &phyEvent);
          cfgTile.memory_trace_config.start_event = bcIdToEvent(bcId);
          cfgTile.core_trace_config.internal_events_broadcast[bcId] = phyEvent;

          bcBit = 0x1;
          bcId = memoryTrace->getStopBc();
          coreToMemBcMask |= (bcBit << bcId);
          XAie_EventLogicalToPhysicalConv_16(aieDevInst, loc, mod, config.coreTraceEndEvent, &phyEvent);
          cfgTile.memory_trace_config.stop_event = bcIdToEvent(bcId);
          cfgTile.core_trace_config.internal_events_broadcast[bcId] = phyEvent;
        }

        // Odd absolute rows change east mask end even row change west mask
        if (row % 2) {
          cfgTile.core_trace_config.broadcast_mask_east = coreToMemBcMask;
        } else {
          cfgTile.core_trace_config.broadcast_mask_west = coreToMemBcMask;
        }
        // Done update config file

        memoryEvents.clear();
        numTileMemoryTraceEvents[numTraceEvents]++;

        std::vector<uint32_t> src = {static_cast<uint32_t>(numTraceEvents), col, row, 0};
        addMessage(msgcfg, Messages::MEMORY_TRACE_EVENTS_RESERVED, src);

        if (memoryTrace->setMode(XAIE_TRACE_EVENT_TIME) != XAIE_OK)
          break;
        XAie_Packet pkt = {0, 1};
        if (memoryTrace->setPkt(pkt) != XAIE_OK)
          break;
        if (memoryTrace->start() != XAIE_OK)
          break;

        // Update memory packet type in config file
        // NOTE: Use time packets for memory module (type 1)
        cfgTile.memory_trace_config.packet_type = 1;
      }
      tilecfg->tiles[tile_idx] = cfgTile;
      tile_idx++;
    }  // For tiles

    // Report trace events reserved per tile
    {
      for (int n = 0; n <= params->NUM_CORE_TRACE_EVENTS; ++n) {
        if (numTileCoreTraceEvents[n] == 0)
          continue;
        if (n != params->NUM_CORE_TRACE_EVENTS)
          tilecfg->numTileCoreTraceEvents[n] = numTileCoreTraceEvents[n];
      }
    }
    {
      for (int n = 0; n <= params->NUM_MEMORY_TRACE_EVENTS; ++n) {
        if (numTileMemoryTraceEvents[n] == 0)
          continue;
        if (n != params->NUM_MEMORY_TRACE_EVENTS)
          tilecfg->numTileMemoryTraceEvents[n] = numTileMemoryTraceEvents[n];
      }
    }

    return 0;
  }  // end setMetricsSettings

  void flushAieTileTraceModule(XAie_DevInst* aieDevInst, EventConfiguration& config,
                               std::vector<XAie_LocType>& traceFlushLocs)
  {
    /*
     * Flush for trace windowing
     */
    if (traceFlushLocs.empty())
      return;

    for (const auto& loc : traceFlushLocs)
      XAie_EventGenerate(aieDevInst, loc, XAIE_CORE_MOD, config.traceFlushEndEvent);
    traceFlushLocs.clear();
  }

}  // namespace

#ifdef __cplusplus
extern "C" {
#endif

// The PS kernel initialization function
__attribute__((visibility("default"))) xrtHandles* aie_trace_config_init(xclDeviceHandle handle,
                                                                         const xuid_t xclbin_uuid)
{
  xrtHandles* constructs = new xrtHandles;
  if (!constructs)
    return nullptr;

  constructs->handle = handle;
  return constructs;
}

// The main PS kernel functionality
__attribute__((visibility("default"))) int aie_trace_config(uint8_t* input, uint8_t* output, uint8_t* messageOutput,
                                                            int iteration, xrtHandles* constructs)
{
  if (constructs == nullptr)
    return 0;

  auto drv = ZYNQ::shim::handleCheck(constructs->handle);
  if (!drv)
    return 0;

  auto aieArray = drv->getAieArray();
  if (!aieArray)
    return 0;

  constructs->aieDevInst = aieArray->get_dev();
  if (!constructs->aieDevInst)
    return 0;

  if (constructs->aieDev == nullptr)
    constructs->aieDev = new xaiefal::XAieDev(constructs->aieDevInst, false);

  EventConfiguration config;

  // setup iteration
  if (iteration == 0) {
    xdp::built_in::TraceInputConfiguration* params = reinterpret_cast<xdp::built_in::TraceInputConfiguration*>(input);
    config.initialize(params);

    xdp::built_in::MessageConfiguration* messageStruct =
        reinterpret_cast<xdp::built_in::MessageConfiguration*>(messageOutput);

    // Using malloc/free instead of new/delete because the struct treats the
    // last element as a variable sized array
    std::size_t total_size =
        sizeof(xdp::built_in::TraceOutputConfiguration) + sizeof(xdp::built_in::TileData[params->numTiles - 1]);
    xdp::built_in::TraceOutputConfiguration* tilecfg = (xdp::built_in::TraceOutputConfiguration*)malloc(total_size);

    tilecfg->numTiles = params->numTiles;
    setMetricsSettings(constructs->aieDevInst, constructs->aieDev, config, params, tilecfg, messageStruct,
                       constructs->traceFlushLocs);

    uint8_t* out = reinterpret_cast<uint8_t*>(tilecfg);
    std::memcpy(output, out, total_size);

    // Clean up
    free(tilecfg);

    // flush iteraiton
  } else if (iteration == 1) {
    flushAieTileTraceModule(constructs->aieDevInst, config, constructs->traceFlushLocs);
  }

  return 0;
}

// The final function for the PS kernel
__attribute__((visibility("default"))) int aie_trace_config_fini(xrtHandles* handles)
{
  if (handles != nullptr)
    delete handles;
  return 0;
}

#ifdef __cplusplus
}
#endif
