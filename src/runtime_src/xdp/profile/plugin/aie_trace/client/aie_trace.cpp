// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved

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
#include "xdp/profile/plugin/aie_base/aie_base_util.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_metadata.h"
#include "xdp/profile/plugin/aie_trace/util/aie_trace_util.h"
#include "xdp/profile/plugin/vp_base/info.h"

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

    boost::property_tree::ptree aiePartitionPt = xdp::aie::getAIEPartitionInfo(hwCtxImpl);
    // Currently, assuming only one Hw Context is alive at a time
    uint8_t startCol = static_cast<uint8_t>(aiePartitionPt.back().second.get<uint64_t>("start_col"));
    uint8_t numCols  = static_cast<uint8_t>(aiePartitionPt.back().second.get<uint64_t>("num_cols"));

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

    boost::property_tree::ptree aiePartitionPt = xdp::aie::getAIEPartitionInfo(hwCtxImpl);
    // Currently, assuming only one Hw Context is alive at a time
    uint8_t startCol = static_cast<uint8_t>(aiePartitionPt.back().second.get<uint64_t>("start_col"));
    uint8_t numCols  = static_cast<uint8_t>(aiePartitionPt.back().second.get<uint64_t>("num_cols"));

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

    boost::property_tree::ptree aiePartitionPt = xdp::aie::getAIEPartitionInfo(hwCtxImpl);
    // Currently, assuming only one Hw Context is alive at a time
    uint8_t startCol = static_cast<uint8_t>(aiePartitionPt.back().second.get<uint64_t>("start_col"));

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

    // Make sure compiler trace option is available as runtime
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
    if (aie::isInputSet(type, metricSet)) {
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
    if (db->infoAvailable(xdp::info::ml_timeline)) {
      db->broadcast(VPDatabase::MessageType::READ_RECORD_TIMESTAMPS, nullptr);
      xrt_core::message::send(severity_level::debug, "XRT", "Done reading recorded timestamps.");
    }

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
      if (!xdp::aie::isStreamSwitchPortEvent(event))
        continue;

      //bool newPort = false;
      auto portnum = xdp::aie::getPortNumberFromEvent(event);
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
          config.port_trace_names[portnum] = tile.port_names.at(portnum);

          if (isMaster) {
            config.s2mm_channels[channelNum] = channelNum;
            config.s2mm_names[channelNum] = tile.s2mm_names.at(channelNum);
          }
          else {
            config.mm2s_channels[channelNum] = channelNum;
            config.mm2s_names[channelNum] = tile.mm2s_names.at(channelNum);
          }
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
          if (streamPortId < tile.port_names.size())
            config.port_trace_names[portnum] = tile.port_names.at(streamPortId);
          
          if (tile.is_master_vec.at(portnum) == 0) {
            config.mm2s_channels[channelNum] = channel; // Slave or Input Port
            if (channelNum < tile.mm2s_names.size())
              config.mm2s_names[channelNum] = tile.mm2s_names.at(channelNum);
          }
          else {
            config.s2mm_channels[channelNum] = channel; // Master or Output Port
            if (channelNum < tile.s2mm_names.size())
              config.s2mm_names[channelNum] = tile.s2mm_names.at(channelNum);
          }
        }
        else {
          // Memory tiles
          auto slaveOrMaster = aie::isInputSet(type, metricSet) ? XAIE_STRMSW_MASTER : XAIE_STRMSW_SLAVE;
          std::string typeName = (slaveOrMaster == XAIE_STRMSW_MASTER) ? "master" : "slave";
          std::string msg = "Configuring memory tile stream switch to monitor "
                          + typeName + " stream port " + std::to_string(channel);
          xrt_core::message::send(severity_level::debug, "XRT", msg);
          //switchPortRsc->setPortToSelect(slaveOrMaster, DMA, channel);
          XAie_EventSelectStrmPort(&aieDevInst, loc, portnum, slaveOrMaster, DMA, channel);

          // Record for runtime config file
          config.port_trace_ids[portnum] = channel;
          config.port_trace_is_master[portnum] = (slaveOrMaster == XAIE_STRMSW_MASTER);
          config.port_trace_names[portnum] = tile.port_names.at(portnum);
        }
      }
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
    if (!aie::isDmaSet(metricSet) || ((type != module_type::core) && (type != module_type::dma)))
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
   * Configure requested tiles with trace metrics and settings
   ***************************************************************************/
  bool AieTrace_WinImpl::setMetricsSettings(uint64_t deviceId, void* hwCtxImpl)
  {
    (void)deviceId;

    // Get partition columns
    boost::property_tree::ptree aiePartitionPt = xdp::aie::getAIEPartitionInfo(hwCtxImpl);
    // Currently, assuming only one Hw Context is alive at a time
    uint8_t startCol = static_cast<uint8_t>(aiePartitionPt.back().second.get<uint64_t>("start_col"));

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
        aie::trace::configGroupEvents(&aieDevInst, loc, mod, type, metricSet);

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

          if(m_trace_start_broadcast)
            traceStartEvent = (XAie_Events) (XAIE_EVENT_BROADCAST_0_MEM + traceStartBroadcastChId1);
          else
            traceStartEvent = XAIE_EVENT_BROADCAST_8_MEM;
          traceEndEvent = XAIE_EVENT_BROADCAST_9_MEM;
          firstBroadcastId = 10;
        }

        if (type == module_type::core) {
          // Only enable Core -> MEM. Block everything else in both modules
          if (XAie_EventBroadcastBlockMapDir(&aieDevInst, loc, XAIE_CORE_MOD, XAIE_EVENT_SWITCH_A, 0xFF00, XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH) != XAIE_OK)
            break;
          if (XAie_EventBroadcastBlockMapDir(&aieDevInst, loc, XAIE_MEM_MOD, XAIE_EVENT_SWITCH_A, 0xFF00, XAIE_EVENT_BROADCAST_EAST | XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH) != XAIE_OK)
            break;
          
          for (uint8_t i = 8; i < 16; i++)
            if (XAie_EventBroadcastUnblockDir(&aieDevInst, loc, XAIE_CORE_MOD, XAIE_EVENT_SWITCH_A, i, XAIE_EVENT_BROADCAST_EAST) != XAIE_OK)
              break;
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
          aie::trace::configEventSelections(&aieDevInst, tile, loc, type, metricSet, channel0, 
                                            channel1, cfgTile->memory_tile_trace_config);
        }
        else {
          // Record if these are channel-specific events
          // NOTE: for now, check first event and assume single channel
          auto channelNum = aie::getChannelNumberFromEvent(memoryEvents.at(0));
          if (channelNum >= 0) {
            if (aie::isInputSet(type, metricSet)) {
              cfgTile->core_trace_config.mm2s_channels[0] = channelNum;
              if (static_cast<size_t>(channelNum) < tile.mm2s_names.size())
                cfgTile->core_trace_config.mm2s_names[0] = tile.mm2s_names.at(channelNum);
            }
            else {
              cfgTile->core_trace_config.s2mm_channels[0] = channelNum;
              if (static_cast<size_t>(channelNum) < tile.s2mm_names.size())
                cfgTile->core_trace_config.s2mm_names[0] = tile.s2mm_names.at(channelNum);
            }
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
          bool isCoreEvent = xdp::aie::isCoreModuleEvent(memoryEvents[i]);

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
          aie::trace::configEdgeEvents(&aieDevInst, tile, type, metricSet, memoryEvents[i], channel0);

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
        auto channelNum = aie::getChannelNumberFromEvent(interfaceEvents.at(0));
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

   /****************************************************************************
   * Set AIE Device Instance (Currently unused in Windows implementation)
   ***************************************************************************/
  void* AieTrace_WinImpl::setAieDeviceInst(void*, uint64_t) {  return nullptr;}

}  // namespace xdp
