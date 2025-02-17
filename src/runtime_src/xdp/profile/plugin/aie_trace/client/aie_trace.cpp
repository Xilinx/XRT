/**
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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
#include "resources_def.h"

#include <boost/algorithm/string.hpp>
#include <memory>

#include "core/common/message.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/device/pl_device_intf.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_metadata.h"
#include "xdp/profile/plugin/aie_trace/util/aie_trace_util.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

  AieTrace_WinImpl::AieTrace_WinImpl(VPDatabase* database, std::shared_ptr<AieTraceMetadata> metadata)
      : AieTraceImpl(database, metadata)
  {
    // Pre-defined metric sets
    auto hwGen = metadata->getHardwareGen();
    coreEventSets = aie::trace::getCoreEventSets(hwGen);
    memoryEventSets = aie::trace::getMemoryEventSets(hwGen);
    memoryTileEventSets = aie::trace::getMemoryTileEventSets(hwGen);
    interfaceTileEventSets = aie::trace::getInterfaceTileEventSets(hwGen);
    
    m_trace_start_broadcast = xrt_core::config::get_aie_trace_settings_trace_start_broadcast();
    if (m_trace_start_broadcast) 
      coreTraceStartEvent = (XAie_Events) (XAIE_EVENT_BROADCAST_0_CORE + traceStartBroadcastChId1);
    else 
      coreTraceStartEvent = XAIE_EVENT_ACTIVE_CORE;
    // These are also broadcast to memory module
    coreTraceEndEvent = XAIE_EVENT_DISABLED_CORE;

    // Memory tile trace is flushed at end of run
    if (m_trace_start_broadcast)
      memoryTileTraceStartEvent = (XAie_Events) (XAIE_EVENT_BROADCAST_0_MEM_TILE + traceStartBroadcastChId1);
    else
      memoryTileTraceStartEvent = XAIE_EVENT_TRUE_MEM_TILE;
    memoryTileTraceEndEvent = XAIE_EVENT_USER_EVENT_1_MEM_TILE;

    // Interface tile trace is flushed at end of run
    if(m_trace_start_broadcast)
      interfaceTileTraceStartEvent = (XAie_Events) (XAIE_EVENT_BROADCAST_A_0_PL + traceStartBroadcastChId2);
    else
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


  void AieTrace_WinImpl::build2ChannelBroadcastNetwork(void *hwCtxImpl, uint8_t broadcastId1, uint8_t broadcastId2, XAie_Events event) {

    boost::property_tree::ptree aiePartitionPt = xdp::aie::getAIEPartitionInfoClient(hwCtxImpl);
    // Currently, assuming only one Hw Context is alive at a time
    uint8_t startCol = static_cast<uint8_t>(aiePartitionPt.front().second.get<uint64_t>("start_col"));
    uint8_t numCols  = static_cast<uint8_t>(aiePartitionPt.front().second.get<uint64_t>("num_cols"));

    std::vector<uint8_t> maxRowAtCol(startCol + numCols, 0);
    for (auto& tileMetric : metadata->getConfigMetrics()) {
      auto tile       = tileMetric.first;
      auto col        = tile.col;
      auto row        = tile.row;
      maxRowAtCol[startCol + col] = std::max(maxRowAtCol[col], (uint8_t)row);
    }

    XAie_Events bcastEvent2_PL =  (XAie_Events) (XAIE_EVENT_BROADCAST_A_0_PL + broadcastId2);
    XAie_EventBroadcast(&aieDevInst, XAie_TileLoc(startCol, 0), XAIE_PL_MOD, broadcastId2, event);

    for(uint8_t col = startCol; col < startCol + numCols; col++) {
      for(uint8_t row = 0; row <= maxRowAtCol[col]; row++) {
        module_type tileType = getTileType(row);
        auto loc = XAie_TileLoc(col, row);

        if(tileType == module_type::shim) {
          // first channel is only used to send north
          if(col == startCol) {
            XAie_EventBroadcast(&aieDevInst, loc, XAIE_PL_MOD, broadcastId1, event);
          }
          else {
            XAie_EventBroadcast(&aieDevInst, loc, XAIE_PL_MOD, broadcastId1, bcastEvent2_PL);
          }
          if(maxRowAtCol[col] != row) {
            XAie_EventBroadcastBlockDir(&aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST);
          }
          else {
            XAie_EventBroadcastBlockDir(&aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST | XAIE_EVENT_BROADCAST_NORTH);
          }

          // second channel is only used to send east
          XAie_EventBroadcastBlockDir(&aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId2, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH);
          
          if(col != startCol + numCols - 1) {
            XAie_EventBroadcastBlockDir(&aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, broadcastId2, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH);
          }
          else {
            XAie_EventBroadcastBlockDir(&aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, broadcastId2, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_EAST);
          }
        }
        else if(tileType == module_type::mem_tile) {
          if(maxRowAtCol[col] != row) {
            XAie_EventBroadcastBlockDir(&aieDevInst, loc, XAIE_MEM_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST);
          }
          else {
            XAie_EventBroadcastBlockDir(&aieDevInst, loc, XAIE_MEM_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST | XAIE_EVENT_BROADCAST_NORTH);
          }
        }
        else { //core tile
          if(maxRowAtCol[col] != row) {
            XAie_EventBroadcastBlockDir(&aieDevInst, loc, XAIE_CORE_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST);
          }
          else {
            XAie_EventBroadcastBlockDir(&aieDevInst, loc, XAIE_CORE_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH);
          }
          XAie_EventBroadcastBlockDir(&aieDevInst, loc, XAIE_MEM_MOD,  XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST | XAIE_EVENT_BROADCAST_NORTH);
        }
      }
    }
  }

  void AieTrace_WinImpl::reset2ChannelBroadcastNetwork(void *hwCtxImpl, uint8_t broadcastId1, uint8_t broadcastId2) {

    boost::property_tree::ptree aiePartitionPt = xdp::aie::getAIEPartitionInfoClient(hwCtxImpl);
    // Currently, assuming only one Hw Context is alive at a time
    uint8_t startCol = static_cast<uint8_t>(aiePartitionPt.front().second.get<uint64_t>("start_col"));
    uint8_t numCols  = static_cast<uint8_t>(aiePartitionPt.front().second.get<uint64_t>("num_cols"));

    std::vector<uint8_t> maxRowAtCol(startCol + numCols, 0);
    for (auto& tileMetric : metadata->getConfigMetrics()) {
      auto tile       = tileMetric.first;
      auto col        = tile.col;
      auto row        = tile.row;
      maxRowAtCol[startCol + col] = std::max(maxRowAtCol[col], (uint8_t)row);
    }

    XAie_EventBroadcastReset(&aieDevInst, XAie_TileLoc(startCol, 0), XAIE_PL_MOD, broadcastId2);

    for(uint8_t col = startCol; col < startCol + numCols; col++) {
      for(uint8_t row = 0; row <= maxRowAtCol[col]; row++) {
        module_type tileType = getTileType(row);
        auto loc = XAie_TileLoc(col, row);

        if(tileType == module_type::shim) {
          XAie_EventBroadcastReset(&aieDevInst, loc, XAIE_PL_MOD, broadcastId1);
          XAie_EventBroadcastUnblockDir(&aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_ALL);
          XAie_EventBroadcastUnblockDir(&aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId2, XAIE_EVENT_BROADCAST_ALL);
          XAie_EventBroadcastUnblockDir(&aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, broadcastId2, XAIE_EVENT_BROADCAST_ALL);
        }
        else if(tileType == module_type::mem_tile) {
          XAie_EventBroadcastUnblockDir(&aieDevInst, loc, XAIE_MEM_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_ALL);
        }
        else { //core tile
          XAie_EventBroadcastUnblockDir(&aieDevInst, loc, XAIE_CORE_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_ALL);
          XAie_EventBroadcastUnblockDir(&aieDevInst, loc, XAIE_MEM_MOD,  XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_ALL);
        }
      }
    }
  }

  bool AieTrace_WinImpl::configureWindowedEventTrace(void* hwCtxImpl) {
    //Start recording the transaction
    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);

    boost::property_tree::ptree aiePartitionPt = xdp::aie::getAIEPartitionInfoClient(hwCtxImpl);
    // Currently, assuming only one Hw Context is alive at a time
    uint8_t startCol = static_cast<uint8_t>(aiePartitionPt.front().second.get<uint64_t>("start_col"));

    XAie_Events bcastEvent2_PL = (XAie_Events) (XAIE_EVENT_BROADCAST_A_0_PL + traceStartBroadcastChId2);
    XAie_Events shimTraceStartEvent = bcastEvent2_PL;
    XAie_Events memTileTraceStartEvent = (XAie_Events)(XAIE_EVENT_BROADCAST_0_MEM_TILE + traceStartBroadcastChId1);
    XAie_Events coreModTraceStartEvent = (XAie_Events)(XAIE_EVENT_BROADCAST_0_CORE + traceStartBroadcastChId1);
    XAie_Events memTraceStartEvent = (XAie_Events)(XAIE_EVENT_BROADCAST_0_MEM + traceStartBroadcastChId1);
    
    unsigned int startLayer = xrt_core::config::get_aie_trace_settings_start_layer();

    // NOTE: rows are stored as absolute as required by resource manager
    for (auto& tileMetric : metadata->getConfigMetrics()) {
      auto tile       = tileMetric.first;
      auto col        = tile.col;
      auto row        = tile.row;
      auto tileType   = getTileType(row);
      auto loc        = XAie_TileLoc(col, row);
      if(tileType == module_type::shim) {
        if(startLayer != UINT_MAX) {
          if(col == startCol) 
            XAie_TraceStartEvent(&aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_PERF_CNT_0_PL);
          else 
            XAie_TraceStartEvent(&aieDevInst, loc, XAIE_PL_MOD, shimTraceStartEvent);
        }
      }
      else if(tileType == module_type::mem_tile) {
        if(startLayer != UINT_MAX)
          XAie_TraceStartEvent(&aieDevInst, loc, XAIE_MEM_MOD, memTileTraceStartEvent);
      }
      else {
        if(startLayer != UINT_MAX) {
          XAie_TraceStartEvent(&aieDevInst, loc, XAIE_CORE_MOD, coreModTraceStartEvent);
          XAie_TraceStartEvent(&aieDevInst, loc, XAIE_MEM_MOD, memTraceStartEvent);
        }
      }
    }

    if(startLayer != UINT_MAX) {
      XAie_PerfCounterControlSet(&aieDevInst, XAie_TileLoc(startCol, 0), XAIE_PL_MOD, 0, XAIE_EVENT_USER_EVENT_0_PL, XAIE_EVENT_USER_EVENT_0_PL);
      XAie_PerfCounterEventValueSet(&aieDevInst, XAie_TileLoc(startCol, 0), XAIE_PL_MOD, 0, startLayer);
    }

    build2ChannelBroadcastNetwork(hwCtxImpl, traceStartBroadcastChId1, traceStartBroadcastChId2, XAIE_EVENT_PERF_CNT_0_PL);

    uint8_t *txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);

    if (!transactionHandler->initializeKernel("XDP_KERNEL"))
      return false;

    if (!transactionHandler->submitTransaction(txn_ptr))
      return false;
    
    // Must clear aie state
    XAie_ClearTransaction(&aieDevInst);

    xrt_core::message::send(severity_level::info, "XRT", "Finished AIE Winodwed Trace Settings. In client aie_trace.cpp");
    return true;
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
    if(xrt_core::config::get_aie_trace_settings_start_type() == "layer") {
      if (!configureWindowedEventTrace(metadata->getHandle())) {
        std::string msg("Unable to configure AIE Windowed event trace");
        xrt_core::message::send(severity_level::warning, "XRT", msg);
        return;
      }
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
  void AieTrace_WinImpl::modifyEvents(module_type type, io_type subtype, 
                                      const std::string metricSet, uint8_t channel, 
                                      std::vector<XAie_Events>& events)
  {
    // Only needed for GMIO DMA channel 1
    if ((type != module_type::shim) || (subtype == io_type::PLIO) || (channel == 0))
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

  module_type AieTrace_WinImpl::getTileType(uint8_t absRow)
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

  /****************************************************************************
   * Check if core module event
   ***************************************************************************/
  bool AieTrace_WinImpl::isCoreModuleEvent(const XAie_Events event)
  {
    return ((event >= XAIE_EVENT_NONE_CORE) 
            && (event <= XAIE_EVENT_INSTR_ERROR_CORE));
  }

  /****************************************************************************
   * Check if metric set contains DMA events
   * TODO: Traverse events vector instead of based on name
   ***************************************************************************/
  bool AieTrace_WinImpl::isDmaSet(const std::string metricSet)
  {
    if ((metricSet.find("dma") != std::string::npos)
        || (metricSet.find("s2mm") != std::string::npos)
        || (metricSet.find("mm2s") != std::string::npos))
      return true;
    return false;
  }
  
  /****************************************************************************
   * Get port number based on event
   ***************************************************************************/
  uint8_t AieTrace_WinImpl::getPortNumberFromEvent(XAie_Events event)
  {
    switch (event) {
    case XAIE_EVENT_PORT_RUNNING_7_CORE:
    case XAIE_EVENT_PORT_STALLED_7_CORE:
    case XAIE_EVENT_PORT_IDLE_7_CORE:
    case XAIE_EVENT_PORT_RUNNING_7_PL:
    case XAIE_EVENT_PORT_STALLED_7_PL:
    case XAIE_EVENT_PORT_IDLE_7_PL:
      return 7;
    case XAIE_EVENT_PORT_RUNNING_6_CORE:
    case XAIE_EVENT_PORT_STALLED_6_CORE:
    case XAIE_EVENT_PORT_IDLE_6_CORE:
    case XAIE_EVENT_PORT_RUNNING_6_PL:
    case XAIE_EVENT_PORT_STALLED_6_PL:
    case XAIE_EVENT_PORT_IDLE_6_PL:
      return 6;
    case XAIE_EVENT_PORT_RUNNING_5_CORE:
    case XAIE_EVENT_PORT_STALLED_5_CORE:
    case XAIE_EVENT_PORT_IDLE_5_CORE:
    case XAIE_EVENT_PORT_RUNNING_5_PL:
    case XAIE_EVENT_PORT_STALLED_5_PL:
    case XAIE_EVENT_PORT_IDLE_5_PL:
      return 5;
    case XAIE_EVENT_PORT_RUNNING_4_CORE:
    case XAIE_EVENT_PORT_STALLED_4_CORE:
    case XAIE_EVENT_PORT_IDLE_4_CORE:
    case XAIE_EVENT_PORT_RUNNING_4_PL:
    case XAIE_EVENT_PORT_STALLED_4_PL:
    case XAIE_EVENT_PORT_IDLE_4_PL:
      return 4;
    case XAIE_EVENT_PORT_RUNNING_3_CORE:
    case XAIE_EVENT_PORT_STALLED_3_CORE:
    case XAIE_EVENT_PORT_IDLE_3_CORE:
    case XAIE_EVENT_PORT_RUNNING_3_PL:
    case XAIE_EVENT_PORT_STALLED_3_PL:
    case XAIE_EVENT_PORT_IDLE_3_PL:
      return 3;
    case XAIE_EVENT_PORT_RUNNING_2_CORE:
    case XAIE_EVENT_PORT_STALLED_2_CORE:
    case XAIE_EVENT_PORT_IDLE_2_CORE:
    case XAIE_EVENT_PORT_RUNNING_2_PL:
    case XAIE_EVENT_PORT_STALLED_2_PL:
    case XAIE_EVENT_PORT_IDLE_2_PL:
      return 2;
    case XAIE_EVENT_PORT_RUNNING_1_CORE:
    case XAIE_EVENT_PORT_STALLED_1_CORE:
    case XAIE_EVENT_PORT_IDLE_1_CORE:
    case XAIE_EVENT_PORT_RUNNING_1_PL:
    case XAIE_EVENT_PORT_STALLED_1_PL:
    case XAIE_EVENT_PORT_IDLE_1_PL:
      return 1;
    default:
      return 0;
    }
  }

  /****************************************************************************
   * Get channel number based on event
   * NOTE: This only covers AIE Tiles and Interface Tiles
   ***************************************************************************/
  int8_t AieTrace_WinImpl::getChannelNumberFromEvent(XAie_Events event)
  {
    switch (event) {
    case XAIE_EVENT_DMA_S2MM_0_START_TASK_MEM:
    case XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM:
    case XAIE_EVENT_DMA_S2MM_0_FINISHED_TASK_MEM:
    case XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_MEM:
    case XAIE_EVENT_DMA_S2MM_0_STREAM_STARVATION_MEM:
    case XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_MEM:
    case XAIE_EVENT_DMA_MM2S_0_START_TASK_MEM:
    case XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM:
    case XAIE_EVENT_DMA_MM2S_0_FINISHED_TASK_MEM:
    case XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_MEM:
    case XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_MEM:
    case XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_MEM:
    case XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_PL:
    case XAIE_EVENT_DMA_S2MM_0_START_TASK_PL:
    case XAIE_EVENT_DMA_S2MM_0_FINISHED_TASK_PL:
    case XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_PL:
    case XAIE_EVENT_DMA_S2MM_0_STREAM_STARVATION_PL:
    case XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL:
    case XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_PL:
    case XAIE_EVENT_DMA_MM2S_0_START_TASK_PL:
    case XAIE_EVENT_DMA_MM2S_0_FINISHED_TASK_PL:
    case XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_PL:
    case XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_PL:
    case XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_PL:
      return 0;
    case XAIE_EVENT_DMA_S2MM_1_START_TASK_MEM:
    case XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM:
    case XAIE_EVENT_DMA_S2MM_1_FINISHED_TASK_MEM:
    case XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_MEM:
    case XAIE_EVENT_DMA_S2MM_1_STREAM_STARVATION_MEM:
    case XAIE_EVENT_DMA_S2MM_1_MEMORY_BACKPRESSURE_MEM:
    case XAIE_EVENT_DMA_MM2S_1_START_TASK_MEM:
    case XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM:
    case XAIE_EVENT_DMA_MM2S_1_FINISHED_TASK_MEM:
    case XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_MEM:
    case XAIE_EVENT_DMA_MM2S_1_STREAM_BACKPRESSURE_MEM:
    case XAIE_EVENT_DMA_MM2S_1_MEMORY_STARVATION_MEM:
    case XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_PL:
    case XAIE_EVENT_DMA_S2MM_1_START_TASK_PL:
    case XAIE_EVENT_DMA_S2MM_1_FINISHED_TASK_PL:
    case XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_PL:
    case XAIE_EVENT_DMA_S2MM_1_STREAM_STARVATION_PL:
    case XAIE_EVENT_DMA_S2MM_1_MEMORY_BACKPRESSURE_PL:
    case XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_PL:
    case XAIE_EVENT_DMA_MM2S_1_START_TASK_PL:
    case XAIE_EVENT_DMA_MM2S_1_FINISHED_TASK_PL:
    case XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_PL:
    case XAIE_EVENT_DMA_MM2S_1_STREAM_BACKPRESSURE_PL:
    case XAIE_EVENT_DMA_MM2S_1_MEMORY_STARVATION_PL:
      return 1;
    default:
      return -1;
    }
  }

  /****************************************************************************
   * Configure stream switch event ports for monitoring purposes
   ***************************************************************************/
  void
  AieTrace_WinImpl::configStreamSwitchPorts(const tile_type& tile, const XAie_LocType loc,
                                            const module_type type, const std::string metricSet,
                                            const uint8_t channel0, const uint8_t channel1, 
                                            std::vector<XAie_Events>& events, aie_cfg_base& config)
  {
    // For now, unused argument
    (void)tile;

    std::set<uint8_t> portSet;
    //std::map<uint8_t, std::shared_ptr<xaiefal::XAieStreamPortSelect>> switchPortMap;

    // Traverse all counters and request monitor ports as needed
    for (size_t i=0; i < events.size(); ++i) {
      // Ensure applicable event
      auto event = events.at(i);
      if (!isStreamSwitchPortEvent(event))
        continue;

      //bool newPort = false;
      auto portnum = getPortNumberFromEvent(event);
      uint8_t channelNum = portnum % 2;
      uint8_t channel = (channelNum == 0) ? channel0 : channel1;

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
          // AIE Tiles - Monitor DMA channels
          bool isMaster = ((portnum >= 2) || (metricSet.find("s2mm") != std::string::npos));
          auto slaveOrMaster = isMaster ? XAIE_STRMSW_MASTER : XAIE_STRMSW_SLAVE;
          std::string typeName = isMaster ? "S2MM" : "MM2S";
          std::string msg = "Configuring core module stream switch to monitor DMA " 
                          + typeName + " channel " + std::to_string(channelNum);
          xrt_core::message::send(severity_level::debug, "XRT", msg);
          //switchPortRsc->setPortToSelect(slaveOrMaster, DMA, channelNum);
          XAie_EventSelectStrmPort(&aieDevInst, loc, portnum, slaveOrMaster, DMA, channelNum);

          // Record for runtime config file
          // NOTE: channel info informs back-end there will be events on that channel
          config.port_trace_ids[portnum] = channelNum;
          config.port_trace_is_master[portnum] = isMaster;
          if (isMaster)
            config.s2mm_channels[channelNum] = channelNum;
          else
            config.mm2s_channels[channelNum] = channelNum;
        }
        else if (type == module_type::shim) {
          // Interface tiles (e.g., GMIO)
          // NOTE: skip configuration of extra ports for tile if stream_ids are not available.
          if (portnum >= tile.stream_ids.size())
            continue;

          auto slaveOrMaster   = (tile.is_master_vec.at(portnum) == 0)   ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
          uint8_t streamPortId = static_cast<uint8_t>(tile.stream_ids.at(portnum));
          std::string typeName = (tile.is_master_vec.at(portnum) == 0) ? "slave" : "master";

          std::string msg = "Configuring interface tile stream switch to monitor " 
                          + typeName + " port with stream ID of " + std::to_string(streamPortId);
          xrt_core::message::send(severity_level::debug, "XRT", msg);
          //switchPortRsc->setPortToSelect(slaveOrMaster, SOUTH, streamPortId);
          XAie_EventSelectStrmPort(&aieDevInst, loc, portnum, slaveOrMaster, SOUTH, streamPortId);

          // Record for runtime config file
          config.port_trace_ids[portnum] = channelNum;
          config.port_trace_is_master[portnum] = (tile.is_master_vec.at(portnum) != 0);
          
          if (tile.is_master_vec.at(portnum) == 0)
            config.mm2s_channels[channelNum] = channel; // Slave or Input Port
          else
            config.s2mm_channels[channelNum] = channel; // Master or Output Port
        }
        else {
          // Memory tiles
          auto slaveOrMaster = isInputSet(type, metricSet) ? XAIE_STRMSW_MASTER : XAIE_STRMSW_SLAVE;
          std::string typeName = (slaveOrMaster == XAIE_STRMSW_MASTER) ? "master" : "slave";
          std::string msg = "Configuring memory tile stream switch to monitor "
                          + typeName + " stream port " + std::to_string(channel);
          xrt_core::message::send(severity_level::debug, "XRT", msg);
          //switchPortRsc->setPortToSelect(slaveOrMaster, DMA, channel);
          XAie_EventSelectStrmPort(&aieDevInst, loc, portnum, slaveOrMaster, DMA, channel);

          // Record for runtime config file
          config.port_trace_ids[portnum] = channel;
          config.port_trace_is_master[portnum] = (slaveOrMaster == XAIE_STRMSW_MASTER);            
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
  
  /****************************************************************************
   * Configure combo events (AIE tiles only)
   ***************************************************************************/
  std::vector<XAie_Events>
  AieTrace_WinImpl::configComboEvents(const XAie_LocType loc, const XAie_ModuleType mod,
                                      const module_type type, const std::string metricSet,
                                      aie_cfg_base& config)
  {
    // Only needed for core/memory modules and metric sets that include DMA events
    if (!isDmaSet(metricSet) || ((type != module_type::core) && (type != module_type::dma)))
      return {};

    std::vector<XAie_Events> comboEvents;

    if (mod == XAIE_CORE_MOD) {
      //auto comboEvent = xaieTile.core().comboEvent(4);
      comboEvents.push_back(XAIE_EVENT_COMBO_EVENT_2_CORE);

      // Combo2 = Port_Idle_0 OR Port_Idle_1 OR Port_Idle_2 OR Port_Idle_3
      std::vector<XAie_Events> events = {XAIE_EVENT_PORT_IDLE_0_CORE,
          XAIE_EVENT_PORT_IDLE_1_CORE, XAIE_EVENT_PORT_IDLE_2_CORE,
          XAIE_EVENT_PORT_IDLE_3_CORE};
      std::vector<XAie_EventComboOps> opts = {XAIE_EVENT_COMBO_E1_OR_E2, 
          XAIE_EVENT_COMBO_E1_OR_E2, XAIE_EVENT_COMBO_E1_OR_E2};

      // Capture in config class to report later
      for (size_t i=0; i < NUM_COMBO_EVENT_CONTROL; ++i)
        config.combo_event_control[i] = 2;
      for (size_t i=0; i < events.size(); ++i) {
        uint16_t phyEvent = 0;
        XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, mod, events.at(i), &phyEvent);
        config.combo_event_input[i] = phyEvent;
      }

      // Set events and trigger on OR of events
      //comboEvent->setEvents(events, opts);
      XAie_EventComboConfig(&aieDevInst, loc, mod, XAIE_EVENT_COMBO0, opts[0], 
                            events[0], events[1]);
      XAie_EventComboConfig(&aieDevInst, loc, mod, XAIE_EVENT_COMBO1, opts[1], 
                            events[2], events[3]);
      XAie_EventComboConfig(&aieDevInst, loc, mod, XAIE_EVENT_COMBO2, opts[2], 
                            XAIE_EVENT_COMBO_EVENT_0_PL, XAIE_EVENT_COMBO_EVENT_1_PL);
      return comboEvents;
    }

    // Since we're tracing DMA events, start trace right away.
    // Specify user event 0 as trace end so we can flush after run.
    comboEvents.push_back(XAIE_EVENT_TRUE_MEM);
    comboEvents.push_back(XAIE_EVENT_USER_EVENT_0_MEM);
    return comboEvents;
  }

  /****************************************************************************
   * Configure group events (core modules only)
   ***************************************************************************/
  void AieTrace_WinImpl::configGroupEvents(const XAie_LocType loc, const XAie_ModuleType mod, 
                                           const module_type type, const std::string metricSet)
  {
    // Only needed for core module and metric sets that include DMA events
    if (!isDmaSet(metricSet) || (type != module_type::core))
      return;

    // Set masks for group events
    XAie_EventGroupControl(&aieDevInst, loc, mod, XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE, 
                           GROUP_CORE_FUNCTIONS_MASK);
    XAie_EventGroupControl(&aieDevInst, loc, mod, XAIE_EVENT_GROUP_CORE_STALL_CORE, 
                           GROUP_CORE_STALL_MASK);
    XAie_EventGroupControl(&aieDevInst, loc, mod, XAIE_EVENT_GROUP_STREAM_SWITCH_CORE, 
                           GROUP_STREAM_SWITCH_RUNNING_MASK);
  }

  /****************************************************************************
   * Configure event selection (memory tiles only)
   ***************************************************************************/
  void AieTrace_WinImpl::configEventSelections(const XAie_LocType loc, const module_type type,
                                               const std::string metricSet, const uint8_t channel0,
                                               const uint8_t channel1, aie_cfg_base& config)
  {
    if (type != module_type::mem_tile)
      return;

    XAie_DmaDirection dmaDir = isInputSet(type, metricSet) ? DMA_S2MM : DMA_MM2S;

    if (aie::isDebugVerbosity()) {
      std::string typeName = (dmaDir == DMA_S2MM) ? "S2MM" : "MM2S";
      std::string msg = "Configuring memory tile event selections to DMA " 
                      + typeName + " channels " + std::to_string(channel0) 
                      + " and " + std::to_string(channel1);
      xrt_core::message::send(severity_level::debug, "XRT", msg);
    }

    XAie_EventSelectDmaChannel(&aieDevInst, loc, 0, dmaDir, channel0);
    XAie_EventSelectDmaChannel(&aieDevInst, loc, 1, dmaDir, channel1);

    // Record for runtime config file
    config.port_trace_ids[0] = channel0;
    config.port_trace_ids[1] = channel1;
    if (aie::isInputSet(type, metricSet)) {
      config.port_trace_is_master[0] = true;
      config.port_trace_is_master[1] = true;
      config.s2mm_channels[0] = channel0;
      if (channel0 != channel1)
        config.s2mm_channels[1] = channel1;
    } 
    else {
      config.port_trace_is_master[0] = false;
      config.port_trace_is_master[1] = false;
      config.mm2s_channels[0] = channel0;
      if (channel0 != channel1)
        config.mm2s_channels[1] = channel1;
    }
  }

  /****************************************************************************
   * Configure edge detection events
   ***************************************************************************/
  void AieTrace_WinImpl::configEdgeEvents(const tile_type& tile, const module_type type,
                                          const std::string metricSet, const XAie_Events event,
                                          const uint8_t channel)
  {
    if ((event != XAIE_EVENT_EDGE_DETECTION_EVENT_0_MEM_TILE)
        && (event != XAIE_EVENT_EDGE_DETECTION_EVENT_1_MEM_TILE)
        && (event != XAIE_EVENT_EDGE_DETECTION_EVENT_0_MEM)
        && (event != XAIE_EVENT_EDGE_DETECTION_EVENT_1_MEM))
      return;

    // Catch memory tiles
    if (type == module_type::mem_tile) {
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

      xrt_core::message::send(severity_level::debug, "XRT",
          "Configuring memory tile edge events to detect rise and fall of event " 
          + std::to_string(eventNum));

      auto tileOffset = _XAie_GetTileAddr(&aieDevInst, tile.row, tile.col);
      XAie_Write32(&aieDevInst, tileOffset + AIE_OFFSET_EDGE_CONTROL_MEM_TILE, 
                   edgeEventsValue);
      return;
    }

    // Below is AIE tile support
    
    // Event is DMA_MM2S_stalled_lock or DMA_S2MM_stream_starvation
    // Event is DMA_S2MM_Sel0_stream_starvation or DMA_MM2S_Sel0_stalled_lock
    uint16_t eventNum = isInputSet(type, metricSet)
        ? ((channel == 0) ? EVENT_MEM_DMA_MM2S_0_STALLED_LOCK
                          : EVENT_MEM_DMA_MM2S_1_STALLED_LOCK)
        : ((channel == 0) ? EVENT_MEM_DMA_S2MM_0_STREAM_STARVATION
                          : EVENT_MEM_DMA_S2MM_1_STREAM_STARVATION);

    // Register Edge_Detection_event_control
    // 26    Event 1 triggered on falling edge
    // 25    Event 1 triggered on rising edge
    // 23:16 Input event for edge event 1
    // 10    Event 0 triggered on falling edge
    //  9    Event 0 triggered on rising edge
    //  7:0  Input event for edge event 0
    uint32_t edgeEventsValue = (1 << 26) + (eventNum << 16) + (1 << 9) + eventNum;

    xrt_core::message::send(severity_level::debug, "XRT", 
        "Configuring AIE tile edge events to detect rise and fall of event " 
        + std::to_string(eventNum));

    auto tileOffset = _XAie_GetTileAddr(&aieDevInst, tile.row, tile.col);
    XAie_Write32(&aieDevInst, tileOffset + AIE_OFFSET_EDGE_CONTROL_MEM, 
                 edgeEventsValue);
  }

  /****************************************************************************
   * Configure requested tiles with trace metrics and settings
   ***************************************************************************/
  bool AieTrace_WinImpl::setMetricsSettings(uint64_t deviceId, void* hwCtxImpl)
  {
    (void)deviceId;

    // Get partition columns
    boost::property_tree::ptree aiePartitionPt = xdp::aie::getAIEPartitionInfoClient(hwCtxImpl);
    // Currently, assuming only one Hw Context is alive at a time
    uint8_t startCol = static_cast<uint8_t>(aiePartitionPt.front().second.get<uint64_t>("start_col"));

    std::string startType = xrt_core::config::get_aie_trace_settings_start_type();
    unsigned int startLayer = xrt_core::config::get_aie_trace_settings_start_layer();
    
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
      for (size_t n = 0; n <= NUM_TRACE_EVENTS; ++n)
        mNumTileTraceEvents[m][n] = 0;
    }

    // Using user event for trace end to enable flushing
    // NOTE: Flush trace module always at the end because for some applications
    //       core might be running infinitely.
    if (metadata->getUseUserControl())
      coreTraceStartEvent = XAIE_EVENT_INSTR_EVENT_0_CORE;
    coreTraceEndEvent = XAIE_EVENT_INSTR_EVENT_1_CORE;

    // Iterate over all used/specified tiles
    // NOTE: rows are stored as absolute as required by resource manager
    //std::cout << "Config Metrics Size: " << metadata->getConfigMetrics().size() << std::endl;
    for (auto& tileMetric : metadata->getConfigMetrics()) {
      auto& metricSet = tileMetric.second;
      auto tile       = tileMetric.first;
      auto col        = tile.col;
      auto row        = tile.row;
      auto subtype    = tile.subtype;
      auto type       = getTileType(row);
      auto typeInt    = static_cast<int>(type);
      //auto& xaieTile  = aieDevice->tile(col, row);
      auto loc        = XAie_TileLoc(col, row);

      std::stringstream cmsg;
      cmsg << "Configuring tile (" << +col << "," << +row << ") in module type: " << aie::getModuleName(type) << ".";
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
      if (type == module_type::core || (type == module_type::mem_tile) 
          || (type == module_type::shim)) {
        if (type == module_type::core)
          traceFlushLocs.push_back(loc);
        else if (type == module_type::mem_tile)
          memoryTileTraceFlushLocs.push_back(loc);
        else if (type == module_type::shim)
          interfaceTileTraceFlushLocs.push_back(loc);
      }

      // AIE config object for this tile
      auto cfgTile = std::make_unique<aie_cfg_tile>(col+startCol, row, type);
      cfgTile->type = type;
      cfgTile->trace_metric_set = metricSet;
      cfgTile->active_core = tile.active_core;
      cfgTile->active_memory = tile.active_memory;

      // Catch core execution trace
      if ((type == module_type::core) && (metricSet == "execution")) {
        // Set start/end events, use execution packets, and start trace module 
        XAie_TraceStopEvent(&aieDevInst, loc, XAIE_CORE_MOD, coreTraceEndEvent);

        // Driver requires at least one, non-zero trace event
        XAie_TraceEvent(&aieDevInst, loc, XAIE_CORE_MOD, XAIE_EVENT_TRUE_CORE, 0);
        
        XAie_Packet pkt = {0, 0};
        XAie_TraceModeConfig(&aieDevInst, loc, XAIE_CORE_MOD, XAIE_TRACE_INST_EXEC);
        XAie_TracePktConfig(&aieDevInst, loc, XAIE_CORE_MOD, pkt);

        if(startType != "layer" || startLayer ==  UINT_MAX)
          XAie_TraceStartEvent(&aieDevInst, loc, XAIE_CORE_MOD, coreTraceStartEvent);
        (db->getStaticInfo()).addAIECfgTile(deviceId, cfgTile);
        continue;
      }

      // Get vector of pre-defined metrics for this set
      // NOTE: These are local copies to add tile-specific events
      EventVector coreEvents;
      EventVector memoryEvents;
      EventVector interfaceEvents;
      if (type == module_type::core) {
        coreEvents = coreEventSets[metricSet];
        memoryEvents = memoryEventSets[metricSet];
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
        infoMsg << "Configuring " << tileName << " tile (" << +col << "," 
                << +row << ") for trace using metric set " << metricSet;
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
        uint16_t phyEvent = 0;
        //auto coreTrace = core.traceControl();

        // Delay cycles and user control are not compatible with each other
        // if (metadata->getUseGraphIterator()) {
        //   if (!configureStartIteration(core))
        //     break;
        // } else if (metadata->getUseDelay()) {
        //   if (!configureStartDelay(core))
        //     break;
        // }

        // Configure combo & group events (e.g., to monitor DMA channels)
        auto comboEvents = configComboEvents(loc, mod, type, metricSet, cfgTile->core_trace_config);
        configGroupEvents(loc, mod, type, metricSet);

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
        if(startType != "layer" || startLayer ==  UINT_MAX)
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
        uint8_t firstBroadcastId = 8;

        //auto memoryTrace = memory.traceControl();
        // Set overall start/end for trace capture
        // Wendy said this should be done first
        auto traceStartEvent = (type == module_type::core) ? coreTraceStartEvent : memoryTileTraceStartEvent;
        auto traceEndEvent = (type == module_type::core) ? coreTraceEndEvent : memoryTileTraceEndEvent;
        //if (memoryTrace->setCntrEvent(traceStartEvent, traceEndEvent) != XAIE_OK)
        //  break;

        aie_cfg_base& aieConfig = cfgTile->core_trace_config;
        if (type == module_type::mem_tile)
          aieConfig = cfgTile->memory_tile_trace_config;

        // Configure combo events for metric sets that include DMA events        
        auto comboEvents = configComboEvents(loc, mod, type, metricSet, aieConfig);
        if (comboEvents.size() == 2) {
          traceStartEvent = comboEvents.at(0);
          traceEndEvent = comboEvents.at(1);
        }
        else if (type == module_type::core) {
          // Broadcast to memory module
          if(!m_trace_start_broadcast)
            if (XAie_EventBroadcast(&aieDevInst, loc, XAIE_CORE_MOD, 8, traceStartEvent) != XAIE_OK)
              break;
          if (XAie_EventBroadcast(&aieDevInst, loc, XAIE_CORE_MOD, 9, traceEndEvent) != XAIE_OK)
            break;

          uint16_t phyEvent = 0;
          if(!m_trace_start_broadcast) {
            XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, XAIE_CORE_MOD, traceStartEvent, &phyEvent);
            cfgTile->core_trace_config.internal_events_broadcast[8] = phyEvent;
          }
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

          if(m_trace_start_broadcast)
            traceStartEvent = (XAie_Events) (XAIE_EVENT_BROADCAST_0_MEM + traceStartBroadcastChId1);
          else
            traceStartEvent = XAIE_EVENT_BROADCAST_8_MEM;
          traceEndEvent = XAIE_EVENT_BROADCAST_9_MEM;
          firstBroadcastId = 10;
        }

        // Configure event ports on stream switch
        // NOTE: These are events from the core module stream switch
        //       outputted on the memory module trace stream. 
        configStreamSwitchPorts(tile, loc, type, metricSet, 0, 0, memoryEvents, aieConfig);
        
        memoryModTraceStartEvent = traceStartEvent;
        if (XAie_TraceStopEvent(&aieDevInst, loc, mod, traceEndEvent) != XAIE_OK)
          break;

        {
          uint16_t phyEvent1 = 0;
          uint16_t phyEvent2 = 0;
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, mod, traceStartEvent, &phyEvent1);
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, mod, traceEndEvent, &phyEvent2);
          if (type == module_type::core) {
            cfgTile->memory_trace_config.start_event = phyEvent1;
            cfgTile->memory_trace_config.stop_event = phyEvent2;
          } else {
            cfgTile->memory_tile_trace_config.start_event = phyEvent1;
            cfgTile->memory_tile_trace_config.stop_event = phyEvent2;
          }
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

        auto iter0 = configChannel0.find(tile);
        auto iter1 = configChannel1.find(tile);
        uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;
        uint8_t channel1 = (iter1 == configChannel1.end()) ? 1 : iter1->second;

        // Specify Sel0/Sel1 for memory tile events 21-44
        if (type == module_type::mem_tile) {
          configEventSelections(loc, type, metricSet, channel0, channel1, 
                                cfgTile->memory_tile_trace_config);
        }
        else {
          // Record if these are channel-specific events
          // NOTE: for now, check first event and assume single channel
          auto channelNum = getChannelNumberFromEvent(memoryEvents.at(0));
          if (channelNum >= 0) {
            if (aie::isInputSet(type, metricSet))
              cfgTile->core_trace_config.mm2s_channels[0] = channelNum;
            else
              cfgTile->core_trace_config.s2mm_channels[0] = channelNum;
          }
        }

        // For now, use hard-coded broadcast IDs for module cross events
        uint8_t bcId = firstBroadcastId;
        int bcIndex = (firstBroadcastId == 10) ? 2 : 0;
        std::vector<XAie_Events> broadcastEvents = {
          XAIE_EVENT_BROADCAST_8_MEM,
          XAIE_EVENT_BROADCAST_9_MEM,
          XAIE_EVENT_BROADCAST_10_MEM,
          XAIE_EVENT_BROADCAST_11_MEM,
          XAIE_EVENT_BROADCAST_12_MEM,
          XAIE_EVENT_BROADCAST_13_MEM,
          XAIE_EVENT_BROADCAST_14_MEM,
          XAIE_EVENT_BROADCAST_15_MEM
        };

        // Configure memory trace events
        for (uint8_t i = 0; i < memoryEvents.size(); i++) {
          bool isCoreEvent = isCoreModuleEvent(memoryEvents[i]);

          if (isCoreEvent) {
            if (XAie_EventBroadcast(&aieDevInst, loc, XAIE_CORE_MOD, bcId, memoryEvents[i]) != XAIE_OK)
              break;
            if (XAie_TraceEvent(&aieDevInst, loc, XAIE_MEM_MOD, broadcastEvents[bcIndex++], i) != XAIE_OK)
              break;
          
            coreToMemBcMask |= (0x1 << bcId);
          } 
          else {
            if (XAie_TraceEvent(&aieDevInst, loc, XAIE_MEM_MOD, memoryEvents[i], i) != XAIE_OK)
              break;
          }
          numMemoryTraceEvents++;

          // Configure edge events (as needed)
          configEdgeEvents(tile, type, metricSet, memoryEvents[i], channel0);

          // Update config file
          uint16_t phyEvent = 0;
          auto phyMod = isCoreEvent ? XAIE_CORE_MOD : XAIE_MEM_MOD;
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, phyMod, memoryEvents[i], &phyEvent);

          if (isCoreEvent) {
            cfgTile->core_trace_config.internal_events_broadcast[bcId] = phyEvent;
            cfgTile->memory_trace_config.traced_events[i] = bcIdToEvent(bcId);
            bcId++;
          }
          else if (type == module_type::mem_tile)
            cfgTile->memory_tile_trace_config.traced_events[i] = phyEvent;
          else
            cfgTile->memory_trace_config.traced_events[i] = phyEvent;
        }

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
        if(startType != "layer" || startLayer ==  UINT_MAX) {
          if (XAie_TraceStartEvent(&aieDevInst, loc, mod, traceStartEvent) != XAIE_OK)
            break;
        }

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
        
        configStreamSwitchPorts(tileMetric.first, loc, type, metricSet, channel0, channel1, 
                                interfaceEvents, cfgTile->interface_tile_trace_config);

        // Configure interface tile trace events
        for (size_t i = 0; i < interfaceEvents.size(); i++) {
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
          uint16_t phyEvent = 0;
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, XAIE_PL_MOD, event, &phyEvent);
          cfgTile->interface_tile_trace_config.traced_events[i] = phyEvent;
        }

        // Update config file
        {
          // Add interface trace control events
          // Start
          uint16_t phyEvent = 0;
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
        if(startType != "layer" || startLayer ==  UINT_MAX) {
          if (XAie_TraceStartEvent(&aieDevInst, loc, mod, interfaceTileTraceStartEvent) != XAIE_OK)
            break;
        }
        if (XAie_TraceStopEvent(&aieDevInst, loc, mod, interfaceTileTraceEndEvent) != XAIE_OK)
          break;
        cfgTile->interface_tile_trace_config.packet_type = packetType;
        auto channelNum = getChannelNumberFromEvent(interfaceEvents.at(0));
        if (channelNum >= 0) {
          if (aie::isInputSet(type, metricSet))
            cfgTile->interface_tile_trace_config.mm2s_channels[channelNum] = channelNum;
          else
            cfgTile->interface_tile_trace_config.s2mm_channels[channelNum] = channelNum;
        }
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
        msg << " trace events for tile (" << +col << "," << +row 
            << "). Adding tile to static database.";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
      }

      // Add config info to static database
      // NOTE: Do not access cfgTile after this
      //std::cout <<"log tile to device : " << deviceId << std::endl;
      (db->getStaticInfo()).addAIECfgTile(deviceId, cfgTile);
    }  // For tiles

    if(m_trace_start_broadcast) {
      build2ChannelBroadcastNetwork(hwCtxImpl, traceStartBroadcastChId1, traceStartBroadcastChId2, interfaceTileTraceStartEvent);
      XAie_EventGenerate(&aieDevInst, XAie_TileLoc(startCol, 0), XAIE_PL_MOD,  interfaceTileTraceStartEvent);
    }

    uint8_t *txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);

    if (!transactionHandler->initializeKernel("XDP_KERNEL"))
      return false;

    if (!transactionHandler->submitTransaction(txn_ptr))
      return false;

    xrt_core::message::send(severity_level::info, "XRT", "Successfully scheduled AIE Trace Transaction Buffer.");

    // Must clear aie state
    XAie_ClearTransaction(&aieDevInst);

    // Clearing the broadcast network used for trace start
    if(m_trace_start_broadcast) {
      XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);
      reset2ChannelBroadcastNetwork(hwCtxImpl, traceStartBroadcastChId1, traceStartBroadcastChId2);
      txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);
      if (!transactionHandler->initializeKernel("XDP_KERNEL"))
        return false;
      if (!transactionHandler->submitTransaction(txn_ptr))
        return false;
    }
    
    // Must clear aie state
    XAie_ClearTransaction(&aieDevInst);

    // Report trace events reserved per tile
    // printTraceEventStats(deviceId);
    xrt_core::message::send(severity_level::info, "XRT", "Finished AIE Trace IPU SetMetricsSettings.");
    
    return true;
  }

}  // namespace xdp
