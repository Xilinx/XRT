/* Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#include "core/edge/include/sk_types.h"
#include "core/edge/user/shim.h"
#include "event_configuration.h"
#include "xaiefal/xaiefal.hpp"
#include "xaiengine.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_trace_new/x86/aie_trace_kernel_config.h"

// User private data structure container (context object) definition
class xrtHandles : public pscontext
{
  public:
    XAie_DevInst* aieDevInst = nullptr;
    xaiefal::XAieDev* aieDev = nullptr;
    xclDeviceHandle handle = nullptr;

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

  bool checkInput(const xdp::built_in::InputConfiguration* params)
  {
    if (params == nullptr)
     return false;

    // Verify the CounterScheme and MetricSet are within expected ranges
    if (params->counterScheme < 0 || params->counterScheme > 1)
      return false;
    if (params->metricSet < 0 || params->metricSet > 3)
      return false;

    return true;
  }

  inline uint32_t bcIdToEvent(int bcId)
  {
    // Core broadcast event base defined on AIE1 as 107 in architecture
    constexpr int core_broadcast_event_base = 107;
    return static_cast<uint32_t>(bcId + core_broadcast_event_base);
  }

  // Check if a given tile on the AIE has enough free resources for the
  // requested trace configuration
  bool tileHasFreeRsc(xaiefal::XAieDev* aieDevice, XAie_LocType& loc,
                      EventConfiguration& config,
                      const xdp::built_in::InputConfiguration* params)
  {
    auto stats = aieDevice->getRscStat(XAIEDEV_DEFAULT_GROUP_AVAIL);
    uint32_t available = 0;
    uint32_t required = 0;

    // Core Module perf counters
    available = stats.getNumRsc(loc, XAIE_CORE_MOD, XAIE_PERFCNT_RSC);
    required = config.coreCounterStartEvents.size();
    if (params->useDelay)
      required += 1;
    if (available < required) {
      // TODO: Send detailed error message back to host code
      // msg << "Available core module performance counters for aie trace : " << available << std::endl
      //<< "Required core module performance counters for aie trace : "  << required;
      //xrt_core::message::send(severity_level::info, "XRT", msg.str());
      return false;
    }

    // Core Module trace slots
    available = stats.getNumRsc(loc, XAIE_CORE_MOD, xaiefal::XAIE_TRACE_EVENTS_RSC);
    required = config.coreCounterStartEvents.size() + config.coreEventsBase.size();
    if (available < required) {
      // TODO: Send detailed message back to host code
      // msg << "Available core module trace slots for aie trace : " << available << std::endl
      //     << "Required core module trace slots for aie trace : "  << required;
      //xrt_core::message::send(severity_level::info, "XRT", msg.str());
      return false;
    }

    // Core Module broadcasts. 2 events for starting/ending trace
    available = stats.getNumRsc(loc, XAIE_CORE_MOD, XAIE_BCAST_CHANNEL_RSC);
    required = config.memoryCrossEventsBase.size() + 2;
    if (available < required) {
      // TODO: Send detailed message back to host code
      // msg << "Available core module broadcast channels for aie trace : " << available << std::endl
      //     << "Required core module broadcast channels for aie trace : "  << required;
      //xrt_core::message::send(severity_level::info, "XRT", msg.str());
      return false;
    }

   // Memory Module perf counters
    available = stats.getNumRsc(loc, XAIE_MEM_MOD, XAIE_PERFCNT_RSC);
    required = config.memoryCounterStartEvents.size();
    if (available < required) {
      // TODO: Send detailed message back to host code
      // msg << "Available memory module performance counters for aie trace : " << available << std::endl
      //     << "Required memory module performance counters for aie trace : "  << required;
      //xrt_core::message::send(severity_level::info, "XRT", msg.str());
      return false;
    }

    // Memory Module trace slots
    available = stats.getNumRsc(loc, XAIE_MEM_MOD, xaiefal::XAIE_TRACE_EVENTS_RSC);
    required = config.memoryCounterStartEvents.size() + config.memoryCrossEventsBase.size();
    if (available < required) {
      // TODO: Send detailed message back to host code
      //msg << "Available memory module trace slots for aie trace : " << available << std::endl
      //    << "Required memory module trace slots for aie trace : "  << required;
      //xrt_core::message::send(severity_level::info, "XRT", msg.str());
      return false;
    }

    // No need to check memory module broadcast

    return true;
  }

  void releaseCurrentTileCounters(int numCoreCounters, int numMemoryCounters,
                                  EventConfiguration& config)
  {
    for (int i=0; i < numCoreCounters; i++) {
      config.mCoreCounters.back()->stop();
      config.mCoreCounters.back()->release();
      config.mCoreCounters.pop_back();
    }

    for (int i=0; i < numMemoryCounters; i++) {
      config.mMemoryCounters.back()->stop();
      config.mMemoryCounters.back()->release();
      config.mMemoryCounters.pop_back();
    }
  }

  int setMetrics(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice,
                 EventConfiguration& config,
                 const xdp::built_in::InputConfiguration* params,
                 xdp::built_in::OutputConfiguration* tilecfg)
  {
    xaiefal::Logger::get().setLogLevel(xaiefal::LogLevel::DEBUG);
    int numTileCoreTraceEvents[params->NUM_CORE_TRACE_EVENTS+1] = {0};
    int numTileMemoryTraceEvents[params->NUM_MEMORY_TRACE_EVENTS+1] = {0};

    //Parse tile data
    uint16_t tile_rows[params->numTiles];
    uint16_t tile_cols[params->numTiles];
    
    for(int i = 0; i < params->numTiles*2; i += 2) {
        tile_rows[i/2] = params->tiles[i];
        tile_cols[i/2] = params->tiles[i+1];
    }
    
    // Iterate over all used/specified tiles
    for (int tile_idx; tile_idx < params->numTiles; tile_idx++) {
      auto  col    = tile_cols[tile_idx];
      auto  row    = tile_rows[tile_idx];
      // NOTE: resource manager requires absolute row number
      auto& core   = aieDevice->tile(col, row + 1).core();
      auto& memory = aieDevice->tile(col, row + 1).mem();
      auto loc = XAie_TileLoc(col, row + 1);

      // AIE config object for this tile
      //auto cfgTile  = std::make_unique<xdp::aie_cfg_tile>(col, row + 1);
      auto cfgTile = xdp::built_in::TileData(col, row+1);

      // Get vector of pre-defined metrics for this set
      // NOTE: these are local copies as we are adding tile/counter-specific events
      std::vector<XAie_Events> coreEvents = config.coreEventsBase;
      std::vector<XAie_Events> memoryCrossEvents = config.memoryCrossEventsBase;
      std::vector<XAie_Events> memoryEvents;

      // Check Resource Availability
      // For now only counters are checked
      if (!tileHasFreeRsc(aieDevice, loc, config, params)) {
        // TODO: Send detailed message back to host
        //xrt_core::message::send(severity_level::warning, "XRT", "Tile doesn't have enough free resources for trace. Aborting trace configuration.");
        //printTileStats(aieDevice, tile);
        return 1;
      }

      //
      // 1. Reserve and start core module counters (as needed)
      //
      int numCoreCounters = 0;
      {
        XAie_ModuleType mod = XAIE_CORE_MOD;

        for (int i=0; i < config.coreCounterStartEvents.size(); ++i) {
          auto perfCounter = core.perfCounter();
          if (perfCounter->initialize(mod, config.coreCounterStartEvents.at(i),
                                      mod, config.coreCounterEndEvents.at(i)) != XAIE_OK)
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

          auto test = perfCounter->start();
          if (test != XAIE_OK)
             break;
          config.mCoreCounters.push_back(perfCounter);
          numCoreCounters++;

          // Update config file
          uint8_t phyEvent = 0;
          auto& cfg = cfgTile.core_trace_config.pc[idx];
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, config.coreCounterStartEvents[i], &phyEvent);
          cfg.start_event = phyEvent;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, config.coreCounterEndEvents[i], &phyEvent);
          cfg.stop_event = phyEvent;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, counterEvent, &phyEvent);
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

        for (int i=0; i < config.memoryCounterStartEvents.size(); ++i) {
          auto perfCounter = memory.perfCounter();
          if (perfCounter->initialize(mod, config.memoryCounterStartEvents.at(i),
                                      mod, config.memoryCounterEndEvents.at(i)) != XAIE_OK) 
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
          uint8_t phyEvent = 0;
          auto& cfg = cfgTile.memory_trace_config.pc[idx];
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, config.memoryCounterStartEvents[i], &phyEvent);
          cfg.start_event = phyEvent;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, config.memoryCounterEndEvents[i], &phyEvent);
          cfg.stop_event = phyEvent;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, counterEvent, &phyEvent);
          cfg.reset_event = phyEvent;
          cfg.event_value = config.memoryCounterEventValues[i];
        }
      }

      // Catch when counters cannot be reserved: report, release, and return
      if ((numCoreCounters < config.coreCounterStartEvents.size())
          || (numMemoryCounters < config.memoryCounterStartEvents.size())) {
        // TODO: Send detailed error message back to host
        //std::stringstream msg;
        //msg << "Unable to reserve " << config.coreCounterStartEvents.size() << " core counters"
        //    << " and " << config.memoryCounterStartEvents.size() << " memory counters"
        //    << " for AIE tile (" << col << "," << row + 1 << ") required for trace.";
        // xrt_core::message::send(severity_level::warning, "XRT", msg.str());

        releaseCurrentTileCounters(numCoreCounters, numMemoryCounters, config);
        // Print resources availability for this tile
        // printTileStats(aieDevice, tile);
        return 1;
      }

      //
      // 3. Configure Core Tracing Events
      //
      {
        XAie_ModuleType mod = XAIE_CORE_MOD;
        uint8_t phyEvent = 0;
        auto coreTrace = core.traceControl();

        // Delay cycles and user control are not compatible with each other
        if (params->userControl) {
          config.coreTraceStartEvent = XAIE_EVENT_INSTR_EVENT_0_CORE;
          config.coreTraceEndEvent = XAIE_EVENT_INSTR_EVENT_1_CORE;
        } else if (params->useDelay) {
          auto perfCounter = core.perfCounter();
          if (perfCounter->initialize(mod, XAIE_EVENT_ACTIVE_CORE,
                                      mod, XAIE_EVENT_DISABLED_CORE) != XAIE_OK) 
            break;
          if (perfCounter->reserve() != XAIE_OK) 
            break;

          perfCounter->changeThreshold(params->delayCycles);
          XAie_Events counterEvent;
          perfCounter->getCounterEvent(mod, counterEvent);

          // Set reset and trace start using this counter
          perfCounter->changeRstEvent(mod, counterEvent);
          config.coreTraceStartEvent = counterEvent;
          // This is needed because the cores are started/stopped during execution
          // to get around some hw bugs. We cannot restart tracemodules when that happens
          config.coreTraceEndEvent = XAIE_EVENT_NONE_CORE;

          if (perfCounter->start() != XAIE_OK) 
            break;
        }

        // Set overall start/end for trace capture
        // Wendy said this should be done first

        if (coreTrace->setCntrEvent(config.coreTraceStartEvent, config.coreTraceEndEvent) != XAIE_OK) 
          break;

        auto ret = coreTrace->reserve();
        if (ret != XAIE_OK) {
          // TODO: Send detailed error message back to the host
          //std::stringstream msg;
          //msg << "Unable to reserve core module trace control for AIE tile (" 
          //    << col << "," << row + 1 << ").";
          //xrt_core::message::send(severity_level::warning, "XRT", msg.str());

          releaseCurrentTileCounters(numCoreCounters, numMemoryCounters, config);
          // Print resources availability for this tile
          //printTileStats(aieDevice, tile);
          return 1;
        }

        int numTraceEvents = 0;
        for (int i=0; i < coreEvents.size(); i++) {
          uint8_t slot;
          if (coreTrace->reserveTraceSlot(slot) != XAIE_OK) 
            break;
          if (coreTrace->setTraceEvent(slot, coreEvents[i]) != XAIE_OK) 
            break;
          numTraceEvents++;

          // Update config file
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, coreEvents[i], &phyEvent);
          cfgTile.core_trace_config.traced_events[slot] = phyEvent;
        }
        // Update config file
        XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, config.coreTraceStartEvent, &phyEvent);
        cfgTile.core_trace_config.start_event = phyEvent;
        XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, config.coreTraceEndEvent, &phyEvent);
        cfgTile.core_trace_config.stop_event = phyEvent;
        
        coreEvents.clear();
        numTileCoreTraceEvents[numTraceEvents]++;

        // TODO: Send success message back to the host
        //std::stringstream msg;
        //msg << "Reserved " << numTraceEvents << " core trace events for AIE tile (" << col << "," << row << ").";
        // xrt_core::message::send(severity_level::debug, "XRT", msg.str());

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
          // TODO: Send detailed error message to host
          //std::stringstream msg;
          //msg << "Unable to reserve memory module trace control for AIE tile (" 
          //    << col << "," << row + 1 << ").";
          // xrt_core::message::send(severity_level::warning, "XRT", msg.str());

          releaseCurrentTileCounters(numCoreCounters, numMemoryCounters, config);
          // Print resources availability for this tile
          // printTileStats(aieDevice, tile);
          return 1;
        }

        int numTraceEvents = 0;
        
        // Configure cross module events
        for (int i=0; i < memoryCrossEvents.size(); i++) {
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
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, memoryCrossEvents[i], &phyEvent);
          cfgTile.core_trace_config.internal_events_broadcast[bcId] = phyEvent;
        }

        // Configure same module events
        for (int i=0; i < memoryEvents.size(); i++) {
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
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, memoryEvents[i], &phyEvent);
          cfgTile.memory_trace_config.traced_events[S] = phyEvent;
        }

        // Update config file
        {
          // Add Memory module trace control events
          uint32_t bcBit = 0x1;
          auto bcId = memoryTrace->getStartBc();
          coreToMemBcMask |= (bcBit << bcId);
          auto mod = XAIE_CORE_MOD;
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, config.coreTraceStartEvent, &phyEvent);
          cfgTile.memory_trace_config.start_event = bcIdToEvent(bcId);
          cfgTile.core_trace_config.internal_events_broadcast[bcId] = phyEvent;

          bcBit = 0x1;
          bcId = memoryTrace->getStopBc();
          coreToMemBcMask |= (bcBit << bcId);
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, config.coreTraceEndEvent, &phyEvent);
          cfgTile.memory_trace_config.stop_event = bcIdToEvent(bcId);
          cfgTile.core_trace_config.internal_events_broadcast[bcId] = phyEvent;
        }

        // Odd absolute rows change east mask end even row change west mask
        if ((row + 1) % 2) {
          cfgTile.core_trace_config.broadcast_mask_east = coreToMemBcMask;
        } else {
          cfgTile.core_trace_config.broadcast_mask_west = coreToMemBcMask;
        }
        // Done update config file

        memoryEvents.clear();
        numTileMemoryTraceEvents[numTraceEvents]++;

        // TODO: Send success message back to host
        //std::stringstream msg;
        //msg << "Reserved " << numTraceEvents << " memory trace events for AIE tile (" << col << "," << row << ").";
        // xrt_core::message::send(severity_level::debug, "XRT", msg.str());

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

      // TODO: Send success message back to host
      //std::stringstream msg;
      //msg << "Adding tile (" << col << "," << row << ") to static database";
      // xrt_core::message::send(severity_level::debug, "XRT", msg.str());

      // Add config info to static database
      // NOTE: Do not access cfgTile after this
  //    (db->getStaticInfo()).addAIECfgTile(deviceId, cfgTile);
      tilecfg->tiles[tile_idx] = cfgTile;
    } // For tiles

     // Report trace events reserved per tile
    {
      for (int n=0; n <= params->NUM_CORE_TRACE_EVENTS; ++n) {
        if (numTileCoreTraceEvents[n] == 0) continue;
        if (n != params->NUM_CORE_TRACE_EVENTS)
           tilecfg->numTileCoreTraceEvents[n] = numTileCoreTraceEvents[n];
      }
      //xrt_core::message::send(severity_level::info, "XRT", msg.str());
    }
    {
      for (int n=0; n <= params->NUM_MEMORY_TRACE_EVENTS; ++n) {
        if (numTileMemoryTraceEvents[n] == 0) continue;
        if (n != params->NUM_MEMORY_TRACE_EVENTS)
            tilecfg->numTileMemoryTraceEvents[n] = numTileMemoryTraceEvents[n];
      }
      //xrt_core::message::send(severity_level::info, "XRT", msg.str());
    }
    return 0;
  }

} // end anonymous namespace

#ifdef __cplusplus
extern "C" {
#endif

// The PS kernel initialization function
__attribute__((visibility("default")))
xrtHandles* aie_trace_config_init (xclDeviceHandle handle, const xuid_t xclbin_uuid) {

    xrtHandles* constructs = new xrtHandles;
    if (!constructs)
        return nullptr;
   
    constructs->handle = handle; 
    return constructs;
}

// The main PS kernel functionality
__attribute__((visibility("default")))
int aie_trace_config(uint8_t* input, uint8_t* output, xrtHandles* constructs)
{
  if (constructs == nullptr)
    return 0;

  auto drv = ZYNQ::shim::handleCheck(constructs->handle);
  if(!drv)
    return 0;

  auto aieArray = drv->getAieArray();
  if (!aieArray)
    return 0;

  constructs->aieDevInst = aieArray->getDevInst();
  if (!constructs->aieDevInst)
    return 0;

  if (constructs->aieDev == nullptr)
    constructs->aieDev = new xaiefal::XAieDev(constructs->aieDevInst, false);

  xdp::built_in::InputConfiguration* params =
    reinterpret_cast<xdp::built_in::InputConfiguration*>(input);

  if (!checkInput(params))
    return 0;

  EventConfiguration config;
  config.initialize(params);

  // Using malloc/free instead of new/delete because the struct treats the
  // last element as a variable sized array
  std::size_t total_size = sizeof(xdp::built_in::OutputConfiguration) + sizeof(xdp::built_in::TileData[params->numTiles - 1]);
  xdp::built_in::OutputConfiguration* tilecfg =
    (xdp::built_in::OutputConfiguration*)malloc(total_size);

  tilecfg->numTiles = params->numTiles;

  int success = setMetrics(constructs->aieDevInst, constructs->aieDev,
                           config, params, tilecfg);
  uint8_t* out = reinterpret_cast<uint8_t*>(tilecfg);
  std::memcpy(output, out, total_size);   

  free(tilecfg); 
  return 0;
}

// The final function for the PS kernel
__attribute__((visibility("default")))
int aie_trace_config_fini(xrtHandles* handles)
{
  if (handles != nullptr)
    delete handles;
  return 0;
}

#ifdef __cplusplus
}
#endif


