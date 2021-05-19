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

#define XDP_SOURCE

#include "xdp/profile/plugin/aie_trace/aie_trace_plugin.h"
#include "xdp/profile/writer/aie_trace/aie_trace_writer.h"
#include "xdp/profile/writer/aie_trace/aie_trace_config_writer.h"

#include "core/common/xrt_profiling.h"
#include "core/edge/user/shim.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/device/aie_trace/aie_trace_offload.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"

#include "core/common/message.h"
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <memory>

#define NUM_CORE_TRACE_EVENTS   8
#define NUM_MEMORY_TRACE_EVENTS 8
#define CORE_BROADCAST_EVENT_BASE 107

namespace {
  static void* fetchAieDevInst(void* devHandle)
  {
    auto drv = ZYNQ::shim::handleCheck(devHandle);
    if (!drv)
      return nullptr ;
    auto aieArray = drv->getAieArray() ;
    if (!aieArray)
      return nullptr ;
    return aieArray->getDevInst() ;
  }

  static void* allocateAieDevice(void* devHandle)
  {
    XAie_DevInst* aieDevInst =
      static_cast<XAie_DevInst*>(fetchAieDevInst(devHandle)) ;
    if (!aieDevInst)
      return nullptr ;
    return new xaiefal::XAieDev(aieDevInst, false) ;
  }

  static void deallocateAieDevice(void* aieDevice)
  {
    xaiefal::XAieDev* object = static_cast<xaiefal::XAieDev*>(aieDevice) ;
    if (object != nullptr)
      delete object ;
  }
} // end anonymous namespace

namespace xdp {

  AieTracePlugin::AieTracePlugin()
                : XDPPlugin()
  {
    db->registerPlugin(this);

    // Pre-defined metric sets
    metricSets = {"functions", "functions_partial_stalls", "functions_all_stalls", "all"};
    
    // Pre-defined metric sets
    //
    // **** Core Module Trace ****
    // functions: "traced_events": [35, 36, 7, 8, 0, 0, 0, 0]
    // functions_partial_stalls: "traced_events": [35, 36, 7, 8, 0, 0, 0, 0]
    // functions_all_stalls: "traced_events": [35, 36, 7, 8, 0, 0, 0, 0]
    // all: "traced_events": [35, 36, 7, 8, 0, 0, 0, 0]
    //      "group_event_config": {
    //          "2": 0,
    //          "15": 0,
    //          "22": 15,
    //          "32": 12,
    //          "46": 0,
    //          "47": 0,
    //          "73": 8738,
    //          "106": 0,
    //          "123": 0
    //      },
    // NOTE: these are supplemented with counter events as those are dependent on counter #
    coreEventSets = {
      {"functions",                {XAIE_EVENT_INSTR_CALL_CORE,       XAIE_EVENT_INSTR_RETURN_CORE}},
      {"functions_partial_stalls", {XAIE_EVENT_INSTR_CALL_CORE,       XAIE_EVENT_INSTR_RETURN_CORE}},
      {"functions_all_stalls",     {XAIE_EVENT_INSTR_CALL_CORE,       XAIE_EVENT_INSTR_RETURN_CORE}},
      {"all",                      {XAIE_EVENT_INSTR_CALL_CORE,       XAIE_EVENT_INSTR_RETURN_CORE}}
    };

    // These are also broadcast to memory module
    coreTraceStartEvent = XAIE_EVENT_ACTIVE_CORE;
    coreTraceEndEvent   = XAIE_EVENT_DISABLED_CORE;
    
    // **** Memory Module Trace ****
    // functions: "traced_events": [120, 119, 5, 6, 0, 0, 0, 0]
    // functions_partial_stalls: "traced_events": [120, 119, 118, 117, 116, 5, 6, 0]
    // functions_all_stalls: "traced_events": [120, 119, 118, 117, 116, 115, 5, 6]
    // all: "traced_events": [120, 119, 118, 117, 116, 115, 5, 6]
    //
    // NOTE: core events listed here are broadcast by the resource manager
    // NOTE: these are supplemented with counter events as those are dependent on counter #
    memoryEventSets = {
      {"functions",                {XAIE_EVENT_INSTR_CALL_CORE,       XAIE_EVENT_INSTR_RETURN_CORE}},
      {"functions_partial_stalls", {XAIE_EVENT_INSTR_CALL_CORE,       XAIE_EVENT_INSTR_RETURN_CORE,
                                    XAIE_EVENT_STREAM_STALL_CORE, 
                                    XAIE_EVENT_CASCADE_STALL_CORE,    XAIE_EVENT_LOCK_STALL_CORE}},
      {"functions_all_stalls",     {XAIE_EVENT_INSTR_CALL_CORE,       XAIE_EVENT_INSTR_RETURN_CORE,
                                    XAIE_EVENT_MEMORY_STALL_CORE,     XAIE_EVENT_STREAM_STALL_CORE, 
                                    XAIE_EVENT_CASCADE_STALL_CORE,    XAIE_EVENT_LOCK_STALL_CORE}},
      {"all",                      {XAIE_EVENT_GROUP_CORE_STALL_CORE, XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE}}
    };

    // **** Core Module Counters ****
    // NOTE: reset events are dependent on actual profile counter reserved
    coreCounterStartEvents   = {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_ACTIVE_CORE};
    coreCounterEndEvents     = {XAIE_EVENT_DISABLED_CORE,             XAIE_EVENT_DISABLED_CORE};
    coreCounterResetEvents   = {XAIE_EVENT_PERF_CNT_0_CORE,           XAIE_EVENT_PERF_CNT_1_CORE,
                                XAIE_EVENT_PERF_CNT_2_CORE,           XAIE_EVENT_PERF_CNT_3_CORE};
    coreCounterEventValues   = {1020, 1040400};

    // **** Memory Module Counters ****
    // NOTE: reset events are dependent on actual profile counter reserved
    memoryCounterStartEvents = {XAIE_EVENT_TRUE_MEM,                  XAIE_EVENT_TRUE_MEM};
    memoryCounterEndEvents   = {XAIE_EVENT_NONE_MEM,                  XAIE_EVENT_NONE_MEM};
    memoryCounterEventValues = {1020, 1040400};
  }

  AieTracePlugin::~AieTracePlugin()
  {
    if(VPDatabase::alive()) {
      try {
        writeAll(false);
      }
      catch(...) {
      }
      db->unregisterPlugin(this);
    }

    // If the database is dead, then we must have already forced a 
    //  write at the database destructor so we can just move on

    for(auto h : deviceHandles) {
      xclClose(h);
    }
  }

  // Convert broadcast ID to event ID
  inline uint32_t AieTracePlugin::bcIdToEvent(int bcId)
  {
    return bcId + CORE_BROADCAST_EVENT_BASE;
  }

  // Release counters from latest tile (because something went wrong)
  void AieTracePlugin::releaseCurrentTileCounters(int numCoreCounters, int numMemoryCounters)
  {
    for (int i=0; i < numCoreCounters; i++) {
      coreCounters.back()->stop();
      coreCounters.back()->release();
      coreCounters.pop_back();
      coreCounterTiles.pop_back();
    }
    for (int i=0; i < numMemoryCounters; i++) {
      memoryCounters.back()->stop();
      memoryCounters.back()->release();
      memoryCounters.pop_back();
    }
  }

  // Configure all resources necessary for trace control and events
  bool AieTracePlugin::setMetrics(uint64_t deviceId, void* handle)
  {
    // Catch when compile-time trace is specified (e.g., --event-trace=functions)
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);
    auto compilerOptions = xrt_core::edge::aie::get_aiecompiler_options(device.get());
    runtimeMetrics = (compilerOptions.event_trace == "runtime");
    if (!runtimeMetrics) {
      std::stringstream msg;
      msg << "Found compiler trace option of " << compilerOptions.event_trace 
          << ". No runtime AIE metrics will be changed.";
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg.str());
      return true;
    }

    XAie_DevInst* aieDevInst =
      static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle)) ;
    xaiefal::XAieDev* aieDevice =
      static_cast<xaiefal::XAieDev*>(db->getStaticInfo().getAieDevice(allocateAieDevice, deallocateAieDevice, handle)) ;
    if (!aieDevInst || !aieDevice) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", 
          "Unable to get AIE device. AIE event trace will not be available.");
      return false;
    }

    // Get AIE tiles and metric set
    std::string metricsStr = xrt_core::config::get_aie_trace_metrics();
    if (metricsStr.empty()) {
      std::string msg("The setting aie_trace_metrics was not specified in xrt.ini. AIE event trace will not be available.");
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      return false;
    }

    std::vector<std::string> vec;
    boost::split(vec, metricsStr, boost::is_any_of(":"));

    for (int i=0; i < vec.size(); ++i) {
      boost::replace_all(vec.at(i), "{", "");
      boost::replace_all(vec.at(i), "}", "");
    }

    // If requested, turn on debug fal messages
    if (xrt_core::config::get_verbosity() >= (uint32_t)xrt_core::message::severity_level::debug)
      xaiefal::Logger::get().setLogLevel(xaiefal::LogLevel::DEBUG);

    // Determine specification type based on vector size:
    //   * Size = 1: All tiles
    //     * aie_trace_metrics = <functions|functions_partial_stalls|functions_all_stalls|all>
    //   * Size = 2: Single tile or kernel name (supported in future release)
    //     * aie_trace_metrics = {<column>,<row>}:<functions|functions_partial_stalls|functions_all_stalls|all>
    //     * aie_trace_metrics= <kernel name>:<functions|functions_partial_stalls|functions_all_stalls|all>
    //   * Size = 3: Range of tiles (supported in future release)
    //     * aie_trace_metrics= {<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<functions|functions_partial_stalls|functions_all_stalls|all>
    metricSet = vec.at( vec.size()-1 );

    if (metricSets.find(metricSet) == metricSets.end()) {
      std::string defaultSet = "functions";
      std::stringstream msg;
      msg << "Unable to find AIE trace metric set " << metricSet 
          << ". Using default of " << defaultSet << ".";
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
      metricSet = defaultSet;
    }

    // Capture all tiles across all graphs
    // NOTE: future releases will support the specification of tile subsets
    auto graphs = xrt_core::edge::aie::get_graphs(device.get());
    std::vector<xrt_core::edge::aie::tile_type> tiles;
    for (auto& graph : graphs) {
      auto currTiles = xrt_core::edge::aie::get_tiles(device.get(), graph);
      std::copy(currTiles.begin(), currTiles.end(), back_inserter(tiles));
    }

    // Keep track of number of events reserved per tile
    int numTileCoreTraceEvents[NUM_CORE_TRACE_EVENTS+1] = {0};
    int numTileMemoryTraceEvents[NUM_MEMORY_TRACE_EVENTS+1] = {0};

    // Iterate over all used/specified tiles
    for (auto& tile : tiles) {
      auto  col    = tile.col;
      auto  row    = tile.row;
      // NOTE: resource manager requires absolute row number
      auto& core   = aieDevice->tile(col, row + 1).core();
      auto& memory = aieDevice->tile(col, row + 1).mem();

      auto loc = XAie_TileLoc(col, row + 1);

      // AIE config object for this tile
      auto cfgTile  = std::make_unique<aie_cfg_tile>(col, row + 1);

      // Get vector of pre-defined metrics for this set
      // NOTE: these are local copies as we are adding tile/counter-specific events
      EventVector coreEvents;
      EventVector memoryCrossEvents;
      EventVector memoryEvents;
      std::copy(coreEventSets[metricSet].begin(), coreEventSets[metricSet].end(), 
                back_inserter(coreEvents));
      std::copy(memoryEventSets[metricSet].begin(), memoryEventSets[metricSet].end(), 
                back_inserter(memoryCrossEvents));

      //
      // 1. Reserve and start core module counters
      //
      int numCoreCounters = 0;
      XAie_ModuleType mod = XAIE_CORE_MOD;
      for (int i=0; i < coreCounterStartEvents.size(); ++i) {
        auto perfCounter = core.perfCounter();
        auto ret = perfCounter->initialize(mod, coreCounterStartEvents.at(i),
                                           mod, coreCounterEndEvents.at(i));
        if (ret != XAIE_OK) break;
        ret = perfCounter->reserve();
        if (ret != XAIE_OK) break;

        // NOTE: store events for later use in trace
        XAie_Events counterEvent;
        perfCounter->getCounterEvent(mod, counterEvent);
        int idx = static_cast<int>(counterEvent) - static_cast<int>(XAIE_EVENT_PERF_CNT_0_CORE);
        // Following function is broken on FAL so we need to directly call aie driver api
        // perfCounter->changeThreshold(coreCounterEventValues.at(i));
        XAie_PerfCounterEventValueSet(aieDevInst, loc, XAIE_CORE_MOD, idx, coreCounterEventValues.at(i));

        // Set reset event based on counter number
        perfCounter->changeRstEvent(mod, counterEvent);
        coreEvents.push_back(counterEvent);

        ret = perfCounter->start();
        if (ret != XAIE_OK) break;

        coreCounterTiles.push_back(tile);
        coreCounters.push_back(perfCounter);
        numCoreCounters++;

        // Update config file
        uint8_t phyEvent = 0;
        auto& cfg = cfgTile->core_trace_config.pc[idx];
        XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, coreCounterStartEvents[i], &phyEvent);
        cfg.start_event = phyEvent;
        XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, coreCounterEndEvents[i], &phyEvent);
        cfg.stop_event = phyEvent;
        XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, counterEvent, &phyEvent);
        cfg.reset_event = phyEvent;
        cfg.event_value = coreCounterEventValues[i];
      }

      //
      // 2. Reserve and start memory module counters
      //
      int numMemoryCounters = 0;
      for (int i=0; i < memoryCounterStartEvents.size(); ++i) {
        auto perfCounter = memory.perfCounter();
        XAie_ModuleType mod = XAIE_MEM_MOD;
        auto ret = perfCounter->initialize(mod, memoryCounterStartEvents.at(i),
                                           mod, memoryCounterEndEvents.at(i));
        if (ret != XAIE_OK) break;
        ret = perfCounter->reserve();
        if (ret != XAIE_OK) break;

        // Set reset event based on counter number
        XAie_Events counterEvent;
        perfCounter->getCounterEvent(mod, counterEvent);
        int idx = static_cast<int>(counterEvent) - static_cast<int>(XAIE_EVENT_PERF_CNT_0_MEM);
        // Following function is broken on FAL so we need to directly call aie driver api
        // perfCounter->changeThreshold(memoryCounterEventValues.at(i));
        XAie_PerfCounterEventValueSet(aieDevInst, loc, XAIE_MEM_MOD, idx, memoryCounterEventValues.at(i));

        perfCounter->changeRstEvent(mod, counterEvent);
        memoryEvents.push_back(counterEvent);

        ret = perfCounter->start();
        if (ret != XAIE_OK) break;

        memoryCounters.push_back(perfCounter);
        numMemoryCounters++;

        // Update config file
        uint8_t phyEvent = 0;
        auto& cfg = cfgTile->memory_trace_config.pc[idx];
        XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, memoryCounterStartEvents[i], &phyEvent);
        cfg.start_event = phyEvent;
        XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, memoryCounterEndEvents[i], &phyEvent);
        cfg.stop_event = phyEvent;
        XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, counterEvent, &phyEvent);
        cfg.reset_event = phyEvent;
        cfg.event_value = memoryCounterEventValues[i];
        cfgTile->memory_trace_config.packet_type=1;
      }

      // Catch when counters cannot be reserved: report, release, and return
      if ((numCoreCounters < coreCounterStartEvents.size())
          || (numMemoryCounters < memoryCounterStartEvents.size())) {
        std::stringstream msg;
        msg << "Unable to reserve " << coreCounterStartEvents.size() << " core counters"
            << " and " << memoryCounterStartEvents.size() << " memory counters"
            << " for AIE tile (" << col << "," << row + 1 << ") required for trace.";
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());

        releaseCurrentTileCounters(numCoreCounters, numMemoryCounters);
        return false;
      }

      //
      // 3. Configure Core Tracing Events
      //
      // TODO: Configure group or combo events where applicable
      {
        XAie_ModuleType mod = XAIE_CORE_MOD;
        uint8_t phyEvent = 0;
        auto coreTrace = core.traceControl();
        // Set overall start/end for trace capture
        // Wendy said this should be done first
        auto ret = coreTrace->setCntrEvent(coreTraceStartEvent, coreTraceEndEvent);
        if (ret != XAIE_OK) break;

        ret = coreTrace->reserve();
        if (ret != XAIE_OK) {
          std::stringstream msg;
          msg << "Unable to reserve core module trace control for AIE tile (" 
              << col << "," << row + 1 << ").";
          xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());

          releaseCurrentTileCounters(numCoreCounters, numMemoryCounters);
          return false;
        }

        int numTraceEvents = 0;
        for (int i=0; i < coreEvents.size(); i++) {
          uint8_t slot;
          ret = coreTrace->reserveTraceSlot(slot);
          if (ret != XAIE_OK) break;
          ret = coreTrace->setTraceEvent(slot, coreEvents[i]);
          if (ret != XAIE_OK) break;
          numTraceEvents++;

          // Update config file
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, coreEvents[i], &phyEvent);
          cfgTile->core_trace_config.traced_events[slot] = phyEvent;
        }
        // Update config file
        XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, coreTraceStartEvent, &phyEvent);
        cfgTile->core_trace_config.start_event = phyEvent;
        XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, coreTraceEndEvent, &phyEvent);
        cfgTile->core_trace_config.stop_event = phyEvent;
        
        coreEvents.clear();
        numTileCoreTraceEvents[numTraceEvents]++;

        std::stringstream msg;
        msg << "Reserved " << numTraceEvents << " core trace events for AIE tile (" << col << "," << row << ").";
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());

        if (ret != XAIE_OK) break;
        ret = coreTrace->setMode(XAIE_TRACE_EVENT_PC);
        if (ret != XAIE_OK) break;
        XAie_Packet pkt = {0, 0};
        ret = coreTrace->setPkt(pkt);
        if (ret != XAIE_OK) break;
        ret = coreTrace->start();
        if (ret != XAIE_OK) break;
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
        auto ret = memoryTrace->setCntrEvent(coreTraceStartEvent, coreTraceEndEvent);
        if (ret != XAIE_OK) break;

        ret = memoryTrace->reserve();
        if (ret != XAIE_OK) {
          std::stringstream msg;
          msg << "Unable to reserve memory module trace control for AIE tile (" 
              << col << "," << row + 1 << ").";
          xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());

          releaseCurrentTileCounters(numCoreCounters, numMemoryCounters);
          return false;
        }

        int numTraceEvents = 0;
        
        // Configure cross module events
        for (int i=0; i < memoryCrossEvents.size(); i++) {
          uint32_t bcBit = 0x1;
          auto TraceE = memory.traceEvent();
          TraceE->setEvent(XAIE_CORE_MOD, memoryCrossEvents[i]);
          if (ret != XAIE_OK) break;
          auto ret = TraceE->reserve();
          if (ret != XAIE_OK) break;

          int bcId = TraceE->getBc();
          if (ret != XAIE_OK) break;
          coreToMemBcMask |= (bcBit << bcId);

          ret = TraceE->start();
          if (ret != XAIE_OK) break;

          numTraceEvents++;

          // Update config file
          uint32_t S = 0;
          XAie_LocType L;
          XAie_ModuleType M;
          TraceE->getRscId(L, M, S);
          cfgTile->memory_trace_config.traced_events[S] = bcIdToEvent(bcId);
          auto mod = XAIE_CORE_MOD;
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, memoryCrossEvents[i], &phyEvent);
          cfgTile->core_trace_config.internal_events_broadcast[bcId] = phyEvent;
        }

        // Configure same module events
        for (int i=0; i < memoryEvents.size(); i++) {
          auto TraceE = memory.traceEvent();
          TraceE->setEvent(XAIE_MEM_MOD, memoryEvents[i]);
          if (ret != XAIE_OK) break;
          auto ret = TraceE->reserve();
          if (ret != XAIE_OK) break;
          ret = TraceE->start();
          if (ret != XAIE_OK) break;
          numTraceEvents++;

          // Update config file
          // Get Trace slot
          uint32_t S = 0;
          XAie_LocType L;
          XAie_ModuleType M;
          TraceE->getRscId(L, M, S);
          // Get Physical event
          auto mod = XAIE_MEM_MOD;
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, memoryEvents[i], &phyEvent);
          cfgTile->memory_trace_config.traced_events[S] = phyEvent;
        }

        // Update config file
        {
          // Add Memory module trace control events
          uint32_t bcBit = 0x1;
          auto bcId = memoryTrace->getStartBc();
          coreToMemBcMask |= (bcBit << bcId);
          auto mod = XAIE_CORE_MOD;
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, coreTraceStartEvent, &phyEvent);
          cfgTile->memory_trace_config.start_event = bcIdToEvent(bcId);
          cfgTile->core_trace_config.internal_events_broadcast[bcId] = phyEvent;

          bcBit = 0x1;
          bcId = memoryTrace->getStopBc();
          coreToMemBcMask |= (bcBit << bcId);
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, coreTraceEndEvent, &phyEvent);
          cfgTile->memory_trace_config.stop_event = bcIdToEvent(bcId);
          cfgTile->core_trace_config.internal_events_broadcast[bcId] = phyEvent;
        }

        // Odd absolute rows change east mask end even row change west mask
        if ((row + 1) % 2) {
          cfgTile->core_trace_config.broadcast_mask_east = coreToMemBcMask;
        } else {
          cfgTile->core_trace_config.broadcast_mask_west = coreToMemBcMask;
        }
        // Done update config file

        memoryEvents.clear();
        numTileMemoryTraceEvents[numTraceEvents]++;

        std::stringstream msg;
        msg << "Reserved " << numTraceEvents << " memory trace events for AIE tile (" << col << "," << row << ").";
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());

        if (ret != XAIE_OK) break;
        ret = memoryTrace->setMode(XAIE_TRACE_EVENT_TIME);
        if (ret != XAIE_OK) break;
        XAie_Packet  pkt = {0, 1};
        ret = memoryTrace->setPkt(pkt);
        if (ret != XAIE_OK) break;
        ret = memoryTrace->start();
        if (ret != XAIE_OK) break;
      }

      std::stringstream msg;
      msg << "Adding tile (" << col << "," << row << ") to static database";
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());

      // Add config info to static database
      // NOTE: Do not access cfgTile after this
      (db->getStaticInfo()).addAIECfgTile(deviceId, cfgTile);
    } // For tiles

    // Report trace events reserved per tile
    {
      std::stringstream msg;
      msg << "AIE trace events reserved in core modules - ";
      for (int n=0; n <= NUM_CORE_TRACE_EVENTS; ++n) {
        if (numTileCoreTraceEvents[n] == 0) continue;
        msg << n << ": " << numTileCoreTraceEvents[n] << " tiles";
        if (n != NUM_CORE_TRACE_EVENTS) msg << ", ";

        (db->getStaticInfo()).addAIECoreEventResources(deviceId, n, numTileCoreTraceEvents[n]);
      }
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg.str());
    }
    {
      std::stringstream msg;
      msg << "AIE trace events reserved in memory modules - ";
      for (int n=0; n <= NUM_MEMORY_TRACE_EVENTS; ++n) {
        if (numTileMemoryTraceEvents[n] == 0) continue;
        msg << n << ": " << numTileMemoryTraceEvents[n] << " tiles";
        if (n != NUM_MEMORY_TRACE_EVENTS) msg << ", ";

        (db->getStaticInfo()).addAIEMemoryEventResources(deviceId, n, numTileMemoryTraceEvents[n]);
      }
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg.str());
    }

    return true;
  } // end setMetrics

  void AieTracePlugin::updateAIEDevice(void* handle)
  {
    if (handle == nullptr)
      return;

    char pathBuf[512];
    memset(pathBuf, 0, 512);
    xclGetDebugIPlayoutPath(handle, pathBuf, 512);

    std::string sysfspath(pathBuf);

    uint64_t deviceId = db->addDevice(sysfspath); // Get the unique device Id

    deviceIdToHandle[deviceId] = handle;
    // handle is not added to "deviceHandles" as this is user provided handle, not owned by XDP

    if (!(db->getStaticInfo()).isDeviceReady(deviceId)) {
      // first delete the offloader, logger
      // Delete the old offloader as data is already from it
      if(aieOffloaders.find(deviceId) != aieOffloaders.end()) {
        auto entry = aieOffloaders[deviceId];

        auto aieOffloader = std::get<0>(entry);
        auto aieLogger    = std::get<1>(entry);

        delete aieOffloader;
        delete aieLogger;
        // don't delete DeviceIntf

        aieOffloaders.erase(deviceId);
      }

      // Update the static database with information from xclbin
      (db->getStaticInfo()).updateDevice(deviceId, handle);
      {
        struct xclDeviceInfo2 info;
        if(xclGetDeviceInfo2(handle, &info) == 0) {
          (db->getStaticInfo()).setDeviceName(deviceId, std::string(info.mName));
        }
      }
    }

    // Set metrics for counters and trace events 
    if (!setMetrics(deviceId, handle)) {
      std::string msg("Unable to configure AIE trace control and events. No trace will be generated.");
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      return;
    }
    
    if (!(db->getStaticInfo()).isGMIORead(deviceId)) {
      // Update the AIE specific portion of the device
      // When new xclbin is loaded, the xclbin specific datastructure is already recreated
      std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle) ;
      if (device != nullptr) {
        for (auto& gmio : xrt_core::edge::aie::get_trace_gmios(device.get())) {
          (db->getStaticInfo()).addTraceGMIO(deviceId, gmio.id, gmio.shim_col, gmio.channel_number, gmio.stream_id, gmio.burst_len) ;
        }
      }
      (db->getStaticInfo()).setIsGMIORead(deviceId, true);
    }

    uint64_t numAIETraceOutput = (db->getStaticInfo()).getNumAIETraceStream(deviceId);
    if (numAIETraceOutput == 0) {
      // no AIE Trace Stream to offload trace, so return
      std::string msg("Neither PLIO nor GMIO trace infrastucture is found in the given design. So, AIE event trace will not be available.");
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      return;
    }

    DeviceIntf* deviceIntf = (db->getStaticInfo()).getDeviceIntf(deviceId);
    if (deviceIntf == nullptr) {
      // If DeviceIntf is not already created, create a new one to communicate with physical device
      deviceIntf = new DeviceIntf();
      try {
        deviceIntf->setDevice(new HalDevice(handle));
        deviceIntf->readDebugIPlayout();
      } catch(std::exception& e) {
        // Read debug IP layout could throw an exception
        std::stringstream msg;
        msg << "Unable to read debug IP layout for device " << deviceId << ": " << e.what();
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
        delete deviceIntf;
        return;
      }
      (db->getStaticInfo()).setDeviceIntf(deviceId, deviceIntf);
      // configure dataflow etc. may not be required here as those are for PL side
    }

    // Create runtime config file
    if (runtimeMetrics) {
      std::string configFile = "aie_event_runtime_config.json";
      writers.push_back(new AieTraceConfigWriter(configFile.c_str(),
                                                 deviceId, metricSet));
      (db->getStaticInfo()).addOpenedFile(configFile, "AIE_EVENT_RUNTIME_CONFIG");
    }

    // Create trace output files
    for(uint64_t n = 0; n < numAIETraceOutput; n++) {
      // Consider both Device Id and Stream Id to create the output file name
      std::string fileName = "aie_trace_" + std::to_string(deviceId) + "_" + std::to_string(n) + ".txt";
      writers.push_back(new AIETraceWriter(fileName.c_str(), deviceId, n,
                            "" /*version*/,
                            "" /*creationTime*/,
                            "" /*xrtVersion*/,
                            "" /*toolVersion*/));
      (db->getStaticInfo()).addOpenedFile(fileName, "AIE_EVENT_TRACE");

      std::stringstream msg;
      msg << "Creating AIE trace file " << fileName << " for device " << deviceId;
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg.str());
    }

    // Create AIE Trace Offloader
    uint64_t aieTraceBufSz = GetTS2MMBufSize(true /*isAIETrace*/);
    bool     isPLIO = ((db->getStaticInfo()).getNumTracePLIO(deviceId)) ? true : false;

#if 0
    uint8_t memIndex = 0;
    if(isPLIO) { // check only one memory for PLIO and for GMIO , assume bank 0 for now
      memIndex = deviceIntf->getAIETs2mmMemIndex(0);
    }
    Memory* memory = (db->getStaticInfo()).getMemory(deviceId, memIndex);
    if(nullptr == memory) {
      std::string msg = "Information about memory index " + std::to_string(memIndex)
                         + " not found in given xclbin. So, cannot check availability of memory resource for device trace offload.";
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      return;
    }
    uint64_t aieMemorySz = (((db->getStaticInfo()).getMemory(deviceId, memIndex))->size) * 1024;
    if(aieMemorySz > 0 && aieTraceBufSz > aieMemorySz) {
      aieTraceBufSz = aieMemorySz;
      std::string msg = "Trace buffer size is too big for memory resource.  Using " + std::to_string(aieMemorySz) + " instead." ;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
    }
#endif

    AIETraceDataLogger* aieTraceLogger = new AIETraceDataLogger(deviceId);

    AIETraceOffload* aieTraceOffloader = new AIETraceOffload(handle, deviceId,
                                              deviceIntf, aieTraceLogger,
                                              isPLIO,          // isPLIO 
                                              aieTraceBufSz,   // total trace buffer size
                                              numAIETraceOutput);  // numStream

    if(!aieTraceOffloader->initReadTrace()) {
      std::string msg = "Allocation of buffer for AIE trace failed. AIE trace will not be available.";
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      delete aieTraceOffloader;
      delete aieTraceLogger;
      return;
    }
    aieOffloaders[deviceId] = std::make_tuple(aieTraceOffloader, aieTraceLogger, deviceIntf);
  }

  void AieTracePlugin::setFlushMetrics(uint64_t deviceId, void* handle)
  {
    XAie_DevInst* aieDevInst =
      static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle)) ;
    xaiefal::XAieDev* aieDevice =
      static_cast<xaiefal::XAieDev*>(db->getStaticInfo().getAieDevice(allocateAieDevice, deallocateAieDevice, handle)) ;

    if (!aieDevInst || !aieDevice) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", 
          "Unable to get AIE device. There will be no flushing of AIE event trace.");
      return;
    }

    // New start & end events for trace control and counters
    coreTraceStartEvent = XAIE_EVENT_TIMER_SYNC_CORE;
    //coreTraceStartEvent = XAIE_EVENT_TRUE_CORE;
    coreTraceEndEvent   = XAIE_EVENT_TIMER_VALUE_REACHED_CORE;

    // Timer trigger value: 300* 1020*1020 = 312,120,000 = 0x129A92C0
    // NOTES: Each packet has 7 payload words (one word: 32bits)
    //        We need 64 packets, so we need 7 * 64 = 448 words.
    //        Existing counters generates 1.5 words for every perf counter 2 triggers.
    //        Thus, 300 (299 exactly,, but 300 would have no harm) perf 2 counter events.
    uint32_t timerTrigValueLow  = 0x129A92C0;
    uint32_t timerTrigValueHigh = 0x0;

    uint32_t prevCol = 0;
    uint32_t prevRow = 0;

    // Reconfigure profile counters
    for (int i=0; i < coreCounters.size(); ++i) {
      // 1. For every tile, stop trace & change trace start/stop and timer
      auto& tile = coreCounterTiles.at(i);
      auto  col  = tile.col;
      auto  row  = tile.row;
      if ((col != prevCol) || (row != prevRow)) {
        std::stringstream msg;
        msg << "AIE Trace Flush: Modifying control and timer for tile (" << col << "," << row << ")";
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());

        prevCol        = col;
        prevRow        = row;
        auto& core     = aieDevice->tile(col, row + 1).core();
        auto coreTrace = core.traceControl();

        coreTrace->stop();
        coreTrace->setCntrEvent(coreTraceStartEvent, coreTraceEndEvent);
      }

      // 2. For every counter, change start/stop events
      std::stringstream msg;
      msg << "AIE Trace Flush: Modifying start/stop events for counter " << i;
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
      
      auto& counter = coreCounters.at(i);
      counter->stop();
      counter->changeStartEvent(XAIE_CORE_MOD, coreTraceStartEvent);
      counter->changeStopEvent(XAIE_CORE_MOD,  coreTraceEndEvent);
      counter->start();
    }

    // 3. For every tile, restart trace and reset timer
    prevCol = 0;
    prevRow = 0;
    for (int i=0; i < coreCounters.size(); ++i) {
      auto& tile = coreCounterTiles.at(i);
      auto  col  = tile.col;
      auto  row  = tile.row;
      if ((col != prevCol) || (row != prevRow)) {
        prevCol        = col;
        prevRow        = row;
        auto& core     = aieDevice->tile(col, row + 1).core();
        auto coreTrace = core.traceControl();
        coreTrace->start();

        XAie_LocType tileLocation = XAie_TileLoc(col, row + 1);
        XAie_SetTimerTrigEventVal(aieDevInst, tileLocation, XAIE_CORE_MOD,
                                  timerTrigValueLow, timerTrigValueHigh);
        XAie_ResetTimer(aieDevInst, tileLocation, XAIE_CORE_MOD);
      }
    }
  }

  void AieTracePlugin::flushAIEDevice(void* handle)
  {
    if (handle == nullptr)
      return;

    char pathBuf[512];
    memset(pathBuf, 0, 512);
    xclGetDebugIPlayoutPath(handle, pathBuf, 512);

    std::string sysfspath(pathBuf);

    uint64_t deviceId = db->addDevice(sysfspath); // Get the unique device Id

    if(aieOffloaders.find(deviceId) != aieOffloaders.end()) {
      (std::get<0>(aieOffloaders[deviceId]))->readTrace();
    }
  }

  void AieTracePlugin::finishFlushAIEDevice(void* handle)
  {
    if (handle == nullptr)
      return;
    
    char pathBuf[512];
    memset(pathBuf, 0, 512);
    xclGetDebugIPlayoutPath(handle, pathBuf, 512);

    std::string sysfspath(pathBuf);

    uint64_t deviceId = db->addDevice(sysfspath); // Get the unique device Id

    auto itr =  deviceIdToHandle.find(deviceId);
    if ((itr == deviceIdToHandle.end()) || (itr->second != handle))
      return;

    // Set metrics to flush the trace FIFOs
    // NOTE: The data mover uses a burst length of 256, so we need 64 more 
    // dummy packets to ensure all execution trace gets written to DDR.
    if (xrt_core::config::get_aie_trace_flush()) {
      setFlushMetrics(deviceId, handle);
      std::this_thread::sleep_for(std::chrono::microseconds(10));
    }

    if(aieOffloaders.find(deviceId) != aieOffloaders.end()) {
      auto offloader = std::get<0>(aieOffloaders[deviceId]);
      auto logger    = std::get<1>(aieOffloaders[deviceId]);

      offloader->readTrace();
      if (offloader->isTraceBufferFull())
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", AIE_TS2MM_WARN_MSG_BUF_FULL);
      offloader->endReadTrace();

      delete (offloader);
      delete (logger);

      aieOffloaders.erase(deviceId);
    }
    
  }

  void AieTracePlugin::writeAll(bool openNewFiles)
  {
    // read the trace data from device and wrie to the output file
    for(auto o : aieOffloaders) {
      auto offloader = std::get<0>(o.second);
      auto logger    = std::get<1>(o.second);

      offloader->readTrace();
      if (offloader->isTraceBufferFull())
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", AIE_TS2MM_WARN_MSG_BUF_FULL);
      offloader->endReadTrace();

      delete offloader;
      delete logger;
      // don't delete DeviceIntf
    }
    aieOffloaders.clear();

    for(auto w : writers) {
      w->write(openNewFiles);
    }
  }

}

