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

#define XDP_PLUGIN_SOURCE

#include "xdp/profile/plugin/aie_trace/edge/aie_trace.h"
#include "xdp/profile/plugin/aie_trace/util/aie_trace_util.h"
#include "xdp/profile/plugin/aie_trace/util/aie_trace_config.h"
#include "xdp/profile/database/static_info/aie_util.h"

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <regex>

#include "core/common/message.h"
#include "core/common/time.h"
#include "core/common/xrt_profiling.h"
#include "core/edge/user/shim.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_metadata.h"
#include "xdp/profile/plugin/vp_base/utility.h"

namespace {
  static void* fetchAieDevInst(void* devHandle)
  {
    auto drv = ZYNQ::shim::handleCheck(devHandle);
    if (!drv)
      return nullptr;
    auto aieArray = drv->getAieArray();
    if (!aieArray)
      return nullptr;
    return aieArray->getDevInst();
  }

  static void* allocateAieDevice(void* devHandle)
  {
    auto aieDevInst = static_cast<XAie_DevInst*>(fetchAieDevInst(devHandle));
    if (!aieDevInst)
      return nullptr;
    return new xaiefal::XAieDev(aieDevInst, false);
  }

  static void deallocateAieDevice(void* aieDevice)
  {
    auto object = static_cast<xaiefal::XAieDev*>(aieDevice);
    if (object != nullptr)
      delete object;
  }
}  // end anonymous namespace

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

  /****************************************************************************
   * Constructor: AIE trace implementation for edge devices
   ***************************************************************************/
  AieTrace_EdgeImpl::AieTrace_EdgeImpl(VPDatabase* database, std::shared_ptr<AieTraceMetadata> metadata)
      : AieTraceImpl(database, metadata)
  {
    auto hwGen = metadata->getHardwareGen();
    auto counterScheme = metadata->getCounterScheme();

    // Pre-defined metric sets
    mCoreEventSets = aie::trace::getCoreEventSets(hwGen);
    mMemoryEventSets = aie::trace::getMemoryEventSets(hwGen);
    mMemoryTileEventSets = aie::trace::getMemoryTileEventSets(hwGen);
    mInterfaceTileEventSets = aie::trace::getInterfaceTileEventSets(hwGen);

    // Core/memory module counters
    mCoreCounterStartEvents = aie::trace::getCoreCounterStartEvents(hwGen, counterScheme);
    mCoreCounterEndEvents = aie::trace::getCoreCounterEndEvents(hwGen, counterScheme);
    mCoreCounterEventValues = aie::trace::getCoreCounterEventValues(hwGen, counterScheme);
    mMemoryCounterStartEvents = aie::trace::getMemoryCounterStartEvents(hwGen, counterScheme);
    mMemoryCounterEndEvents = aie::trace::getMemoryCounterEndEvents(hwGen, counterScheme);
    mMemoryCounterEventValues = aie::trace::getMemoryCounterEventValues(hwGen, counterScheme);

    // Core trace start/end: these are also broadcast to memory module
    mCoreTraceStartEvent = XAIE_EVENT_ACTIVE_CORE;
    mCoreTraceEndEvent = XAIE_EVENT_DISABLED_CORE;

    // Memory/interface tile trace is flushed at end of run
    mMemoryTileTraceStartEvent = XAIE_EVENT_TRUE_MEM_TILE;
    mMemoryTileTraceEndEvent = XAIE_EVENT_USER_EVENT_1_MEM_TILE;
    mInterfaceTileTraceStartEvent = XAIE_EVENT_TRUE_PL;
    mInterfaceTileTraceEndEvent = XAIE_EVENT_USER_EVENT_1_PL;
  }

  /****************************************************************************
   * Verify correctness of trace buffer size
   ***************************************************************************/
  uint64_t AieTrace_EdgeImpl::checkTraceBufSize(uint64_t aieTraceBufSize)
  {
    uint64_t deviceMemorySize = getPSMemorySize();
    if (deviceMemorySize == 0)
      return aieTraceBufSize;

    double percentSize = (100.0 * aieTraceBufSize) / deviceMemorySize;

    std::stringstream percentSizeStr;
    percentSizeStr << std::fixed << std::setprecision(3) << percentSize;

    // Limit size of trace buffer if requested amount is too high
    if (percentSize >= 80.0) {
      aieTraceBufSize = static_cast<uint64_t>(std::ceil(0.8 * deviceMemorySize));

      std::stringstream newBufSizeStr;
      newBufSizeStr << std::fixed << std::setprecision(3) << (aieTraceBufSize / (1024.0 * 1024.0));  // In MB

      std::string msg = "Requested AIE trace buffer is " + percentSizeStr.str() + "% of device memory." +
                        " You may run into errors depending upon memory usage"
                        " of your application." +
                        " Limiting to " + newBufSizeStr.str() + " MB.";
      xrt_core::message::send(severity_level::warning, "XRT", msg);
    } else {
      std::string msg = "Requested AIE trace buffer is " + percentSizeStr.str() + "% of device memory.";
      xrt_core::message::send(severity_level::info, "XRT", msg);
    }

    return aieTraceBufSize;
  }

  /****************************************************************************
   * Check if given tile has free resources
   ***************************************************************************/
  bool AieTrace_EdgeImpl::tileHasFreeRsc(xaiefal::XAieDev* aieDevice, XAie_LocType& loc, 
                                         const module_type type, const std::string& metricSet)
  {
    auto stats = aieDevice->getRscStat(XAIEDEV_DEFAULT_GROUP_AVAIL);
    uint32_t available = 0;
    uint32_t required = 0;
    std::stringstream msg;

    // Check trace events for interface tiles
    if (type == module_type::shim) {
      available = stats.getNumRsc(loc, XAIE_PL_MOD, xaiefal::XAIE_TRACE_EVENTS_RSC);
      required = mInterfaceTileEventSets[metricSet].size();
      if (available < required) {
        msg << "Available interface tile trace slots for AIE trace : " << available << std::endl
            << "Required interface tile trace slots for AIE trace  : " << required;
        xrt_core::message::send(severity_level::info, "XRT", msg.str());
        return false;
      }
      return true;
    }

    // Memory module/tile perf counters
    available = stats.getNumRsc(loc, XAIE_MEM_MOD, XAIE_PERFCNT_RSC);
    required = mMemoryCounterStartEvents.size();
    if (available < required) {
      msg << "Available memory performance counters for AIE trace : " << available << std::endl
          << "Required memory performance counters for AIE trace  : " << required;
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
      return false;
    }

    // Memory module/tile trace slots
    available = stats.getNumRsc(loc, XAIE_MEM_MOD, xaiefal::XAIE_TRACE_EVENTS_RSC);
    required = mMemoryCounterStartEvents.size() + mMemoryEventSets[metricSet].size();
    if (available < required) {
      msg << "Available memory trace slots for AIE trace : " << available << std::endl
          << "Required memory trace slots for AIE trace  : " << required;
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
      return false;
    }

    // Core resources not needed in memory tiles
    if (type == module_type::mem_tile)
      return true;

    // Core module perf counters
    available = stats.getNumRsc(loc, XAIE_CORE_MOD, XAIE_PERFCNT_RSC);
    required = mCoreCounterStartEvents.size();
    if (metadata->getUseDelay()) {
      ++required;
      if (!metadata->getUseOneDelayCounter())
        ++required;
    } else if (metadata->getUseGraphIterator())
      ++required;

    if (available < required) {
      msg << "Available core module performance counters for AIE trace : " << available << std::endl
          << "Required core module performance counters for AIE trace  : " << required;
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
      return false;
    }

    // Core module trace slots
    available = stats.getNumRsc(loc, XAIE_CORE_MOD, xaiefal::XAIE_TRACE_EVENTS_RSC);
    required = mCoreCounterStartEvents.size() + mCoreEventSets[metricSet].size();
    if (available < required) {
      msg << "Available core module trace slots for AIE trace : " << available << std::endl
          << "Required core module trace slots for AIE trace  : " << required;
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
      return false;
    }

    // Core module broadcasts. 2 events for starting/ending trace
    available = stats.getNumRsc(loc, XAIE_CORE_MOD, XAIE_BCAST_CHANNEL_RSC);
    required = mMemoryEventSets[metricSet].size() + 2;
    if (available < required) {
      msg << "Available core module broadcast channels for AIE trace : " << available << std::endl
          << "Required core module broadcast channels for AIE trace  : " << required;
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
      return false;
    }

    return true;
  }

  /****************************************************************************
   * Stop and release resources (e.g., counters, ports)
   ***************************************************************************/
  void AieTrace_EdgeImpl::freeResources()
  {
    for (auto& c : mPerfCounters) {
      c->stop();
      c->release();
    }
    for (auto& p : mStreamPorts) {
      p->stop();
      p->release();
    }
  }

  /****************************************************************************
   * Validitate AIE device and runtime metrics
   ***************************************************************************/
  bool AieTrace_EdgeImpl::checkAieDeviceAndRuntimeMetrics(uint64_t deviceId, void* handle)
  {
    aieDevInst = static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle));
    aieDevice = static_cast<xaiefal::XAieDev*>(db->getStaticInfo().getAieDevice(allocateAieDevice, deallocateAieDevice, handle));
    if (!aieDevInst || !aieDevice) {
      xrt_core::message::send(severity_level::warning, "XRT",
          "Unable to get AIE device. AIE event trace will not be available.");
      return false;
    }

    // Check compile-time trace
    if (!metadata->getRuntimeMetrics()) {
      return false;
    }

    return true;
  }

  /****************************************************************************
   * Update device (e.g., after loading xclbin)
   ***************************************************************************/
  void AieTrace_EdgeImpl::updateDevice()
  {
    if (!checkAieDeviceAndRuntimeMetrics(metadata->getDeviceID(), metadata->getHandle()))
      return;

    // Set metrics for counters and trace events
    if (!setMetricsSettings(metadata->getDeviceID(), metadata->getHandle())) {
      std::string msg("Unable to configure AIE trace control and events. No trace will be generated.");
      xrt_core::message::send(severity_level::warning, "XRT", msg);
      return;
    }
  }

  /****************************************************************************
   * Configure requested tiles with trace metrics and settings
   ***************************************************************************/
  bool AieTrace_EdgeImpl::setMetricsSettings(uint64_t deviceId, void* handle)
  {
    if (!metadata->getIsValidMetrics()) {
      std::string msg("AIE trace metrics were not specified in xrt.ini. AIE event trace will not be available.");
      xrt_core::message::send(severity_level::warning, "XRT", msg);
      return false;
    }

    // Get channel configurations (memory and interface tiles)
    auto configChannel0 = metadata->getConfigChannel0();
    auto configChannel1 = metadata->getConfigChannel1();

    // Zero trace event tile counts
    for (int m = 0; m < static_cast<int>(module_type::num_types); ++m) {
      for (int n = 0; n <= NUM_TRACE_EVENTS; ++n)
        mNumTileTraceEvents[m][n] = 0;
    }

    // Decide when to use user event for trace end to enable flushing
    // NOTE: This is needed to "flush" the last trace packet.
    //       We use the event generate register to create this 
    //       event and gracefully shut down trace modules.
    bool useTraceFlush = false;
    if ((metadata->getUseUserControl()) || (metadata->getUseGraphIterator()) || (metadata->getUseDelay()) ||
        (xrt_core::config::get_aie_trace_settings_end_type() == "event1")) {
      if (metadata->getUseUserControl())
        mCoreTraceStartEvent = XAIE_EVENT_INSTR_EVENT_0_CORE;
      mCoreTraceEndEvent = XAIE_EVENT_INSTR_EVENT_1_CORE;
      useTraceFlush = true;
      xrt_core::message::send(severity_level::info, "XRT", "Enabling trace flush");
    }

    // Iterate over all used/specified tiles
    // NOTE: rows are stored as absolute as required by resource manager
    for (auto& tileMetric : metadata->getConfigMetrics()) {
      auto& metricSet = tileMetric.second;
      auto tile       = tileMetric.first;
      auto col        = tile.col;
      auto row        = tile.row;
      auto subtype    = tile.subtype;
      auto type       = aie::getModuleType(row, metadata->getRowOffset());
      auto typeInt    = static_cast<int>(type);
      auto& xaieTile  = aieDevice->tile(col, row);
      auto loc        = XAie_TileLoc(col, row);
      
      std::string tileName = (type == module_type::mem_tile) ? "memory" 
                           : ((type == module_type::shim) ? "interface" : "AIE");
      tileName.append(" tile (" + std::to_string(col) + "," + std::to_string(row) + ")");

      xaiefal::XAieMod core;
      xaiefal::XAieMod memory;
      xaiefal::XAieMod shim;
      if (type == module_type::core)
        core = xaieTile.core();
      if (type == module_type::shim)
        shim = xaieTile.pl();
      else
        memory = xaieTile.mem();

      // Store location to flush at end of run
      if (useTraceFlush || (type == module_type::mem_tile) 
          || (type == module_type::shim)) {
        if (type == module_type::core)
          mTraceFlushLocs.push_back(loc);
        else if (type == module_type::mem_tile)
          mMemoryTileTraceFlushLocs.push_back(loc);
        else if (type == module_type::shim)
          mInterfaceTileTraceFlushLocs.push_back(loc);
      }

      // AIE config object for this tile
      auto cfgTile = std::make_unique<aie_cfg_tile>(col, row, type);
      cfgTile->type = type;
      cfgTile->trace_metric_set = metricSet;

      // Get vector of pre-defined metrics for this set
      // NOTE: these are local copies as we are adding tile/counter-specific events
      EventVector coreEvents;
      EventVector memoryEvents;
      EventVector interfaceEvents;
      if (type == module_type::core) {
        coreEvents = mCoreEventSets[metricSet];
        memoryEvents = mMemoryEventSets[metricSet];
      }
      else if (type == module_type::mem_tile) {
        memoryEvents = mMemoryTileEventSets[metricSet];
      }
      else if (type == module_type::shim) {
        interfaceEvents = mInterfaceTileEventSets[metricSet];
      }

      if (aie::isInfoVerbosity()) {
        std::stringstream infoMsg;
        infoMsg << "Configuring " << tileName << " for trace using metric set " << metricSet;
        xrt_core::message::send(severity_level::info, "XRT", infoMsg.str());
      }

      // Check Resource Availability
      if (!tileHasFreeRsc(aieDevice, loc, type, metricSet)) {
        xrt_core::message::send(severity_level::warning, "XRT",
            "Tile doesn't have enough free resources for trace. Aborting trace configuration.");
        aie::trace::printTileStats(aieDevice, tile);
        return false;
      }

      int numCoreCounters = 0;
      int numMemoryCounters = 0;
      int numCoreTraceEvents = 0;
      int numMemoryTraceEvents = 0;
      int numInterfaceTraceEvents = 0;

      //
      // 1. Reserve and start core module counters (as needed)
      //
      if ((type == module_type::core) && (mCoreCounterStartEvents.size() > 0)) {
        if (aie::isDebugVerbosity()) {
          std::stringstream msg;
          msg << "Reserving " << mCoreCounterStartEvents.size() 
              << " core counters for " << tileName;
          xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        }

        XAie_ModuleType mod = XAIE_CORE_MOD;

        for (int i = 0; i < mCoreCounterStartEvents.size(); ++i) {
          auto perfCounter = core.perfCounter();
          if (perfCounter->initialize(mod, mCoreCounterStartEvents.at(i), mod, mCoreCounterEndEvents.at(i)) != XAIE_OK)
            break;
          if (perfCounter->reserve() != XAIE_OK)
            break;

          // NOTE: store events for later use in trace
          XAie_Events counterEvent;
          perfCounter->getCounterEvent(mod, counterEvent);
          int idx = static_cast<int>(counterEvent) - static_cast<int>(XAIE_EVENT_PERF_CNT_0_CORE);
          perfCounter->changeThreshold(mCoreCounterEventValues.at(i));

          // Set reset event based on counter number
          perfCounter->changeRstEvent(mod, counterEvent);
          coreEvents.push_back(counterEvent);

          // If no memory counters are used, then we need to broadcast the core
          // counter
          if (mMemoryCounterStartEvents.empty())
            memoryEvents.push_back(counterEvent);

          if (perfCounter->start() != XAIE_OK)
            break;

          mPerfCounters.push_back(perfCounter);
          numCoreCounters++;

          // Update config file
          uint8_t phyEvent = 0;
          auto& cfg = cfgTile->core_trace_config.pc[idx];
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, mCoreCounterStartEvents[i], &phyEvent);
          cfg.start_event = phyEvent;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, mCoreCounterStartEvents[i], &phyEvent);
          cfg.stop_event = phyEvent;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, counterEvent, &phyEvent);
          cfg.reset_event = phyEvent;
          cfg.event_value = mCoreCounterEventValues[i];
        }
      }

      //
      // 2. Reserve and start memory module counters (as needed)
      //
      if ((type == module_type::core) && (mMemoryCounterStartEvents.size() > 0)) {
        if (aie::isDebugVerbosity()) {
          std::stringstream msg;
          msg << "Reserving " << mMemoryCounterStartEvents.size() 
              << " memory counters for " << tileName;
          xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        }

        XAie_ModuleType mod = XAIE_MEM_MOD;

        for (int i = 0; i < mMemoryCounterStartEvents.size(); ++i) {
          auto perfCounter = memory.perfCounter();
          if (perfCounter->initialize(mod, mMemoryCounterStartEvents.at(i), mod, mMemoryCounterEndEvents.at(i)) !=
              XAIE_OK)
            break;
          if (perfCounter->reserve() != XAIE_OK)
            break;

          // Set reset event based on counter number
          XAie_Events counterEvent;
          perfCounter->getCounterEvent(mod, counterEvent);
          int idx = static_cast<int>(counterEvent) - static_cast<int>(XAIE_EVENT_PERF_CNT_0_MEM);
          perfCounter->changeThreshold(mMemoryCounterEventValues.at(i));

          perfCounter->changeRstEvent(mod, counterEvent);
          memoryEvents.push_back(counterEvent);

          if (perfCounter->start() != XAIE_OK)
            break;

          mPerfCounters.push_back(perfCounter);
          numMemoryCounters++;

          // Update config file
          uint8_t phyEvent = 0;
          auto& cfg = cfgTile->memory_trace_config.pc[idx];
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, mMemoryCounterStartEvents[i], &phyEvent);
          cfg.start_event = phyEvent;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, mMemoryCounterEndEvents[i], &phyEvent);
          cfg.stop_event = phyEvent;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, counterEvent, &phyEvent);
          cfg.reset_event = phyEvent;
          cfg.event_value = mMemoryCounterEventValues[i];
        }

        // Catch when counters cannot be reserved: report, release, and return
        if ((numCoreCounters < mCoreCounterStartEvents.size()) ||
            (numMemoryCounters < mMemoryCounterStartEvents.size())) {
          std::stringstream msg;
          msg << "Unable to reserve " << mCoreCounterStartEvents.size() 
              << " core counters and " << mMemoryCounterStartEvents.size() 
              << " memory counters for " << tileName << " required for trace.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());

          freeResources();
          // Print resources availability for this tile
          aie::trace::printTileStats(aieDevice, tile);
          return false;
        }
      }

      //
      // 3. Configure Core Tracing Events
      //
      if (type == module_type::core) {
        if (aie::isDebugVerbosity()) {
          std::stringstream msg;
          msg << "Reserving " << coreEvents.size() << " core trace events for " << tileName;
          xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        }

        XAie_ModuleType mod = XAIE_CORE_MOD;
        uint8_t phyEvent = 0;
        auto coreTrace = core.traceControl();

        // Delay cycles and user control are not compatible with each other
        if (metadata->getUseGraphIterator()) {
          if (!aie::trace::configStartIteration(core, metadata->getIterationCount(), 
                                                mCoreTraceStartEvent))
            break;
        } else if (metadata->getUseDelay()) {
          if (!aie::trace::configStartDelay(core, metadata->getDelay(), 
                                            mCoreTraceStartEvent))
            break;
        }

        // Configure combo & group events (e.g., to monitor DMA channels)
        auto comboEvents = aie::trace::configComboEvents(aieDevInst, xaieTile, loc, mod, type, 
                                                         metricSet, cfgTile->core_trace_config);
        aie::trace::configGroupEvents(aieDevInst, loc, mod, type, metricSet);

        // Set overall start/end for trace capture
        if (coreTrace->setCntrEvent(mCoreTraceStartEvent, mCoreTraceEndEvent) != XAIE_OK)
          break;

        auto ret = coreTrace->reserve();
        if (ret != XAIE_OK) {
          std::stringstream msg;
          msg << "Unable to reserve core module trace control for " << tileName;
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());

          freeResources();
          // Print resources availability for this tile
          aie::trace::printTileStats(aieDevice, tile);
          return false;
        }

        for (int i = 0; i < coreEvents.size(); i++) {
          uint8_t slot;
          if (coreTrace->reserveTraceSlot(slot) != XAIE_OK)
            break;
          if (coreTrace->setTraceEvent(slot, coreEvents[i]) != XAIE_OK)
            break;
          numCoreTraceEvents++;

          // Update config file
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, coreEvents[i], &phyEvent);
          cfgTile->core_trace_config.traced_events[slot] = phyEvent;
        }

        // Update config file
        XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, mCoreTraceStartEvent, &phyEvent);
        cfgTile->core_trace_config.start_event = phyEvent;
        XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, mCoreTraceEndEvent, &phyEvent);
        cfgTile->core_trace_config.stop_event = phyEvent;

        // Record allocated trace events
        mNumTileTraceEvents[typeInt][numCoreTraceEvents]++;
        coreEvents.clear();

        // Specify packet type and ID then start core trace
        // NOTE: always use PC packets
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
      // NOTE: this is applicable for memory modules in AIE tiles or memory tiles
      uint32_t coreToMemBcMask = 0;
      if ((type == module_type::core) || (type == module_type::mem_tile)) {
        if (aie::isDebugVerbosity()) {
          xrt_core::message::send(severity_level::debug, "XRT", "Reserving " +
            std::to_string(memoryEvents.size()) + " memory trace events for " + tileName);
        }

        // Set overall start/end for trace capture
        // NOTE: this should be done first for FAL-based implementations
        auto memoryTrace = memory.traceControl();
        auto traceStartEvent = (type == module_type::core) ? mCoreTraceStartEvent : mMemoryTileTraceStartEvent;
        auto traceEndEvent = (type == module_type::core) ? mCoreTraceEndEvent : mMemoryTileTraceEndEvent;
        
        aie_cfg_base& aieConfig = cfgTile->core_trace_config;
        if (type == module_type::mem_tile)
          aieConfig = cfgTile->memory_tile_trace_config;

        // Configure combo events for metric sets that include DMA events        
        auto comboEvents = aie::trace::configComboEvents(aieDevInst, xaieTile, loc, 
            XAIE_CORE_MOD, module_type::dma, metricSet, aieConfig);
        if (comboEvents.size() == 2) {
          traceStartEvent = comboEvents.at(0);
          traceEndEvent = comboEvents.at(1);
        }

        // Configure event ports on stream switch
        // NOTE: These are events from the core module stream switch
        //       outputted on the memory module trace stream. 
        auto streamPorts = aie::trace::configStreamSwitchPorts(aieDevInst, tile,
            xaieTile, loc, type, metricSet, 0, 0, memoryEvents, aieConfig);
        std::copy(streamPorts.begin(), streamPorts.end(), back_inserter(mStreamPorts));
          
        // Set overall start/end for trace capture
        if (memoryTrace->setCntrEvent(traceStartEvent, traceEndEvent) != XAIE_OK)
          break;

        auto ret = memoryTrace->reserve();
        if (ret != XAIE_OK) {
          std::stringstream msg;
          msg << "Unable to reserve memory trace control for " << tileName;
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());

          freeResources();
          // Print resources availability for this tile
          aie::trace::printTileStats(aieDevice, tile);
          return false;
        }

        // Specify Sel0/Sel1 for memory tile events 21-44
        if (type == module_type::mem_tile) {
          auto iter0 = configChannel0.find(tile);
          auto iter1 = configChannel1.find(tile);
          uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;
          uint8_t channel1 = (iter1 == configChannel1.end()) ? 1 : iter1->second;
          aie::trace::configEventSelections(aieDevInst, loc, type, metricSet, channel0, channel1);

          // Record for runtime config file
          cfgTile->memory_tile_trace_config.port_trace_ids[0] = channel0;
          cfgTile->memory_tile_trace_config.port_trace_ids[1] = channel1;
          if (aie::isInputSet(type, metricSet)) {
            cfgTile->memory_tile_trace_config.port_trace_is_master[0] = true;
            cfgTile->memory_tile_trace_config.port_trace_is_master[1] = true;
            cfgTile->memory_tile_trace_config.s2mm_channels[0] = channel0;
            if (channel0 != channel1)
              cfgTile->memory_tile_trace_config.s2mm_channels[1] = channel1;
          } 
          else {
            cfgTile->memory_tile_trace_config.port_trace_is_master[0] = false;
            cfgTile->memory_tile_trace_config.port_trace_is_master[1] = false;
            cfgTile->memory_tile_trace_config.mm2s_channels[0] = channel0;
            if (channel0 != channel1)
              cfgTile->memory_tile_trace_config.mm2s_channels[1] = channel1;
          }
        }

        // Configure memory trace events
        for (int i = 0; i < memoryEvents.size(); i++) {
          bool isCoreEvent = aie::trace::isCoreModuleEvent(memoryEvents[i]);
          XAie_ModuleType mod = isCoreEvent ? XAIE_CORE_MOD : XAIE_MEM_MOD;

          auto TraceE = memory.traceEvent();
          TraceE->setEvent(mod, memoryEvents[i]);
          if (TraceE->reserve() != XAIE_OK)
            break;
          if (TraceE->start() != XAIE_OK)
            break;
          numMemoryTraceEvents++;
          
          // Configure edge events (as needed)
          aie::trace::configEdgeEvents(aieDevInst, tile, type, metricSet, memoryEvents[i]);

          // Update config file
          // Get Trace slot
          uint32_t S = 0;
          XAie_LocType L;
          XAie_ModuleType M;
          TraceE->getRscId(L, M, S);

          // Get physical event
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, memoryEvents[i], &phyEvent);

          if (isCoreEvent) {
            auto bcId = TraceE->getBc();
            coreToMemBcMask |= (1 << bcId);
            
            cfgTile->core_trace_config.internal_events_broadcast[bcId] = phyEvent;
            cfgTile->memory_trace_config.traced_events[S] = aie::bcIdToEvent(bcId);
          }
          else {
            cfgTile->memory_tile_trace_config.traced_events[S] = phyEvent;
          }
        }

        // Add trace control events to config file
        {
          uint8_t phyEvent = 0;

          // Start
          if (aie::trace::isCoreModuleEvent(traceStartEvent)) {
            auto bcId = memoryTrace->getStartBc();
            coreToMemBcMask |= (1 << bcId);

            XAie_EventLogicalToPhysicalConv(aieDevInst, loc, XAIE_CORE_MOD, traceStartEvent, &phyEvent);
            cfgTile->memory_trace_config.start_event = aie::bcIdToEvent(bcId);
            cfgTile->core_trace_config.internal_events_broadcast[bcId] = phyEvent;
          }
          else {
            XAie_EventLogicalToPhysicalConv(aieDevInst, loc, XAIE_MEM_MOD, traceStartEvent, &phyEvent);
            if (type == module_type::mem_tile)
              cfgTile->memory_tile_trace_config.start_event = phyEvent;
            else
              cfgTile->memory_trace_config.start_event = phyEvent;
          }

          // Stop
          if (aie::trace::isCoreModuleEvent(traceEndEvent)) {
            auto bcId = memoryTrace->getStopBc();
            coreToMemBcMask |= (1 << bcId);
          
            XAie_EventLogicalToPhysicalConv(aieDevInst, loc, XAIE_CORE_MOD, traceEndEvent, &phyEvent);
            cfgTile->memory_trace_config.stop_event = aie::bcIdToEvent(bcId);
            cfgTile->core_trace_config.internal_events_broadcast[bcId] = phyEvent;

            // Use east broadcasting for AIE2+ or odd absolute rows of AIE1 checkerboard
            if ((row % 2) || (metadata->getHardwareGen() > 1))
              cfgTile->core_trace_config.broadcast_mask_east = coreToMemBcMask;
            else
              cfgTile->core_trace_config.broadcast_mask_west = coreToMemBcMask;
          }
          else {
            XAie_EventLogicalToPhysicalConv(aieDevInst, loc, XAIE_MEM_MOD, traceEndEvent, &phyEvent);
            if (type == module_type::mem_tile)
              cfgTile->memory_tile_trace_config.stop_event = phyEvent;
            else
              cfgTile->memory_trace_config.stop_event = phyEvent;
          }
        }

        // Record allocated trace events
        mNumTileTraceEvents[typeInt][numMemoryTraceEvents]++;
        memoryEvents.clear();
        
        // Specify packet type and ID then start memory trace
        // NOTE: always use time packets
        if (memoryTrace->setMode(XAIE_TRACE_EVENT_TIME) != XAIE_OK)
          break;
        uint8_t packetType = (type == module_type::mem_tile) ? 3 : 1;
        XAie_Packet pkt = {0, packetType};
        if (memoryTrace->setPkt(pkt) != XAIE_OK)
          break;
        if (memoryTrace->start() != XAIE_OK)
          break;

        // Update memory packet type in config file
        if (type == module_type::mem_tile)
          cfgTile->memory_tile_trace_config.packet_type = packetType;
        else
          cfgTile->memory_trace_config.packet_type = packetType;
      }

      //
      // 5. Configure Interface Tile Tracing Events
      //
      if (type == module_type::shim) {
        if (aie::isDebugVerbosity()) {
          std::stringstream msg;
          msg << "Reserving " << interfaceEvents.size() << " trace events for " << tileName;
          xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        }

        auto shimTrace = shim.traceControl();
        if (shimTrace->setCntrEvent(mInterfaceTileTraceStartEvent, mInterfaceTileTraceEndEvent) != XAIE_OK)
          break;

        auto ret = shimTrace->reserve();
        if (ret != XAIE_OK) {
          std::stringstream msg;
          msg << "Unable to reserve trace control for " << tileName;
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());

          freeResources();
          // Print resources availability for this tile
          aie::trace::printTileStats(aieDevice, tile);
          return false;
        }

        // Specify channels for interface tile DMA events
        auto iter0 = configChannel0.find(tile);
        auto iter1 = configChannel1.find(tile);
        uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;
        uint8_t channel1 = (iter1 == configChannel1.end()) ? 1 : iter1->second;

        // Modify events as needed
        aie::trace::modifyEvents(type, subtype, metricSet, channel0, interfaceEvents);

        // Record for runtime config file
        if (type == module_type::shim) {
          if (aie::isInputSet(type, metricSet)) {
            cfgTile->interface_tile_trace_config.mm2s_channels[0] = channel0;
            if (channel0 != channel1)
              cfgTile->interface_tile_trace_config.mm2s_channels[1] = channel1;
          } 
          else {
            cfgTile->interface_tile_trace_config.s2mm_channels[0] = channel0;
            if (channel0 != channel1)
              cfgTile->interface_tile_trace_config.s2mm_channels[1] = channel1;
          }
        }

        // Configure event ports on stream switch
        auto streamPorts = aie::trace::configStreamSwitchPorts(aieDevInst, tile,
            xaieTile, loc, type, metricSet, channel0, channel1, interfaceEvents,
            cfgTile->interface_tile_trace_config);
        std::copy(streamPorts.begin(), streamPorts.end(), back_inserter(mStreamPorts));

        // Configure interface tile trace events
        for (int i = 0; i < interfaceEvents.size(); i++) {
          auto event = interfaceEvents.at(i);
          auto TraceE = shim.traceEvent();
          TraceE->setEvent(XAIE_PL_MOD, event);
          if (TraceE->reserve() != XAIE_OK)
            break;
          if (TraceE->start() != XAIE_OK)
            break;
          numInterfaceTraceEvents++;

          // Update config file
          // Get Trace slot
          uint32_t S = 0;
          XAie_LocType L;
          XAie_ModuleType M;
          TraceE->getRscId(L, M, S);
          // Get Physical event
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, XAIE_PL_MOD, event, &phyEvent);
          cfgTile->interface_tile_trace_config.traced_events[S] = phyEvent;
        }

        // Update config file
        {
          // Add interface trace control events
          // Start
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, XAIE_PL_MOD, mInterfaceTileTraceStartEvent, &phyEvent);
          cfgTile->interface_tile_trace_config.start_event = phyEvent;
          // Stop
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, XAIE_PL_MOD, mInterfaceTileTraceEndEvent, &phyEvent);
          cfgTile->interface_tile_trace_config.stop_event = phyEvent;
        }

        // Record allocated trace events
        mNumTileTraceEvents[typeInt][numInterfaceTraceEvents]++;
        
        // Specify packet type and ID then start interface tile trace
        // NOTE: always use time packets
        if (shimTrace->setMode(XAIE_TRACE_EVENT_TIME) != XAIE_OK)
          break;
        uint8_t packetType = 4;
        XAie_Packet pkt = {0, packetType};
        if (shimTrace->setPkt(pkt) != XAIE_OK)
          break;
        if (shimTrace->start() != XAIE_OK)
          break;
        cfgTile->interface_tile_trace_config.packet_type = packetType;
      } // interface tiles

      if (aie::isDebugVerbosity()) {
        std::stringstream msg;
        msg << "Reserved ";
        if (type == module_type::core)
          msg << numCoreTraceEvents << " core and " << numMemoryTraceEvents << " memory";
        else if (type == module_type::mem_tile)
          msg << numMemoryTraceEvents << " memory tile";
        else if (type == module_type::shim)
          msg << numInterfaceTraceEvents << " interface tile";
        msg << " trace events for " << tileName << ". Adding tile to static database.";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
      }

      // Add config info to static database
      // NOTE: Do not access cfgTile after this
      (db->getStaticInfo()).addAIECfgTile(deviceId, cfgTile);
    }  // For tiles

    // Report and store trace events per tile
    for (int m = 0; m < static_cast<int>(module_type::num_types); ++m) {
      aie::trace::printTraceEventStats(m, mNumTileTraceEvents[m]);
      for (int n = 0; n <= NUM_TRACE_EVENTS; ++n)
        (db->getStaticInfo()).addAIECoreEventResources(deviceId, n, mNumTileTraceEvents[m][n]);
    }
    return true;
  }  // end setMetricsSettings

  /****************************************************************************
   * Flush trace modules by forcing end events
   *
   * Trace modules buffer partial packets. At end of run, this needs to be 
   * flushed using a custom end event. This applies to trace windowing and 
   * passive tiles like memory and interface.
   *
   ***************************************************************************/
  void AieTrace_EdgeImpl::flushTraceModules()
  {
    if (mTraceFlushLocs.empty() && mMemoryTileTraceFlushLocs.empty()
        && mInterfaceTileTraceFlushLocs.empty())
      return;

    auto handle = metadata->getHandle();
    aieDevInst = static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle));

    if (aie::isDebugVerbosity()) {
      std::stringstream msg;
      msg << "Flushing AIE trace by forcing end event for " << mTraceFlushLocs.size()
          << " AIE tiles, " << mMemoryTileTraceFlushLocs.size() << " memory tiles, and " 
          << mInterfaceTileTraceFlushLocs.size() << " interface tiles.";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    }

    // Flush trace by forcing end event
    // NOTE: this informs tiles to output remaining packets (even if partial)
    for (const auto& loc : mTraceFlushLocs) 
      XAie_EventGenerate(aieDevInst, loc, XAIE_CORE_MOD, mCoreTraceEndEvent);
    for (const auto& loc : mMemoryTileTraceFlushLocs)
      XAie_EventGenerate(aieDevInst, loc, XAIE_MEM_MOD, mMemoryTileTraceEndEvent);
    for (const auto& loc : mInterfaceTileTraceFlushLocs)
      XAie_EventGenerate(aieDevInst, loc, XAIE_PL_MOD, mInterfaceTileTraceEndEvent);

    mTraceFlushLocs.clear();
    mMemoryTileTraceFlushLocs.clear();
    mInterfaceTileTraceFlushLocs.clear();
  }

  /****************************************************************************
   * Poll AIE timers (for system timeline only)
   ***************************************************************************/
  void AieTrace_EdgeImpl::pollTimers(uint64_t index, void* handle)
  {
    // Wait until xclbin has been loaded and device has been updated in database
    if (!(db->getStaticInfo().isDeviceReady(index)))
      return;
    XAie_DevInst* aieDevInst =
      static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle)) ;
    if (!aieDevInst)
      return;

    // Only read first timer and assume common time domain across all tiles
    static auto tileMetrics = metadata->getConfigMetrics();
    if (tileMetrics.empty())
      return;

    static auto tile   = tileMetrics.begin()->first;
    auto loc           = XAie_TileLoc(tile.col, tile.row);
    auto moduleType    = aie::getModuleType(tile.row, metadata->getRowOffset());
    auto falModuleType =  (moduleType == module_type::core) ? XAIE_CORE_MOD 
                       : ((moduleType == module_type::shim) ? XAIE_PL_MOD 
                       : XAIE_MEM_MOD);

    uint64_t timerValue = 0;  
    auto timestamp1 = xrt_core::time_ns();
    XAie_ReadTimer(aieDevInst, loc, falModuleType, &timerValue);
    auto timestamp2 = xrt_core::time_ns();
    
    std::vector<uint64_t> values;
    values.push_back(tile.col);
    values.push_back( aie::getRelativeRow(tile.row, metadata->getRowOffset()) );
    values.push_back(timerValue);

    db->getDynamicInfo().addAIETimerSample(index, timestamp1, timestamp2, values);
  }
}  // namespace xdp
