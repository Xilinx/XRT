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

#define XDP_SOURCE

#include "xdp/profile/plugin/aie_trace/edge/aie_trace.h"
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

  AieTrace_EdgeImpl::AieTrace_EdgeImpl(VPDatabase* database, std::shared_ptr<AieTraceMetadata> metadata)
      : AieTraceImpl(database, metadata)
  {
    //
    // Pre-defined metric sets
    //
    // **** Core Module Trace ****
    // NOTE: these are supplemented with counter events as those are dependent on counter #
    mCoreEventSets = {{"functions", 
                       {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE}},
                      {"functions_partial_stalls", 
                       {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE}},
                      {"functions_all_stalls", 
                       {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE}},
                      {"all", 
                       {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE}}};

    // These are also broadcast to memory module
    mCoreTraceStartEvent = XAIE_EVENT_ACTIVE_CORE;
    mCoreTraceEndEvent = XAIE_EVENT_DISABLED_CORE;

    // **** Memory Module Trace ****
    // NOTE 1: Core events listed here are broadcast by the resource manager
    // NOTE 2: These are supplemented with counter events as those are dependent on counter # 
    // NOTE 3: For now, 'all' is the same as 'functions_all_stalls'.
    // Combo events (required for all) have limited support in the resource manager.
    mMemoryEventSets = {{"functions", 
                         {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE}},
                        {"functions_partial_stalls",
                         {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE, XAIE_EVENT_STREAM_STALL_CORE,
                          XAIE_EVENT_CASCADE_STALL_CORE, XAIE_EVENT_LOCK_STALL_CORE}},
                        {"functions_all_stalls",
                         {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE, XAIE_EVENT_MEMORY_STALL_CORE,
                          XAIE_EVENT_STREAM_STALL_CORE, XAIE_EVENT_CASCADE_STALL_CORE, XAIE_EVENT_LOCK_STALL_CORE}},
                        {"all",
                         {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE, XAIE_EVENT_MEMORY_STALL_CORE,
                          XAIE_EVENT_STREAM_STALL_CORE, XAIE_EVENT_CASCADE_STALL_CORE, XAIE_EVENT_LOCK_STALL_CORE}}};

    // Core/memory module counters
    // NOTE 1: Only applicable to AIE1 devices
    // NOTE 2: Reset events are dependent on actual profile counter reserved.
    // NOTE 3: These counters are required HW workarounds with thresholds chosen
    //         to produce events before hitting the bug. For example, sync packets
    //         occur after 1024 cycles and with no events, is incorrectly repeated.
    if (metadata->getHardwareGen() == 1) {
      auto counterScheme = metadata->getCounterScheme();

      if (counterScheme == "es1") {
        mCoreCounterStartEvents = {XAIE_EVENT_ACTIVE_CORE, XAIE_EVENT_ACTIVE_CORE};
        mCoreCounterEndEvents = {XAIE_EVENT_DISABLED_CORE, XAIE_EVENT_DISABLED_CORE};
        mCoreCounterEventValues = {ES1_TRACE_COUNTER, ES1_TRACE_COUNTER * ES1_TRACE_COUNTER};

        mMemoryCounterStartEvents = {XAIE_EVENT_TRUE_MEM, XAIE_EVENT_TRUE_MEM};
        mMemoryCounterEndEvents = {XAIE_EVENT_NONE_MEM, XAIE_EVENT_NONE_MEM};
        mMemoryCounterEventValues = {ES1_TRACE_COUNTER, ES1_TRACE_COUNTER * ES1_TRACE_COUNTER};
      } else if (counterScheme == "es2") {
        mCoreCounterStartEvents = {XAIE_EVENT_ACTIVE_CORE};
        mCoreCounterEndEvents = {XAIE_EVENT_DISABLED_CORE};
        mCoreCounterEventValues = {ES2_TRACE_COUNTER};

        mMemoryCounterStartEvents = {XAIE_EVENT_TRUE_MEM};
        mMemoryCounterEndEvents = {XAIE_EVENT_NONE_MEM};
        mMemoryCounterEventValues = {ES2_TRACE_COUNTER};
      }
    }

    // **** Memory Tile Trace ****
    mMemoryTileEventSets = {
        {"input_channels",
         {XAIE_EVENT_DMA_S2MM_SEL0_START_TASK_MEM_TILE,    XAIE_EVENT_DMA_S2MM_SEL1_START_TASK_MEM_TILE,
          XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_BD_MEM_TILE,   XAIE_EVENT_DMA_S2MM_SEL1_FINISHED_BD_MEM_TILE,
          XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_TASK_MEM_TILE, XAIE_EVENT_DMA_S2MM_SEL1_FINISHED_TASK_MEM_TILE}},
        {"input_channels_stalls",
         {XAIE_EVENT_DMA_S2MM_SEL0_START_TASK_MEM_TILE,    XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_BD_MEM_TILE,
          XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_TASK_MEM_TILE, XAIE_EVENT_EDGE_DETECTION_EVENT_0_MEM_TILE,
          XAIE_EVENT_EDGE_DETECTION_EVENT_1_MEM_TILE,      XAIE_EVENT_DMA_S2MM_SEL0_STREAM_STARVATION_MEM_TILE, 
          XAIE_EVENT_DMA_S2MM_SEL0_MEMORY_BACKPRESSURE_MEM_TILE}},
        {"output_channels",
         {XAIE_EVENT_DMA_MM2S_SEL0_START_TASK_MEM_TILE,    XAIE_EVENT_DMA_MM2S_SEL1_START_TASK_MEM_TILE,
          XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_MEM_TILE,   XAIE_EVENT_DMA_MM2S_SEL1_FINISHED_BD_MEM_TILE,
          XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_TASK_MEM_TILE, XAIE_EVENT_DMA_MM2S_SEL1_FINISHED_TASK_MEM_TILE}},
        {"output_channels_stalls",
         {XAIE_EVENT_DMA_MM2S_SEL0_START_TASK_MEM_TILE,    XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_MEM_TILE,
          XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_TASK_MEM_TILE, XAIE_EVENT_EDGE_DETECTION_EVENT_0_MEM_TILE, 
          XAIE_EVENT_EDGE_DETECTION_EVENT_1_MEM_TILE,      XAIE_EVENT_DMA_MM2S_SEL0_STREAM_BACKPRESSURE_MEM_TILE, 
          XAIE_EVENT_DMA_MM2S_SEL0_MEMORY_STARVATION_MEM_TILE}}};

    // Memory tile trace is flushed at end of run
    mMemoryTileTraceStartEvent = XAIE_EVENT_TRUE_MEM_TILE;
    mMemoryTileTraceEndEvent = XAIE_EVENT_USER_EVENT_1_MEM_TILE;

    // **** Interface Tile Trace ****
    mInterfaceTileEventSets = {{"ports",
                                {XAIE_EVENT_PORT_TLAST_0_PL,   XAIE_EVENT_PORT_TLAST_1_PL,
                                 XAIE_EVENT_PORT_TLAST_2_PL,   XAIE_EVENT_PORT_TLAST_3_PL, 
                                 XAIE_EVENT_PORT_TLAST_4_PL,   XAIE_EVENT_PORT_TLAST_5_PL,
                                 XAIE_EVENT_PORT_TLAST_6_PL,   XAIE_EVENT_PORT_TLAST_7_PL}},
                               {"input_ports",
                                {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_RUNNING_1_PL,
                                 XAIE_EVENT_PORT_RUNNING_2_PL, XAIE_EVENT_PORT_RUNNING_3_PL}},
                               {"output_ports",
                                {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_RUNNING_1_PL,
                                 XAIE_EVENT_PORT_RUNNING_2_PL, XAIE_EVENT_PORT_RUNNING_3_PL}},
                               {"input_ports_stalls",
                                {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_STALLED_0_PL,
                                 XAIE_EVENT_PORT_RUNNING_1_PL, XAIE_EVENT_PORT_STALLED_1_PL}},
                               {"output_ports_stalls",
                                {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_STALLED_0_PL,
                                 XAIE_EVENT_PORT_RUNNING_1_PL, XAIE_EVENT_PORT_STALLED_1_PL}}};

    if (metadata->getHardwareGen() == 1) {
      mInterfaceTileEventSets["input_ports_details"] = {
          XAIE_EVENT_DMA_MM2S_0_START_BD_PL,   XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_PL,
          XAIE_EVENT_DMA_MM2S_0_GO_TO_IDLE_PL, XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_ACQUIRE_PL,
          XAIE_EVENT_DMA_MM2S_1_START_BD_PL,   XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_PL,
          XAIE_EVENT_DMA_MM2S_1_GO_TO_IDLE_PL, XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_ACQUIRE_PL};
      mInterfaceTileEventSets["output_ports_details"] = {
          XAIE_EVENT_DMA_S2MM_0_START_BD_PL,   XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_PL,
          XAIE_EVENT_DMA_S2MM_0_GO_TO_IDLE_PL, XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_ACQUIRE_PL,
          XAIE_EVENT_DMA_S2MM_1_START_BD_PL,   XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_PL,
          XAIE_EVENT_DMA_S2MM_1_GO_TO_IDLE_PL, XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_ACQUIRE_PL};
    } else {
      mInterfaceTileEventSets["input_ports_details"] = {
          XAIE_EVENT_DMA_MM2S_0_START_TASK_PL,          XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_PL,
          XAIE_EVENT_DMA_MM2S_0_FINISHED_TASK_PL,       XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_PL,
          XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_PL, XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_PL};
      mInterfaceTileEventSets["output_ports_details"] = {
          XAIE_EVENT_DMA_S2MM_0_START_TASK_PL,        XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_PL,
          XAIE_EVENT_DMA_S2MM_0_FINISHED_TASK_PL,     XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_PL,
          XAIE_EVENT_DMA_S2MM_0_STREAM_STARVATION_PL, XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL};
    }

    // Interface tile trace is flushed at end of run
    mInterfaceTileTraceStartEvent = XAIE_EVENT_TRUE_PL;
    mInterfaceTileTraceEndEvent = XAIE_EVENT_USER_EVENT_1_PL;
  }

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
      msg << "Available memory performance counters for aie trace : " << available << std::endl
          << "Required memory performance counters for aie trace  : " << required;
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
      return false;
    }

    // Memory module/tile trace slots
    available = stats.getNumRsc(loc, XAIE_MEM_MOD, xaiefal::XAIE_TRACE_EVENTS_RSC);
    required = mMemoryCounterStartEvents.size() + mMemoryEventSets[metricSet].size();
    if (available < required) {
      msg << "Available memory trace slots for aie trace : " << available << std::endl
          << "Required memory trace slots for aie trace  : " << required;
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

  // Print out resource usage statistics for a given tile
  void AieTrace_EdgeImpl::printTileStats(xaiefal::XAieDev* aieDevice, const tile_type& tile)
  {
    if (xrt_core::config::get_verbosity() < static_cast<uint32_t>(severity_level::info))
      return;

    auto col = tile.col;
    auto row = tile.row + metadata->getRowOffset();
    auto loc = XAie_TileLoc(col, row);
    std::stringstream msg;

    const std::string groups[3] = {
      XAIEDEV_DEFAULT_GROUP_GENERIC,
      XAIEDEV_DEFAULT_GROUP_STATIC, 
      XAIEDEV_DEFAULT_GROUP_AVAIL
    };

    msg << "Resource usage stats for Tile : (" << col << "," << row << ") Module : Core" << std::endl;
    for (auto& g : groups) {
      auto stats = aieDevice->getRscStat(g);
      auto pc = stats.getNumRsc(loc, XAIE_CORE_MOD, XAIE_PERFCNT_RSC);
      auto ts = stats.getNumRsc(loc, XAIE_CORE_MOD, xaiefal::XAIE_TRACE_EVENTS_RSC);
      auto bc = stats.getNumRsc(loc, XAIE_CORE_MOD, XAIE_BCAST_CHANNEL_RSC);
      msg << "Resource Group : " << std::left << std::setw(10) << g << " "
          << "Performance Counters : " << pc << " "
          << "Trace Slots : " << ts << " "
          << "Broadcast Channels : " << bc << " " 
          << std::endl;
    }
    msg << "Resource usage stats for Tile : (" << col << "," << row << ") Module : Memory" << std::endl;
    for (auto& g : groups) {
      auto stats = aieDevice->getRscStat(g);
      auto pc = stats.getNumRsc(loc, XAIE_MEM_MOD, XAIE_PERFCNT_RSC);
      auto ts = stats.getNumRsc(loc, XAIE_MEM_MOD, xaiefal::XAIE_TRACE_EVENTS_RSC);
      auto bc = stats.getNumRsc(loc, XAIE_MEM_MOD, XAIE_BCAST_CHANNEL_RSC);
      msg << "Resource Group : " << std::left << std::setw(10) << g << " "
          << "Performance Counters : " << pc << " "
          << "Trace Slots : " << ts << " "
          << "Broadcast Channels : " << bc << " " 
          << std::endl;
    }
    xrt_core::message::send(severity_level::info, "XRT", msg.str());
  }

  // Print out reserved trace events
  void AieTrace_EdgeImpl::printTraceEventStats(uint64_t deviceId)
  {
    std::map<module_type, std::string> modNames {
      {module_type::core,     "AIE modules"},
      {module_type::dma,      "AIE tile memory modules"},
      {module_type::shim,     "interface tiles"},
      {module_type::mem_tile, "memory tiles"}
    };

    for (int m = 0; m < static_cast<int>(module_type::num_types); ++m) {
      auto mod = static_cast<module_type>(m);
      std::stringstream msg;
      msg << "AIE trace events reserved in " << modNames[mod] << " - ";
      for (int n = 0; n <= NUM_TRACE_EVENTS; ++n) {
        int numTiles = mNumTileTraceEvents[m][n];
        if (numTiles == 0)
          continue;
        msg << n << ": " << numTiles << " tiles, ";

        (db->getStaticInfo()).addAIECoreEventResources(deviceId, n, mNumTileTraceEvents[m][n]);
      }
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
    }
  }

  // Release resources
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

  module_type AieTrace_EdgeImpl::getTileType(uint16_t absRow)
  {
    if (absRow == 0)
      return module_type::shim;
    if (absRow < metadata->getRowOffset())
      return module_type::mem_tile;
    return module_type::core;
  }

  uint16_t AieTrace_EdgeImpl::getRelativeRow(uint16_t absRow)
  {
    auto rowOffset = metadata->getRowOffset();
    if (absRow == 0)
      return 0;
    if (absRow < rowOffset)
      return (absRow - 1);
    return (absRow - rowOffset);
  }

  module_type 
  AieTrace_EdgeImpl::getModuleType(uint16_t absRow, XAie_ModuleType mod)
  {
    if (absRow == 0)
      return module_type::shim;
    if (absRow < metadata->getRowOffset())
      return module_type::mem_tile;
    return ((mod == XAIE_CORE_MOD) ? module_type::core : module_type::dma);
  }

  bool AieTrace_EdgeImpl::isStreamSwitchPortEvent(const XAie_Events event)
  {
    // AIE tiles
    if ((event > XAIE_EVENT_GROUP_STREAM_SWITCH_CORE) 
        && (event < XAIE_EVENT_GROUP_BROADCAST_CORE))
      return true;
    // Interface tiles
    if ((event > XAIE_EVENT_GROUP_STREAM_SWITCH_PL) 
        && (event < XAIE_EVENT_GROUP_BROADCAST_A_PL))
      return true;
    // Memory tiles
    if ((event > XAIE_EVENT_GROUP_STREAM_SWITCH_MEM_TILE) 
        && (event < XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM_TILE))
      return true;

    return false;
  }

  // Configure stream switch ports for monitoring purposes
  // NOTE: Used to monitor streams: trace, interfaces, and memory tiles
  void AieTrace_EdgeImpl::configStreamSwitchPorts(XAie_DevInst* aieDevInst, const tile_type& tile,
                                                  xaiefal::XAieTile& xaieTile, const XAie_LocType loc,
                                                  const module_type type, const XAie_Events event,
                                                  const int countnum, const std::string metricSet,
                                                  const uint8_t channel)
  {
    // Only configure as needed: must be applicable event and only need at most two
    if (!isStreamSwitchPortEvent(event) || (countnum > 1))
      return;

    auto switchPortRsc = xaieTile.sswitchPort();
    auto ret = switchPortRsc->reserve();
    if (ret != AieRC::XAIE_OK)
      return;

    uint32_t rscId = 0;
    XAie_LocType tmpLoc;
    XAie_ModuleType tmpMod;
    switchPortRsc->getRscId(tmpLoc, tmpMod, rscId);

    // AIE Tiles (e.g., trace streams)
    if (type == module_type::core) {
      // Define stream switch port to monitor core or memory trace
      uint8_t traceSelect = (event == XAIE_EVENT_PORT_RUNNING_0_CORE) ? 0 : 1;
      XAie_EventSelectStrmPort(aieDevInst, loc, rscId, XAIE_STRMSW_SLAVE, TRACE, traceSelect);
      return;
    }

    // Interface tiles (e.g., PLIO, GMIO)
    if (type == module_type::shim) {
      // Grab slave/master and stream ID
      // NOTE: stored in getTilesForProfiling() above
      auto slaveOrMaster = (tile.itr_mem_col == 0) ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
      auto streamPortId  = static_cast<uint8_t>(tile.itr_mem_row);

      // Define stream switch port to monitor interface 
      XAie_EventSelectStrmPort(aieDevInst, loc, rscId, slaveOrMaster, SOUTH, streamPortId);
      return;
    }

    // MEM tiles
    if (metricSet.find("trace") != std::string::npos) {
      XAie_EventSelectStrmPort(aieDevInst, loc, rscId, XAIE_STRMSW_SLAVE, TRACE, 0);
    }
    else {
      auto slaveOrMaster = (metricSet.find("output") != std::string::npos) ?
        XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
      XAie_EventSelectStrmPort(aieDevInst, loc, rscId, slaveOrMaster, DMA, channel);
    }

    mStreamPorts.push_back(switchPortRsc);
  }

  void AieTrace_EdgeImpl::configEventSelections(XAie_DevInst* aieDevInst, const XAie_LocType loc,
                                                const XAie_ModuleType mod, const module_type type,
                                                const std::string metricSet, const uint8_t channel0,
                                                const uint8_t channel1)
  {
    if (type != module_type::mem_tile)
      return;

    XAie_DmaDirection dmaDir = (metricSet.find("input") != std::string::npos) ? DMA_S2MM : DMA_MM2S;
    XAie_EventSelectDmaChannel(aieDevInst, loc, 0, dmaDir, channel0);
    XAie_EventSelectDmaChannel(aieDevInst, loc, 1, dmaDir, channel1);
  }

  void AieTrace_EdgeImpl::configEdgeEvents(XAie_DevInst* aieDevInst, const tile_type& tile,
                                           const std::string metricSet, const XAie_Events event)
  {
    // For now, only memory tiles are supported
    if ((event != XAIE_EVENT_EDGE_DETECTION_EVENT_0_MEM_TILE)
        && (event != XAIE_EVENT_EDGE_DETECTION_EVENT_1_MEM_TILE))
      return;

    // AIE core register offsets
    constexpr uint64_t AIE_OFFSET_EDGE_CONTROL_MEM_TILE = 0x94408;

    // Event is DMA_S2MM_Sel0_stream_starvation or DMA_MM2S_Sel0_stalled_lock
    uint16_t eventNum = (metricSet.find("input") != std::string::npos)
        ? EVENT_MEM_TILE_DMA_S2MM_SEL0_STREAM_STARVATION
        : EVENT_MEM_TILE_DMA_MM2S_SEL0_STALLED_LOCK;

    // Register Edge_Detection_event_control
    // 26    Event 1 triggered on falling edge
    // 25    Event 1 triggered on rising edge
    // 23:16 Input event for edge event 1
    // 10    Event 0 triggered on falling edge
    //  9    Event 0 triggered on rising edge
    //  7:0  Input event for edge event 0
    uint32_t edgeEventsValue = (1 << 26) + (eventNum << 16) + (1 << 9) + eventNum;

    auto tileOffset = _XAie_GetTileAddr(aieDevInst, tile.row, tile.col);
    XAie_Write32(aieDevInst, tileOffset + AIE_OFFSET_EDGE_CONTROL_MEM_TILE, edgeEventsValue);
  }

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

      if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(severity_level::info))
        xrt_core::message::send(severity_level::info, "XRT", "Enabling trace flush");
    }

    // Iterate over all used/specified tiles
    // NOTE: rows are stored as absolute as required by resource manager
    for (auto& tileMetric : metadata->getConfigMetrics()) {
      auto& metricSet = tileMetric.second;
      auto tile       = tileMetric.first;
      auto col        = tile.col;
      auto row        = tile.row;
      auto type       = getTileType(row);
      auto typeInt    = static_cast<int>(type);
      auto& xaieTile  = aieDevice->tile(col, row);
      auto loc        = XAie_TileLoc(col, row);

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
      EventVector memoryCrossEvents;
      EventVector memoryEvents;
      EventVector interfaceEvents;
      if (type == module_type::core) {
        coreEvents = mCoreEventSets[metricSet];
        memoryCrossEvents = mMemoryEventSets[metricSet];
      }
      else if (type == module_type::mem_tile) {
        memoryEvents = mMemoryTileEventSets[metricSet];
      }
      else if (type == module_type::shim) {
        interfaceEvents = mInterfaceTileEventSets[metricSet];
      }

      if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(severity_level::info)) {
        std::stringstream infoMsg;
        auto tileName = (type == module_type::mem_tile) ? "memory" 
            : ((type == module_type::shim) ? "interface" : "AIE");
        infoMsg << "Configuring " << tileName << " tile (" << col << "," << row 
                << ") for trace using metric set " << metricSet;
        xrt_core::message::send(severity_level::info, "XRT", infoMsg.str());
      }

      // Check Resource Availability
      if (!tileHasFreeRsc(aieDevice, loc, type, metricSet)) {
        xrt_core::message::send(severity_level::warning, "XRT",
            "Tile doesn't have enough free resources for trace. Aborting trace configuration.");
        printTileStats(aieDevice, tile);
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
      if (type == module_type::core) {
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
            memoryCrossEvents.push_back(counterEvent);

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
      if (type == module_type::core) {
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
          msg << "Unable to reserve " << mCoreCounterStartEvents.size() << " core counters"
              << " and " << mMemoryCounterStartEvents.size() << " memory counters"
              << " for AIE tile (" << col << "," << row << ") required for trace.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());

          freeResources();
          // Print resources availability for this tile
          printTileStats(aieDevice, tile);
          return false;
        }
      }

      //
      // 3. Configure Core Tracing Events
      //
      if (type == module_type::core) {
        XAie_ModuleType mod = XAIE_CORE_MOD;
        uint8_t phyEvent = 0;
        auto coreTrace = core.traceControl();

        // Delay cycles and user control are not compatible with each other
        if (metadata->getUseGraphIterator()) {
          if (!configureStartIteration(core))
            break;
        } else if (metadata->getUseDelay()) {
          if (!configureStartDelay(core))
            break;
        }

        // Set overall start/end for trace capture
        // Wendy said this should be done first
        if (coreTrace->setCntrEvent(mCoreTraceStartEvent, mCoreTraceEndEvent) != XAIE_OK)
          break;

        auto ret = coreTrace->reserve();
        if (ret != XAIE_OK) {
          std::stringstream msg;
          msg << "Unable to reserve core module trace control for AIE tile (" << col << "," << row << ").";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());

          freeResources();
          // Print resources availability for this tile
          printTileStats(aieDevice, tile);
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

        coreEvents.clear();
        mNumTileTraceEvents[typeInt][numCoreTraceEvents]++;

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
        auto memoryTrace = memory.traceControl();
        // Set overall start/end for trace capture
        // Wendy said this should be done first
        auto traceStartEvent = (type == module_type::core) ? mCoreTraceStartEvent : mMemoryTileTraceStartEvent;
        auto traceEndEvent = (type == module_type::core) ? mCoreTraceEndEvent : mMemoryTileTraceEndEvent;
        if (memoryTrace->setCntrEvent(traceStartEvent, traceEndEvent) != XAIE_OK)
          break;

        auto ret = memoryTrace->reserve();
        if (ret != XAIE_OK) {
          std::stringstream msg;
          msg << "Unable to reserve memory trace control for AIE tile (" << col << "," << row << ").";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());

          freeResources();
          // Print resources availability for this tile
          printTileStats(aieDevice, tile);
          return false;
        }

        // Specify Sel0/Sel1 for memory tile events 21-44
        if (type == module_type::mem_tile) {
          auto iter0 = configChannel0.find(tile);
          auto iter1 = configChannel1.find(tile);
          uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;
          uint8_t channel1 = (iter1 == configChannel1.end()) ? 1 : iter1->second;
          configEventSelections(aieDevInst, loc, XAIE_MEM_MOD, type, metricSet, channel0, channel1);

          // Record for runtime config file
          cfgTile->memory_tile_trace_config.port_trace_ids[0] = channel0;
          cfgTile->memory_tile_trace_config.port_trace_ids[1] = channel1;
          if (metricSet.find("input") != std::string::npos) {
            cfgTile->memory_tile_trace_config.port_trace_is_master[0] = true;
            cfgTile->memory_tile_trace_config.port_trace_is_master[1] = true;
            cfgTile->memory_tile_trace_config.s2mm_channels[0] = channel0;
            if (channel0 != channel1)
              cfgTile->memory_tile_trace_config.s2mm_channels[1] = channel1;
          } else {
            cfgTile->memory_tile_trace_config.port_trace_is_master[0] = false;
            cfgTile->memory_tile_trace_config.port_trace_is_master[1] = false;
            cfgTile->memory_tile_trace_config.mm2s_channels[0] = channel0;
            if (channel0 != channel1)
              cfgTile->memory_tile_trace_config.mm2s_channels[1] = channel1;
          }
        }

        // Configure cross module events
        // NOTE: this is only applicable for memory modules, not memory tiles
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
          numMemoryTraceEvents++;

          // Update config file
          uint32_t S = 0;
          XAie_LocType L;
          XAie_ModuleType M;
          TraceE->getRscId(L, M, S);
          // Get physical event
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, XAIE_CORE_MOD, memoryCrossEvents[i], &phyEvent);

          if (type == module_type::mem_tile) {
            cfgTile->memory_tile_trace_config.traced_events[S] = phyEvent;
          } else {
            cfgTile->core_trace_config.internal_events_broadcast[bcId] = phyEvent;
            cfgTile->memory_trace_config.traced_events[S] = bcIdToEvent(bcId);
          }
        }

        // Configure memory trace events
        for (int i = 0; i < memoryEvents.size(); i++) {
          auto TraceE = memory.traceEvent();
          TraceE->setEvent(XAIE_MEM_MOD, memoryEvents[i]);
          if (TraceE->reserve() != XAIE_OK)
            break;
          if (TraceE->start() != XAIE_OK)
            break;
          numMemoryTraceEvents++;

          // Configure edge events (as needed)
          configEdgeEvents(aieDevInst, tile, metricSet, memoryEvents[i]);

          // Update config file
          // Get Trace slot
          uint32_t S = 0;
          XAie_LocType L;
          XAie_ModuleType M;
          TraceE->getRscId(L, M, S);
          // Get Physical event
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, XAIE_MEM_MOD, memoryEvents[i], &phyEvent);

          if (type == module_type::mem_tile)
            cfgTile->memory_tile_trace_config.traced_events[S] = phyEvent;
          else
            cfgTile->memory_trace_config.traced_events[S] = phyEvent;
        }

        // Update config file
        {
          // Add Memory trace control events
          // Start
          uint32_t bcBit = 0x1;
          auto bcId = memoryTrace->getStartBc();
          coreToMemBcMask |= (bcBit << bcId);
          uint8_t phyEvent = 0;
          if (type == module_type::mem_tile) {
            XAie_EventLogicalToPhysicalConv(aieDevInst, loc, XAIE_MEM_MOD, traceStartEvent, &phyEvent);
            cfgTile->memory_tile_trace_config.start_event = phyEvent;
          } else {
            XAie_EventLogicalToPhysicalConv(aieDevInst, loc, XAIE_CORE_MOD, traceStartEvent, &phyEvent);
            cfgTile->memory_trace_config.start_event = bcIdToEvent(bcId);
            cfgTile->core_trace_config.internal_events_broadcast[bcId] = phyEvent;
          }
          // Stop
          bcBit = 0x1;
          bcId = memoryTrace->getStopBc();
          coreToMemBcMask |= (bcBit << bcId);
          if (type == module_type::mem_tile) {
            XAie_EventLogicalToPhysicalConv(aieDevInst, loc, XAIE_MEM_MOD, traceEndEvent, &phyEvent);
            cfgTile->memory_tile_trace_config.stop_event = phyEvent;
          } else {
            XAie_EventLogicalToPhysicalConv(aieDevInst, loc, XAIE_CORE_MOD, traceEndEvent, &phyEvent);
            cfgTile->memory_trace_config.stop_event = bcIdToEvent(bcId);
            cfgTile->core_trace_config.internal_events_broadcast[bcId] = phyEvent;

            // Odd absolute rows change east mask end even row change west mask
            if (row % 2)
              cfgTile->core_trace_config.broadcast_mask_east = coreToMemBcMask;
            else
              cfgTile->core_trace_config.broadcast_mask_west = coreToMemBcMask;
          }
        }

        memoryEvents.clear();
        mNumTileTraceEvents[typeInt][numMemoryTraceEvents]++;
        
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
        auto shimTrace = shim.traceControl();
        if (shimTrace->setCntrEvent(mInterfaceTileTraceStartEvent, mInterfaceTileTraceEndEvent) != XAIE_OK)
          break;

        auto ret = shimTrace->reserve();
        if (ret != XAIE_OK) {
          std::stringstream msg;
          msg << "Unable to reserve trace control for interface tile (" << col << "," << row << ").";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());

          freeResources();
          // Print resources availability for this tile
          printTileStats(aieDevice, tile);
          return false;
        }

        // Specify Sel0/Sel1 for interface tile DMA events
        auto iter0 = configChannel0.find(tile);
        auto iter1 = configChannel1.find(tile);
        uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;
        uint8_t channel1 = (iter1 == configChannel1.end()) ? 1 : iter1->second;
        configEventSelections(aieDevInst, loc, XAIE_PL_MOD, type, metricSet, channel0, channel1);

        // Record for runtime config file
        cfgTile->interface_tile_trace_config.port_trace_ids[0] = channel0;
        cfgTile->interface_tile_trace_config.port_trace_ids[1] = channel1;
        if (metricSet.find("input") != std::string::npos) {
          cfgTile->interface_tile_trace_config.port_trace_is_master[0] = true;
          cfgTile->interface_tile_trace_config.port_trace_is_master[1] = true;
          cfgTile->interface_tile_trace_config.s2mm_channels[0] = channel0;
          if (channel0 != channel1)
            cfgTile->interface_tile_trace_config.s2mm_channels[1] = channel1;
        } else {
          cfgTile->interface_tile_trace_config.port_trace_is_master[0] = false;
          cfgTile->interface_tile_trace_config.port_trace_is_master[1] = false;
          cfgTile->interface_tile_trace_config.mm2s_channels[0] = channel0;
          if (channel0 != channel1)
            cfgTile->interface_tile_trace_config.mm2s_channels[1] = channel1;
        }

        // Configure interface tile trace events
        for (int i = 0; i < interfaceEvents.size(); i++) {
          auto TraceE = shim.traceEvent();
          TraceE->setEvent(XAIE_PL_MOD, interfaceEvents[i]);
          if (TraceE->reserve() != XAIE_OK)
            break;
          if (TraceE->start() != XAIE_OK)
            break;
          numInterfaceTraceEvents++;

          // Channel number is based on monitoring port 0 or 1
          auto channel = (interfaceEvents[i] <= XAIE_EVENT_PORT_TLAST_0_PL) ? channel0 : channel1;
          configStreamSwitchPorts(aieDevInst, tile, xaieTile, loc, type,
                                  interfaceEvents[i], i, metricSet, channel);

          // Update config file
          // Get Trace slot
          uint32_t S = 0;
          XAie_LocType L;
          XAie_ModuleType M;
          TraceE->getRscId(L, M, S);
          // Get Physical event
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, XAIE_PL_MOD, interfaceEvents[i], &phyEvent);
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

        mNumTileTraceEvents[typeInt][numInterfaceTraceEvents]++;
        
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

      if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(severity_level::debug)) {
        std::stringstream msg;
        msg << "Reserved " << numCoreTraceEvents << " core and " << numMemoryTraceEvents
            << " memory trace events for AIE tile (" << col << "," << row << "). Adding tile to static database.";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
      }

      // Add config info to static database
      // NOTE: Do not access cfgTile after this
      (db->getStaticInfo()).addAIECfgTile(deviceId, cfgTile);
    }  // For tiles

    // Report trace events reserved per tile
    printTraceEventStats(deviceId);
    return true;
  }  // end setMetricsSettings

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

  bool AieTrace_EdgeImpl::configureStartDelay(xaiefal::XAieMod& core)
  {
    if (!metadata->getDelay())
      return false;

    // This algorithm daisy chains counters to get an effective 64 bit delay
    // counterLow -> counterHigh -> trace start
    uint32_t delayCyclesHigh = 0;
    uint32_t delayCyclesLow = 0;
    XAie_ModuleType mod = XAIE_CORE_MOD;

    if (!metadata->getUseOneDelayCounter()) {
      // ceil(x/y) where x and y are  positive integers
      delayCyclesHigh = static_cast<uint32_t>(1 + ((metadata->getDelay() - 1) / std::numeric_limits<uint32_t>::max()));
      delayCyclesLow = static_cast<uint32_t>(metadata->getDelay() / delayCyclesHigh);
    } else {
      delayCyclesLow = static_cast<uint32_t>(metadata->getDelay());
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
    if (!metadata->getUseOneDelayCounter() && delayCyclesHigh) {
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

    if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(severity_level::debug)) {
      std::stringstream msg;
      msg << "Configuring delay : "
          << "mDelay : " << metadata->getDelay() << " "
          << "low : " << delayCyclesLow << " "
          << "high : " << delayCyclesHigh << " " << std::endl;
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    }

    mCoreTraceStartEvent = counterEvent;
    return true;
  }

  inline uint32_t AieTrace_EdgeImpl::bcIdToEvent(int bcId)
  {
    return bcId + CORE_BROADCAST_EVENT_BASE;
  }

  bool AieTrace_EdgeImpl::configureStartIteration(xaiefal::XAieMod& core)
  {
    XAie_ModuleType mod = XAIE_CORE_MOD;
    // Count up by 1 for every iteration
    auto pc = core.perfCounter();
    if (pc->initialize(mod, XAIE_EVENT_INSTR_EVENT_0_CORE, mod, XAIE_EVENT_INSTR_EVENT_0_CORE) != XAIE_OK)
      return false;
    if (pc->reserve() != XAIE_OK)
      return false;
    pc->changeThreshold(metadata->getIterationCount());
    XAie_Events counterEvent;
    pc->getCounterEvent(mod, counterEvent);
    // Reset when done counting
    pc->changeRstEvent(mod, counterEvent);
    if (pc->start() != XAIE_OK)
      return false;

    if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(severity_level::debug)) {
      std::stringstream msg;
      msg << "Configuring aie trace to start on iteration : " << metadata->getIterationCount();
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    }

    mCoreTraceStartEvent = counterEvent;
    return true;
  }

  void AieTrace_EdgeImpl::flushAieTileTraceModule()
  {
    if (mTraceFlushLocs.empty() && mMemoryTileTraceFlushLocs.empty())
      return;

    auto handle = metadata->getHandle();
    aieDevInst = static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle));

    if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(severity_level::info)) {
      std::stringstream msg;
      msg << "Flushing AIE trace by forcing end event for " << mTraceFlushLocs.size() << " AIE tiles";
      if (metadata->getHardwareGen() > 1)
        msg << " and " << mMemoryTileTraceFlushLocs.size() << " memory tiles";
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
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

  void AieTrace_EdgeImpl::pollTimers(uint32_t index, void* handle)
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
    auto moduleType    = getModuleType(tile.row, XAIE_CORE_MOD);
    auto falModuleType =  (moduleType == module_type::core) ? XAIE_CORE_MOD 
                       : ((moduleType == module_type::shim) ? XAIE_PL_MOD 
                       : XAIE_MEM_MOD);

    uint64_t timerValue = 0;  
    auto timestamp1 = xrt_core::time_ns();
    XAie_ReadTimer(aieDevInst, loc, falModuleType, &timerValue);
    auto timestamp2 = xrt_core::time_ns();
    
    std::vector<uint64_t> values;
    values.push_back(tile.col);
    values.push_back(getRelativeRow(tile.row));
    values.push_back(timerValue);

    db->getDynamicInfo().addAIETimerSample(index, timestamp1, timestamp2, values);
  }
}  // namespace xdp
