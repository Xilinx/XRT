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

#include "aie_trace.h"

#include <boost/algorithm/string.hpp>

#include "core/common/message.h"
#include "core/common/xrt_profiling.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_metadata.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

  AieTrace_WinImpl::AieTrace_WinImpl(VPDatabase* database, std::shared_ptr<AieTraceMetadata> metadata)
      : AieTraceImpl(database, metadata)
  {
    //
    // Pre-defined metric sets
    //
    // **** Core Module Trace ****
    coreEventSets = {
        {"functions", 
         {XAIE_EVENT_INSTR_CALL_CORE,                      XAIE_EVENT_INSTR_RETURN_CORE}},
        {"functions_partial_stalls", 
         {XAIE_EVENT_INSTR_CALL_CORE,                      XAIE_EVENT_INSTR_RETURN_CORE}},
        {"functions_all_stalls", 
         {XAIE_EVENT_INSTR_CALL_CORE,                      XAIE_EVENT_INSTR_RETURN_CORE}},
        {"all", 
         {XAIE_EVENT_INSTR_CALL_CORE,                      XAIE_EVENT_INSTR_RETURN_CORE}}
    };

    // These are also broadcast to memory module
    coreTraceStartEvent = XAIE_EVENT_ACTIVE_CORE;
    coreTraceEndEvent = XAIE_EVENT_DISABLED_CORE;

    // **** Memory Module Trace ****
    // NOTE 1: Core events listed here are broadcast by the resource manager 
    // NOTE 2: For now, 'all' is the same as 'functions_all_stalls'.
    //         Combo events (required for all) have limited support in resource manager.
    memoryEventSets = {
        {"functions", 
         {XAIE_EVENT_INSTR_CALL_CORE,                      XAIE_EVENT_INSTR_RETURN_CORE}},
        {"functions_partial_stalls",
         {XAIE_EVENT_INSTR_CALL_CORE,                      XAIE_EVENT_INSTR_RETURN_CORE, 
          XAIE_EVENT_STREAM_STALL_CORE,                    XAIE_EVENT_CASCADE_STALL_CORE, 
          XAIE_EVENT_LOCK_STALL_CORE}},
        {"functions_all_stalls",
         {XAIE_EVENT_INSTR_CALL_CORE,                      XAIE_EVENT_INSTR_RETURN_CORE, 
          XAIE_EVENT_MEMORY_STALL_CORE,                    XAIE_EVENT_STREAM_STALL_CORE, 
          XAIE_EVENT_CASCADE_STALL_CORE,                   XAIE_EVENT_LOCK_STALL_CORE}},
        {"all",
         {XAIE_EVENT_INSTR_CALL_CORE,                      XAIE_EVENT_INSTR_RETURN_CORE, 
          XAIE_EVENT_MEMORY_STALL_CORE,                    XAIE_EVENT_STREAM_STALL_CORE, 
          XAIE_EVENT_CASCADE_STALL_CORE,                   XAIE_EVENT_LOCK_STALL_CORE}}
    };

    // **** Memory Tile Trace ****
    memoryTileEventSets = {
        {"input_channels",
         {XAIE_EVENT_DMA_S2MM_SEL0_START_TASK_MEM_TILE,    XAIE_EVENT_DMA_S2MM_SEL1_START_TASK_MEM_TILE,
          XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_BD_MEM_TILE,   XAIE_EVENT_DMA_S2MM_SEL1_FINISHED_BD_MEM_TILE,
          XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_TASK_MEM_TILE, XAIE_EVENT_DMA_S2MM_SEL1_FINISHED_TASK_MEM_TILE}},
        {"input_channels_stalls",
         {XAIE_EVENT_DMA_S2MM_SEL0_START_TASK_MEM_TILE,    XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_BD_MEM_TILE,
          XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_TASK_MEM_TILE, XAIE_EVENT_DMA_S2MM_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE,
          XAIE_EVENT_EDGE_DETECTION_EVENT_0_MEM_TILE,      XAIE_EVENT_EDGE_DETECTION_EVENT_1_MEM_TILE, 
          XAIE_EVENT_DMA_S2MM_SEL0_MEMORY_BACKPRESSURE_MEM_TILE}},
        {"output_channels",
         {XAIE_EVENT_DMA_MM2S_SEL0_START_TASK_MEM_TILE,    XAIE_EVENT_DMA_MM2S_SEL1_START_TASK_MEM_TILE,
          XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_MEM_TILE,   XAIE_EVENT_DMA_MM2S_SEL1_FINISHED_BD_MEM_TILE,
          XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_TASK_MEM_TILE, XAIE_EVENT_DMA_MM2S_SEL1_FINISHED_TASK_MEM_TILE}},
        {"output_channels_stalls",
         {XAIE_EVENT_DMA_MM2S_SEL0_START_TASK_MEM_TILE,    XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_MEM_TILE,
          XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_TASK_MEM_TILE, XAIE_EVENT_EDGE_DETECTION_EVENT_0_MEM_TILE, 
          XAIE_EVENT_EDGE_DETECTION_EVENT_1_MEM_TILE,      XAIE_EVENT_DMA_MM2S_SEL0_STREAM_BACKPRESSURE_MEM_TILE, 
          XAIE_EVENT_DMA_MM2S_SEL0_MEMORY_STARVATION_MEM_TILE}},
        {"memory_conflicts1",         
         {XAIE_EVENT_CONFLICT_DM_BANK_0_MEM_TILE,          XAIE_EVENT_CONFLICT_DM_BANK_1_MEM_TILE,
          XAIE_EVENT_CONFLICT_DM_BANK_2_MEM_TILE,          XAIE_EVENT_CONFLICT_DM_BANK_3_MEM_TILE,
          XAIE_EVENT_CONFLICT_DM_BANK_4_MEM_TILE,          XAIE_EVENT_CONFLICT_DM_BANK_5_MEM_TILE,
          XAIE_EVENT_CONFLICT_DM_BANK_6_MEM_TILE,          XAIE_EVENT_CONFLICT_DM_BANK_7_MEM_TILE}},
        {"memory_conflicts2",         
         {XAIE_EVENT_CONFLICT_DM_BANK_8_MEM_TILE,          XAIE_EVENT_CONFLICT_DM_BANK_9_MEM_TILE,
          XAIE_EVENT_CONFLICT_DM_BANK_10_MEM_TILE,         XAIE_EVENT_CONFLICT_DM_BANK_11_MEM_TILE,
          XAIE_EVENT_CONFLICT_DM_BANK_12_MEM_TILE,         XAIE_EVENT_CONFLICT_DM_BANK_13_MEM_TILE,
          XAIE_EVENT_CONFLICT_DM_BANK_14_MEM_TILE,         XAIE_EVENT_CONFLICT_DM_BANK_15_MEM_TILE}} 
    };
    memoryTileEventSets["s2mm_channels"]        = memoryTileEventSets["input_channels"];
    memoryTileEventSets["s2mm_channels_stalls"] = memoryTileEventSets["input_channels_stalls"];
    memoryTileEventSets["mm2s_channels"]        = memoryTileEventSets["output_channels"];
    memoryTileEventSets["mm2s_channels_stalls"] = memoryTileEventSets["output_channels_stalls"];

    // Memory tile trace is flushed at end of run
    memoryTileTraceStartEvent = XAIE_EVENT_TRUE_MEM_TILE;
    memoryTileTraceEndEvent = XAIE_EVENT_USER_EVENT_1_MEM_TILE;

    // **** Interface Tile Trace ****
    interfaceTileEventSets = {
        {"input_ports",
         {XAIE_EVENT_PORT_RUNNING_0_PL,                    XAIE_EVENT_PORT_RUNNING_1_PL,
          XAIE_EVENT_PORT_RUNNING_2_PL,                    XAIE_EVENT_PORT_RUNNING_3_PL}},
        {"output_ports",
         {XAIE_EVENT_PORT_RUNNING_0_PL,                    XAIE_EVENT_PORT_RUNNING_1_PL,
          XAIE_EVENT_PORT_RUNNING_2_PL,                    XAIE_EVENT_PORT_RUNNING_3_PL}},
        {"input_ports_stalls",
         {XAIE_EVENT_PORT_RUNNING_0_PL,                    XAIE_EVENT_PORT_STALLED_0_PL,
          XAIE_EVENT_PORT_RUNNING_1_PL,                    XAIE_EVENT_PORT_STALLED_1_PL}},
        {"output_ports_stalls",
        {XAIE_EVENT_PORT_RUNNING_0_PL,                     XAIE_EVENT_PORT_STALLED_0_PL,
         XAIE_EVENT_PORT_RUNNING_1_PL,                     XAIE_EVENT_PORT_STALLED_1_PL}},
        {"input_ports_details",
        {XAIE_EVENT_DMA_MM2S_0_START_TASK_PL,              XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_PL,
         XAIE_EVENT_DMA_MM2S_0_FINISHED_TASK_PL,           XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_PL,
         XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_PL,     XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_PL}},
        {"output_ports_details",
        {XAIE_EVENT_DMA_S2MM_0_START_TASK_PL,              XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_PL,
         XAIE_EVENT_DMA_S2MM_0_FINISHED_TASK_PL,           XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_PL,
         XAIE_EVENT_DMA_S2MM_0_STREAM_STARVATION_PL,       XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL}}
    };
    interfaceTileEventSets["mm2s_ports"]           = interfaceTileEventSets["input_ports"];
    interfaceTileEventSets["s2mm_ports"]           = interfaceTileEventSets["output_ports"];
    interfaceTileEventSets["mm2s_ports_stalls"]    = interfaceTileEventSets["input_ports_stalls"];
    interfaceTileEventSets["s2mm_ports_stalls"]    = interfaceTileEventSets["output_ports_stalls"];
    interfaceTileEventSets["mm2s_ports_details"]   = interfaceTileEventSets["input_ports_details"];
    interfaceTileEventSets["s2mm_ports_details"]   = interfaceTileEventSets["output_ports_details"];

    // Interface tile trace is flushed at end of run
    interfaceTileTraceStartEvent = XAIE_EVENT_TRUE_PL;
    interfaceTileTraceEndEvent = XAIE_EVENT_USER_EVENT_1_PL;

    xdp::aie::driver_config meta_config = metadata->getAIEConfigMetadata();

    XAie_Config cfg {
      meta_config.hw_gen,
      meta_config.base_address,
      meta_config.column_shift,
      meta_config.row_shift,
      meta_config.num_rows,
      meta_config.num_columns,
      meta_config.shim_row,
      meta_config.mem_row_start,
      meta_config.mem_num_rows,
      meta_config.aie_tile_row_start,
      meta_config.aie_tile_num_rows,
      {0} // PartProp
    };

    auto RC = XAie_CfgInitialize(&aieDevInst, &cfg);
    if (RC != XAIE_OK)
      xrt_core::message::send(severity_level::warning, "XRT", "AIE Driver Initialization Failed.");
    

    auto context = metadata->getHwContext();
    transactionHandler = std::make_unique<aie::ClientTransaction>(context, "AIE Trace Setup");
  }

  void AieTrace_WinImpl::updateDevice()
  {
    xrt_core::message::send(severity_level::info, "XRT", "Calling AIE Trace IPU updateDevice.");

    // compile-time trace
    if (!metadata->getRuntimeMetrics())
      return;

    // Set metrics for trace events
    if (!setMetricsSettings(metadata->getDeviceID(), metadata->getHandle())) {
      std::string msg("Unable to configure AIE trace control and events. No trace will be generated.");
      xrt_core::message::send(severity_level::warning, "XRT", msg);
      return;
    }
  }

  // No CMA checks on Win
  uint64_t AieTrace_WinImpl::checkTraceBufSize(uint64_t size)
  {
    return size;
  }

  /****************************************************************************
   * Modify events in metric set based on type and channel
   ***************************************************************************/
  void AieTrace_WinImpl::modifyEvents(module_type type, uint16_t subtype, 
                                      const std::string metricSet, uint8_t channel, 
                                      std::vector<XAie_Events>& events)
  {
    // Only needed for GMIO DMA channel 1
    if ((type != module_type::shim) || (subtype == 0) || (channel == 0))
      return;

    // Check type to minimize replacements
    if (isInputSet(type, metricSet)) {
      // Input or MM2S
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_MM2S_0_START_TASK_PL,          XAIE_EVENT_DMA_MM2S_1_START_TASK_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_PL,         XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_MM2S_0_FINISHED_TASK_PL,       XAIE_EVENT_DMA_MM2S_1_FINISHED_TASK_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_PL,        XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_PL, XAIE_EVENT_DMA_MM2S_1_STREAM_BACKPRESSURE_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_PL,   XAIE_EVENT_DMA_MM2S_1_MEMORY_STARVATION_PL);
    }
    else {
      // Output or S2MM
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_S2MM_0_START_TASK_PL,          XAIE_EVENT_DMA_S2MM_1_START_TASK_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_PL,         XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_S2MM_0_FINISHED_TASK_PL,       XAIE_EVENT_DMA_S2MM_1_FINISHED_TASK_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_PL,        XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_S2MM_0_STREAM_STARVATION_PL,   XAIE_EVENT_DMA_S2MM_1_STREAM_STARVATION_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL, XAIE_EVENT_DMA_S2MM_1_MEMORY_BACKPRESSURE_PL);
    }
  }

  void AieTrace_WinImpl::flushTraceModules()
  {
    if (traceFlushLocs.empty() && memoryTileTraceFlushLocs.empty() 
        && interfaceTileTraceFlushLocs.empty())
      return;

    if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(severity_level::info)) {
      std::stringstream msg;
      msg << "Flushing AIE trace by forcing end event for " << traceFlushLocs.size()
          << " AIE tiles, " << memoryTileTraceFlushLocs.size() << " memory tiles, and " 
          << interfaceTileTraceFlushLocs.size() << " interface tiles.";
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
    }

    // Start recording the transaction
    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);

    // Flush trace by forcing end event
    // NOTE: this informs tiles to output remaining packets (even if partial)
    for (const auto& loc : traceFlushLocs) 
      XAie_EventGenerate(&aieDevInst, loc, XAIE_CORE_MOD, coreTraceEndEvent);
    for (const auto& loc : memoryTileTraceFlushLocs)
      XAie_EventGenerate(&aieDevInst, loc, XAIE_MEM_MOD, memoryTileTraceEndEvent);
    for (const auto& loc : interfaceTileTraceFlushLocs)
      XAie_EventGenerate(&aieDevInst, loc, XAIE_PL_MOD, interfaceTileTraceEndEvent);

    traceFlushLocs.clear();
    memoryTileTraceFlushLocs.clear();
    interfaceTileTraceFlushLocs.clear();

    uint8_t *txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);
    
    transactionHandler->setTransactionName("AIE Trace Flush");
    if (!transactionHandler->submitTransaction(txn_ptr))
      return;

    XAie_ClearTransaction(&aieDevInst);
    xrt_core::message::send(severity_level::info, "XRT", "Successfully scheduled AIE trace flush transaction.");
  }

  void AieTrace_WinImpl::pollTimers(uint64_t index, void* handle) 
  {
    // TODO: Poll timers (needed for system timeline only)
    (void)index;
    (void)handle;
  }

  uint16_t AieTrace_WinImpl::getRelativeRow(uint16_t absRow)
  {
    auto rowOffset = metadata->getRowOffset();
    if (absRow == 0)
      return 0;
    if (absRow < rowOffset)
      return (absRow - 1);
    return (absRow - rowOffset);
  }

  module_type AieTrace_WinImpl::getTileType(uint16_t absRow)
  {
    if (absRow == 0)
      return module_type::shim;
    if (absRow < metadata->getRowOffset())
      return module_type::mem_tile;
    return module_type::core;
  }

  void AieTrace_WinImpl::freeResources() 
  {
    // Nothing to do
  }

  inline uint32_t AieTrace_WinImpl::bcIdToEvent(int bcId)
  {
    return bcId + CORE_BROADCAST_EVENT_BASE;
  }

  bool AieTrace_WinImpl::isInputSet(const module_type type, const std::string metricSet)
  {
    // Catch memory tile sets
    if (type == module_type::mem_tile) {
      if ((metricSet.find("input") != std::string::npos)
          || (metricSet.find("s2mm") != std::string::npos))
        return true;
      else
        return false;
    }

    // Remaining covers interface tiles
    if ((metricSet.find("input") != std::string::npos)
        || (metricSet.find("mm2s") != std::string::npos))
      return true;
    else
      return false;
  }

  bool AieTrace_WinImpl::isStreamSwitchPortEvent(const XAie_Events event)
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

  bool AieTrace_WinImpl::isPortRunningEvent(const XAie_Events event)
  {
    std::set<XAie_Events> runningEvents = {
      XAIE_EVENT_PORT_RUNNING_0_CORE,     XAIE_EVENT_PORT_RUNNING_1_CORE,
      XAIE_EVENT_PORT_RUNNING_2_CORE,     XAIE_EVENT_PORT_RUNNING_3_CORE,
      XAIE_EVENT_PORT_RUNNING_4_CORE,     XAIE_EVENT_PORT_RUNNING_5_CORE,
      XAIE_EVENT_PORT_RUNNING_6_CORE,     XAIE_EVENT_PORT_RUNNING_7_CORE,
      XAIE_EVENT_PORT_RUNNING_0_PL,       XAIE_EVENT_PORT_RUNNING_1_PL,
      XAIE_EVENT_PORT_RUNNING_2_PL,       XAIE_EVENT_PORT_RUNNING_3_PL,
      XAIE_EVENT_PORT_RUNNING_4_PL,       XAIE_EVENT_PORT_RUNNING_5_PL,
      XAIE_EVENT_PORT_RUNNING_6_PL,       XAIE_EVENT_PORT_RUNNING_7_PL,
      XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, XAIE_EVENT_PORT_RUNNING_1_MEM_TILE,
      XAIE_EVENT_PORT_RUNNING_2_MEM_TILE, XAIE_EVENT_PORT_RUNNING_3_MEM_TILE,
      XAIE_EVENT_PORT_RUNNING_4_MEM_TILE, XAIE_EVENT_PORT_RUNNING_5_MEM_TILE,
      XAIE_EVENT_PORT_RUNNING_6_MEM_TILE, XAIE_EVENT_PORT_RUNNING_7_MEM_TILE
    };

    return (runningEvents.find(event) != runningEvents.end());
  }

  uint8_t AieTrace_WinImpl::getPortNumberFromEvent(XAie_Events event)
  {
    switch (event) {
    case XAIE_EVENT_PORT_RUNNING_3_PL:
    case XAIE_EVENT_PORT_STALLED_3_PL:
      return 3;
    case XAIE_EVENT_PORT_RUNNING_2_PL:
    case XAIE_EVENT_PORT_STALLED_2_PL:
      return 2;
    case XAIE_EVENT_PORT_RUNNING_1_PL:
    case XAIE_EVENT_PORT_STALLED_1_PL:
      return 1;
    default:
      return 0;
    }
  }

  // Configure stream switch ports for monitoring purposes
  // NOTE: Used to monitor streams: trace, interfaces, and memory tiles
  void
  AieTrace_WinImpl::configStreamSwitchPorts(const tile_type& tile,
                                            /*xaiefal::XAieTile& xaieTile,*/ const XAie_LocType loc,
                                            const module_type type, const std::string metricSet,
                                            const uint8_t channel0, const uint8_t channel1, 
                                            std::vector<XAie_Events>& events)
  {
    std::set<uint8_t> portSet;
    //std::map<uint8_t, std::shared_ptr<xaiefal::XAieStreamPortSelect>> switchPortMap;

    // Traverse all counters and request monitor ports as needed
    for (int i=0; i < events.size(); ++i) {
      // Ensure applicable event
      auto event = events.at(i);
      if (!isStreamSwitchPortEvent(event))
        continue;

      //bool newPort = false;
      auto portnum = getPortNumberFromEvent(event);

      // New port needed: reserver, configure, and store
      //if (switchPortMap.find(portnum) == switchPortMap.end()) {
      if (portSet.find(portnum) == portSet.end()) {
        portSet.insert(portnum);
        //auto switchPortRsc = xaieTile.sswitchPort();
        //if (switchPortRsc->reserve() != AieRC::XAIE_OK)
        //  continue;
        //newPort = true;
        //switchPortMap[portnum] = switchPortRsc;

        if (type == module_type::core) {
          // AIE Tiles (e.g., trace streams)
          // Define stream switch port to monitor core or memory trace
          uint8_t traceSelect = (event == XAIE_EVENT_PORT_RUNNING_0_CORE) ? 0 : 1;
          //switchPortRsc->setPortToSelect(XAIE_STRMSW_SLAVE, TRACE, traceSelect);
          XAie_EventSelectStrmPort(&aieDevInst, loc, 0, XAIE_STRMSW_SLAVE, TRACE, traceSelect);
      
        }
        else if (type == module_type::shim) {
          // Interface tiles (e.g., PLIO, GMIO)
          // Grab slave/master and stream ID
          auto slaveOrMaster = (tile.itr_mem_col == 0) ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
          auto streamPortId  = static_cast<uint8_t>(tile.itr_mem_row);
          //switchPortRsc->setPortToSelect(slaveOrMaster, SOUTH, streamPortId);
          XAie_EventSelectStrmPort(&aieDevInst, loc, 0, slaveOrMaster, SOUTH, streamPortId);
        }
        else {
          // Memory tiles
          if (metricSet.find("trace") != std::string::npos) {
            // switchPortRsc->setPortToSelect(XAIE_STRMSW_SLAVE, TRACE, 0);
          }
          else {
            uint8_t channel = (portnum == 0) ? channel0 : channel1;
            auto slaveOrMaster = isInputSet(type, metricSet) ? XAIE_STRMSW_MASTER : XAIE_STRMSW_SLAVE;
            //switchPortRsc->setPortToSelect(slaveOrMaster, DMA, channel);
            XAie_EventSelectStrmPort(&aieDevInst, loc, 0, slaveOrMaster, DMA, channel);
          }
        }
      }

      //auto switchPortRsc = switchPortMap[portnum];

      // Event options:
      //   getSSIdleEvent, getSSRunningEvent, getSSStalledEvent, & getSSTlastEvent
      // XAie_Events ssEvent;
      // if (isPortRunningEvent(event))
      //  switchPortRsc->getSSRunningEvent(ssEvent);
      // else
      //  switchPortRsc->getSSStalledEvent(ssEvent);
      // events.at(i) = ssEvent;

      // if (newPort) {
      //  switchPortRsc->start();
      //  streamPorts.push_back(switchPortRsc);
      // }
    }

    //switchPortMap.clear();
    portSet.clear();
  }
  
  void AieTrace_WinImpl::configEventSelections(const XAie_LocType loc, const module_type type,
                                               const std::string metricSet, const uint8_t channel0,
                                               const uint8_t channel1)
  {
    if (type != module_type::mem_tile)
      return;

    XAie_DmaDirection dmaDir = isInputSet(type, metricSet) ? DMA_S2MM : DMA_MM2S;
    XAie_EventSelectDmaChannel(&aieDevInst, loc, 0, dmaDir, channel0);
    XAie_EventSelectDmaChannel(&aieDevInst, loc, 1, dmaDir, channel1);
  }

  void AieTrace_WinImpl::configEdgeEvents(const tile_type& tile, const module_type type,
                                          const std::string metricSet, const XAie_Events event)
  {
    // For now, only memory tiles are supported
    if ((event != XAIE_EVENT_EDGE_DETECTION_EVENT_0_MEM_TILE)
        && (event != XAIE_EVENT_EDGE_DETECTION_EVENT_1_MEM_TILE))
      return;

    // AIE core register offsets
    constexpr uint64_t AIE_OFFSET_EDGE_CONTROL_MEM_TILE = 0x94408;

    // Event is DMA_S2MM_Sel0_stream_starvation or DMA_MM2S_Sel0_stalled_lock
    uint16_t eventNum = isInputSet(type, metricSet)
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

    auto tileOffset = _XAie_GetTileAddr(&aieDevInst, static_cast<uint8_t>(tile.row), static_cast<uint8_t>(tile.col));
    XAie_Write32(&aieDevInst, tileOffset + AIE_OFFSET_EDGE_CONTROL_MEM_TILE, edgeEventsValue);
  }

  bool AieTrace_WinImpl::setMetricsSettings(uint64_t deviceId, void* handle)
  {
    // Gather data to send to PS Kernel
    (void)deviceId;
    (void)handle;

    //Start recording the transaction
    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);

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
        coreTraceStartEvent = XAIE_EVENT_INSTR_EVENT_0_CORE;
      coreTraceEndEvent = XAIE_EVENT_INSTR_EVENT_1_CORE;
      useTraceFlush = true;

      if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(severity_level::info))
        xrt_core::message::send(severity_level::info, "XRT", "Enabling trace flush");
    }

    // Iterate over all used/specified tiles
    // NOTE: rows are stored as absolute as required by resource manager
    std::cout << "Config Metrics Size: " << metadata->getConfigMetrics().size() << std::endl;
    for (auto& tileMetric : metadata->getConfigMetrics()) {
      auto& metricSet = tileMetric.second;
      auto tile       = tileMetric.first;
      auto col        = tile.col;
      auto row        = tile.row;
      auto subtype    = tile.subtype;
      auto type       = getTileType(row);
      auto typeInt    = static_cast<int>(type);
      //auto& xaieTile  = aieDevice->tile(col, row);
      auto loc        = XAie_TileLoc(static_cast<uint8_t>(col), static_cast<uint8_t>(row));

      std::stringstream cmsg;
      cmsg << "Configuring tile (" << col << "," << row << ") in module type: " << typeInt << ".";
      xrt_core::message::send(severity_level::info, "XRT", cmsg.str());

      // xaiefal::XAieMod core;
      // xaiefal::XAieMod memory;
      // xaiefal::XAieMod shim;
      // if (type == module_type::core)
      //   core = xaieTile.core();
      // if (type == module_type::shim)
      //   shim = xaieTile.pl();
      // else
      //   memory = xaieTile.mem();

      // Store location to flush at end of run
      if (useTraceFlush || (type == module_type::mem_tile) 
          || (type == module_type::shim)) {
        if (type == module_type::core)
          traceFlushLocs.push_back(loc);
        else if (type == module_type::mem_tile)
          memoryTileTraceFlushLocs.push_back(loc);
        else if (type == module_type::shim)
          interfaceTileTraceFlushLocs.push_back(loc);
      }

      // AIE config object for this tile
      auto cfgTile = std::make_unique<aie_cfg_tile>(col, row, type);
      cfgTile->type = type;
      cfgTile->trace_metric_set = metricSet;

      // Get vector of pre-defined metrics for this set
      // NOTE: These are local copies to add tile-specific events
      EventVector coreEvents;
      EventVector memoryCrossEvents;
      EventVector memoryEvents;
      EventVector interfaceEvents;
      if (type == module_type::core) {
        coreEvents = coreEventSets[metricSet];
        memoryCrossEvents = memoryEventSets[metricSet];
      }
      else if (type == module_type::mem_tile) {
        memoryEvents = memoryTileEventSets[metricSet];
      }
      else if (type == module_type::shim) {
        interfaceEvents = interfaceTileEventSets[metricSet];
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
      // if (!tileHasFreeRsc(aieDevice, loc, type, metricSet)) {
      //   xrt_core::message::send(severity_level::warning, "XRT",
      //       "Tile doesn't have enough free resources for trace. Aborting trace configuration.");
      //   printTileStats(aieDevice, tile);
      //   return false;
      // }

      int numCoreTraceEvents = 0;
      int numMemoryTraceEvents = 0;
      int numInterfaceTraceEvents = 0;
      
      //
      // 1. Configure Core Trace Events
      //
      if (type == module_type::core) {
        xrt_core::message::send(severity_level::info, "XRT", "Configuring Core Trace Events");

        XAie_ModuleType mod = XAIE_CORE_MOD;
        uint8_t phyEvent = 0;
        //auto coreTrace = core.traceControl();

        // Delay cycles and user control are not compatible with each other
        // if (metadata->getUseGraphIterator()) {
        //   if (!configureStartIteration(core))
        //     break;
        // } else if (metadata->getUseDelay()) {
        //   if (!configureStartDelay(core))
        //     break;
        // }

        // Set overall start/end for trace capture
        // NOTE: This needs to be done first.
        //if (coreTrace->setCntrEvent(coreTraceStartEvent, coreTraceEndEvent) != XAIE_OK)
        //  break;
        //if (XAie_TraceStartEvent(&aieDevInst, loc, mod, coreTraceStartEvent) != XAIE_OK)
        //  break;
        if (XAie_TraceStopEvent(&aieDevInst, loc, mod, coreTraceEndEvent) != XAIE_OK)
          break;

        //auto ret = coreTrace->reserve();
        // if (ret != XAIE_OK) {
        //   std::stringstream msg;
        //   msg << "Unable to reserve core module trace control for AIE tile (" << col << "," << row << ").";
        //   xrt_core::message::send(severity_level::warning, "XRT", msg.str());

        //   freeResources();
        //   // Print resources availability for this tile
        //   printTileStats(aieDevice, tile);
        //   return false;
        // }

        for (uint8_t i = 0; i < coreEvents.size(); i++) {
          uint8_t slot = i;
          //if (coreTrace->reserveTraceSlot(slot) != XAIE_OK)
          //  break;
          //if (coreTrace->setTraceEvent(slot, coreEvents[i]) != XAIE_OK)
          //  break;
          if (XAie_TraceEvent(&aieDevInst, loc, mod, coreEvents[i], i) != XAIE_OK)
            break;
          numCoreTraceEvents++;

          // Update config file
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, mod, coreEvents[i], &phyEvent);
          cfgTile->core_trace_config.traced_events[slot] = phyEvent;
        }

        // Update config file
        XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, mod, coreTraceStartEvent, &phyEvent);
        cfgTile->core_trace_config.start_event = phyEvent;
        XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, mod, coreTraceEndEvent, &phyEvent);
        cfgTile->core_trace_config.stop_event = phyEvent;

        coreEvents.clear();
        mNumTileTraceEvents[typeInt][numCoreTraceEvents]++;

        //if (coreTrace->setMode(XAIE_TRACE_EVENT_PC) != XAIE_OK)
        //  break;
        XAie_Packet pkt = {0, 0};
        //if (coreTrace->setPkt(pkt) != XAIE_OK)
        //  break;
        //if (coreTrace->start() != XAIE_OK)
        //  break;
        if (XAie_TraceModeConfig(&aieDevInst, loc, mod, XAIE_TRACE_EVENT_PC) != XAIE_OK)
          break;
        if (XAie_TracePktConfig(&aieDevInst, loc, mod, pkt) != XAIE_OK)
          break;
        XAie_TraceStartEvent(&aieDevInst, loc, mod, coreTraceStartEvent);
      } // Core modules

      //
      // 2. Configure Memory Trace Events
      //
      // NOTE: this is applicable for memory modules in AIE tiles or memory tiles
      uint32_t coreToMemBcMask = 0;
      if ((type == module_type::core) || (type == module_type::mem_tile)) {
        xrt_core::message::send(severity_level::info, "XRT", "Configuring Memory Trace Events");

        XAie_ModuleType mod = XAIE_MEM_MOD;
        //auto memoryTrace = memory.traceControl();
        // Set overall start/end for trace capture
        // Wendy said this should be done first
        auto traceStartEvent = (type == module_type::core) ? coreTraceStartEvent : memoryTileTraceStartEvent;
        auto traceEndEvent = (type == module_type::core) ? coreTraceEndEvent : memoryTileTraceEndEvent;
        //if (memoryTrace->setCntrEvent(traceStartEvent, traceEndEvent) != XAIE_OK)
        //  break;

        if (type == module_type::core) {
          // Broadcast to memory module
          if (XAie_EventBroadcast(&aieDevInst, loc, XAIE_CORE_MOD, 8, traceStartEvent) != XAIE_OK)
            break;
          if (XAie_EventBroadcast(&aieDevInst, loc, XAIE_CORE_MOD, 9, traceEndEvent) != XAIE_OK)
            break;

          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, XAIE_CORE_MOD, traceStartEvent, &phyEvent);
          cfgTile->core_trace_config.internal_events_broadcast[8] = phyEvent;
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, XAIE_CORE_MOD, traceEndEvent, &phyEvent);
          cfgTile->core_trace_config.internal_events_broadcast[9] = phyEvent;

          // Only enable Core -> MEM. Block everything else in both modules
          if (XAie_EventBroadcastBlockMapDir(&aieDevInst, loc, XAIE_CORE_MOD, XAIE_EVENT_SWITCH_A, 0xFF00, XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH) != XAIE_OK)
            break;
          if (XAie_EventBroadcastBlockMapDir(&aieDevInst, loc, XAIE_MEM_MOD, XAIE_EVENT_SWITCH_A, 0xFF00, XAIE_EVENT_BROADCAST_EAST | XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH) != XAIE_OK)
            break;
          
          for (uint8_t i = 8; i < 16; i++)
            if (XAie_EventBroadcastUnblockDir(&aieDevInst, loc, XAIE_CORE_MOD, XAIE_EVENT_SWITCH_A, i, XAIE_EVENT_BROADCAST_EAST) != XAIE_OK)
              break;

          traceStartEvent = XAIE_EVENT_BROADCAST_8_MEM;
          traceEndEvent = XAIE_EVENT_BROADCAST_9_MEM;
        }

        mMemoryModTraceStartEvent = traceStartEvent;
        if (XAie_TraceStopEvent(&aieDevInst, loc, mod, traceEndEvent) != XAIE_OK)
          break;

        {
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, mod, traceStartEvent, &phyEvent);
          cfgTile->memory_trace_config.start_event = phyEvent;
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, mod, traceEndEvent, &phyEvent);
          cfgTile->memory_trace_config.stop_event = phyEvent;
        }

        // auto ret = memoryTrace->reserve();
        // if (ret != XAIE_OK) {
        //   std::stringstream msg;
        //   msg << "Unable to reserve memory trace control for AIE tile (" << col << "," << row << ").";
        //   xrt_core::message::send(severity_level::warning, "XRT", msg.str());

        //   freeResources();
        //   // Print resources availability for this tile
        //   printTileStats(aieDevice, tile);
        //   return false;
        // }

        // Specify Sel0/Sel1 for memory tile events 21-44
        if (type == module_type::mem_tile) {
          auto iter0 = configChannel0.find(tile);
          auto iter1 = configChannel1.find(tile);
          uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;
          uint8_t channel1 = (iter1 == configChannel1.end()) ? 1 : iter1->second;
          configEventSelections(loc, type, metricSet, channel0, channel1);

          // Record for runtime config file
          cfgTile->memory_tile_trace_config.port_trace_ids[0] = channel0;
          cfgTile->memory_tile_trace_config.port_trace_ids[1] = channel1;
          if (isInputSet(type, metricSet)) {
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

        xrt_core::message::send(severity_level::info, "XRT", "Configuring Cross Module Events");

        // Configure cross module events
        // NOTE: this is only applicable for memory modules, not memory tiles
        // uint8_t firstBroadcastId = 16 - static_cast<uint8_t>(memoryCrossEvents.size());
        uint8_t firstBroadcastId = 10;
        std::vector<XAie_Events> broadcastEvents = {
          XAIE_EVENT_BROADCAST_10_MEM,
          XAIE_EVENT_BROADCAST_11_MEM,
          XAIE_EVENT_BROADCAST_12_MEM,
          XAIE_EVENT_BROADCAST_13_MEM,
          XAIE_EVENT_BROADCAST_14_MEM,
          XAIE_EVENT_BROADCAST_15_MEM
        };
        for (int i = 0; i < memoryCrossEvents.size(); i++) {
          uint32_t bcBit = 0x1;
          // TODO: Is this correct?
          int bcId = firstBroadcastId + i;
          // auto event = bcIdToEvent(bcId);
          //auto TraceE = memory.traceEvent();
          //TraceE->setEvent(XAIE_CORE_MOD, memoryCrossEvents[i]);
          //if (TraceE->reserve() != XAIE_OK)
          //  break;
          if (XAie_EventBroadcast(&aieDevInst, loc, XAIE_CORE_MOD,  static_cast<uint8_t>(bcId) , memoryCrossEvents[i]) != XAIE_OK)
            break;
          if (XAie_TraceEvent(&aieDevInst, loc, mod, broadcastEvents[i], static_cast<uint8_t>(i)) != XAIE_OK)
            break;
          
          
          //int bcId = TraceE->getBc();
          coreToMemBcMask |= (bcBit << bcId);

          //if (TraceE->start() != XAIE_OK)
          //  break;
          numMemoryTraceEvents++;

          // Update config file with physical events
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, XAIE_CORE_MOD, memoryCrossEvents[i], &phyEvent);
          cfgTile->core_trace_config.internal_events_broadcast[bcId] = phyEvent;
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, XAIE_MEM_MOD, broadcastEvents[i], &phyEvent);
          cfgTile->memory_trace_config.traced_events[i] = phyEvent;
        }
        auto numCrossEvents = numMemoryTraceEvents;
        xrt_core::message::send(severity_level::info, "XRT", "Configuring Memory Trace Events");

        // Configure memory trace events
        for (int i = 0; i < memoryEvents.size(); i++) {
          //auto TraceE = memory.traceEvent();
          //TraceE->setEvent(XAIE_MEM_MOD, memoryEvents[i]);
          //if (TraceE->reserve() != XAIE_OK)
          //  break;
          //if (TraceE->start() != XAIE_OK)
          //  break;
          if (XAie_TraceEvent(&aieDevInst, loc, mod, memoryEvents[i], static_cast<uint8_t>(numCrossEvents+i)) != XAIE_OK)
            break;
          numMemoryTraceEvents++;

          // Configure edge events (as needed)
          configEdgeEvents(tile, type, metricSet, memoryEvents[i]);

          // Update config file
          // Get Trace slot
          // uint32_t S = 0;
          // XAie_LocType L;
          // XAie_ModuleType M;
          // TraceE->getRscId(L, M, S);
          // Get Physical event
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, XAIE_MEM_MOD, memoryEvents[i], &phyEvent);

          if (type == module_type::mem_tile)
            cfgTile->memory_tile_trace_config.traced_events[i] = phyEvent;
          else
            cfgTile->memory_trace_config.traced_events[i] = phyEvent;
        }

        xrt_core::message::send(severity_level::info, "XRT", "Updating Config File");

        // Update config file
        /*{
          // Add Memory trace control events
          // Start
          uint32_t bcBit = 0x1;
          // TODO: figure out BC ID for start
          int bcId = 0;
          //auto bcId = memoryTrace->getStartBc();
          coreToMemBcMask |= (bcBit << bcId);
          uint8_t phyEvent = 0;
          if (type == module_type::mem_tile) {
            XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, XAIE_MEM_MOD, traceStartEvent, &phyEvent);
            cfgTile->memory_tile_trace_config.start_event = phyEvent;
          } else {
            XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, XAIE_CORE_MOD, coreTraceStartEvent, &phyEvent);
            cfgTile->memory_trace_config.start_event = bcIdToEvent(bcId);
            cfgTile->core_trace_config.internal_events_broadcast[bcId] = phyEvent;
          }
          // Stop
          bcBit = 0x1;
          // TODO: figure out BC ID for stop
          // int bcId = 1;
          //bcId = memoryTrace->getStopBc();
          coreToMemBcMask |= (bcBit << bcId);
          if (type == module_type::mem_tile) {
            XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, XAIE_MEM_MOD, traceEndEvent, &phyEvent);
            cfgTile->memory_tile_trace_config.stop_event = phyEvent;
          } else {
            XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, XAIE_CORE_MOD, coreTraceEndEvent, &phyEvent);
            cfgTile->memory_trace_config.stop_event = bcIdToEvent(bcId);
            cfgTile->core_trace_config.internal_events_broadcast[bcId] = phyEvent;

            // Odd absolute rows change east mask end even row change west mask
            if (row % 2)
              cfgTile->core_trace_config.broadcast_mask_east = coreToMemBcMask;
            else
              cfgTile->core_trace_config.broadcast_mask_west = coreToMemBcMask;
          }
        }*/

        memoryEvents.clear();
        mNumTileTraceEvents[typeInt][numMemoryTraceEvents]++;
        
        //if (memoryTrace->setMode(XAIE_TRACE_EVENT_TIME) != XAIE_OK)
        //  break;
        uint8_t packetType = (type == module_type::mem_tile) ? 3 : 1;
        XAie_Packet pkt = {0, packetType};
        //if (memoryTrace->setPkt(pkt) != XAIE_OK)
        //  break;
        //if (memoryTrace->start() != XAIE_OK)
        //  break;
        xrt_core::message::send(severity_level::info, "XRT", "Configuring Memory Trace Mode");

        // if (XAie_TraceModeConfig(&aieDevInst, loc, mod, XAIE_TRACE_EVENT_TIME) != XAIE_OK)
        //   break;
        if (XAie_TracePktConfig(&aieDevInst, loc, mod, pkt) != XAIE_OK)
          break;
        if (XAie_TraceStartEvent(&aieDevInst, loc, mod, traceStartEvent) != XAIE_OK)
          break;

        // Update memory packet type in config file
        if (type == module_type::mem_tile)
          cfgTile->memory_tile_trace_config.packet_type = packetType;
        else
          cfgTile->memory_trace_config.packet_type = packetType;
      } // Memory modules/tiles

      //
      // 3. Configure Interface Tile Trace Events
      //
      if (type == module_type::shim) {
        xrt_core::message::send(severity_level::info, "XRT", "Configuring Interface Tile Trace Events");

        XAie_ModuleType mod = XAIE_PL_MOD;
        //auto shimTrace = shim.traceControl();
        //if (shimTrace->setCntrEvent(interfaceTileTraceStartEvent, interfaceTileTraceEndEvent) != XAIE_OK)
        //  break;

        // auto ret = shimTrace->reserve();
        // if (ret != XAIE_OK) {
        //   std::stringstream msg;
        //   msg << "Unable to reserve trace control for interface tile (" << col << "," << row << ").";
        //   xrt_core::message::send(severity_level::warning, "XRT", msg.str());

        //   freeResources();
        //   // Print resources availability for this tile
        //   printTileStats(aieDevice, tile);
        //   return false;
        // }

        // Get specified channel numbers
        auto iter0 = configChannel0.find(tile);
        auto iter1 = configChannel1.find(tile);
        uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;
        uint8_t channel1 = (iter1 == configChannel1.end()) ? 1 : iter1->second;

        // Modify events as needed
        modifyEvents(type, subtype, metricSet, channel0, interfaceEvents);

        // Record for runtime config file
        // NOTE: input/output designations are different from memory tile channels
        cfgTile->interface_tile_trace_config.port_trace_ids[0] = channel0;
        cfgTile->interface_tile_trace_config.port_trace_ids[1] = channel1;
        if (isInputSet(type, metricSet)) {
          cfgTile->interface_tile_trace_config.port_trace_is_master[0] = false;
          cfgTile->interface_tile_trace_config.port_trace_is_master[1] = false;
          cfgTile->interface_tile_trace_config.mm2s_channels[0] = channel0;
          if (channel0 != channel1)
            cfgTile->interface_tile_trace_config.mm2s_channels[1] = channel1;
        } else {
          cfgTile->interface_tile_trace_config.port_trace_is_master[0] = true;
          cfgTile->interface_tile_trace_config.port_trace_is_master[1] = true;
          cfgTile->interface_tile_trace_config.s2mm_channels[0] = channel0;
          if (channel0 != channel1)
            cfgTile->interface_tile_trace_config.s2mm_channels[1] = channel1;
        }
        
        configStreamSwitchPorts(tileMetric.first, /*xaieTile,*/ loc, type,
                                metricSet, channel0, channel1, interfaceEvents);

        // Configure interface tile trace events
        for (int i = 0; i < interfaceEvents.size(); i++) {
          auto event = interfaceEvents.at(i);
          //auto TraceE = shim.traceEvent();
          //TraceE->setEvent(XAIE_PL_MOD, event);
          //if (TraceE->reserve() != XAIE_OK)
          //  break;
          //if (TraceE->start() != XAIE_OK)
          //  break;
          if (XAie_TraceEvent(&aieDevInst, loc, mod, event, static_cast<uint8_t>(i)) != XAIE_OK)
            break;
          numInterfaceTraceEvents++;

          // Update config file
          // Get Trace slot
          // uint32_t S = 0;
          // XAie_LocType L;
          // XAie_ModuleType M;
          // TraceE->getRscId(L, M, S);
          // Get Physical event
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, XAIE_PL_MOD, event, &phyEvent);
          cfgTile->interface_tile_trace_config.traced_events[i] = phyEvent;
        }

        // Update config file
        {
          // Add interface trace control events
          // Start
          uint8_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, XAIE_PL_MOD, interfaceTileTraceStartEvent, &phyEvent);
          cfgTile->interface_tile_trace_config.start_event = phyEvent;
          // Stop
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, XAIE_PL_MOD, interfaceTileTraceEndEvent, &phyEvent);
          cfgTile->interface_tile_trace_config.stop_event = phyEvent;
        }

        mNumTileTraceEvents[typeInt][numInterfaceTraceEvents]++;
        
        //if (shimTrace->setMode(XAIE_TRACE_EVENT_TIME) != XAIE_OK)
        //  break;
        uint8_t packetType = 4;
        XAie_Packet pkt = {0, packetType};
        //if (shimTrace->setPkt(pkt) != XAIE_OK)
        //  break;
        //if (shimTrace->start() != XAIE_OK)
        //  break;
        // if (XAie_TraceModeConfig(&aieDevInst, loc, mod, XAIE_TRACE_EVENT_TIME) != XAIE_OK)
        //   break;
        if (XAie_TracePktConfig(&aieDevInst, loc, mod, pkt) != XAIE_OK)
          break;
        if (XAie_TraceStartEvent(&aieDevInst, loc, mod, interfaceTileTraceStartEvent) != XAIE_OK)
          break;
        if (XAie_TraceStopEvent(&aieDevInst, loc, mod, interfaceTileTraceEndEvent) != XAIE_OK)
          break;
        cfgTile->interface_tile_trace_config.packet_type = packetType;
      } // Interface tiles

      if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(severity_level::debug)) {
        std::stringstream msg;
        msg << "Reserved ";
        if (type == module_type::core)
          msg << numCoreTraceEvents << " core and " << numMemoryTraceEvents << " memory";
        else if (type == module_type::mem_tile)
          msg << numMemoryTraceEvents << " memory tile";
        else if (type == module_type::shim)
          msg << numInterfaceTraceEvents << " interface tile";
        msg << " trace events for tile (" << col << "," << row 
            << "). Adding tile to static database.";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
      }

      // Add config info to static database
      // NOTE: Do not access cfgTile after this
      std::cout <<"log tile to device : " << deviceId << std::endl;
      (db->getStaticInfo()).addAIECfgTile(deviceId, cfgTile);
    }  // For tiles

    uint8_t *txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);

    if (!transactionHandler->initializeKernel("XDP_KERNEL"))
      return false;

    if (!transactionHandler->submitTransaction(txn_ptr))
      return false;

    xrt_core::message::send(severity_level::info, "XRT", "Successfully scheduled AIE Trace Transaction Buffer.");

    // Must clear aie state
    XAie_ClearTransaction(&aieDevInst);

    // Report trace events reserved per tile
    // printTraceEventStats(deviceId);
    xrt_core::message::send(severity_level::info, "XRT", "Finished AIE Trace IPU SetMetricsSettings.");
    
    return true;
  }

}  // namespace xdp
