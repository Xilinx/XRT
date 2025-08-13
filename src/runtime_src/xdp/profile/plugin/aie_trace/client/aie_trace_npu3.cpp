// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved

#define XDP_PLUGIN_SOURCE

#include "aie_trace_npu3.h"
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

  AieTrace_NPU3Impl::AieTrace_NPU3Impl(VPDatabase* database, std::shared_ptr<AieTraceMetadata> metadata)
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
    if (m_trace_start_broadcast)
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
    
    tranxHandler = std::make_unique<aie::NPU3Transaction>();
  }

  /***************************************************************************
  * Build broadcast network using specified channels
  ***************************************************************************/
  void AieTrace_NPU3Impl::build2ChannelBroadcastNetwork(void *hwCtxImpl, uint8_t broadcastId1, 
                                                        uint8_t broadcastId2, XAie_Events event) 
  {
    boost::property_tree::ptree aiePartitionPt = xdp::aie::getAIEPartitionInfo(hwCtxImpl);
    // Currently, assuming only one Hw Context is alive at a time
    // uint8_t startCol = static_cast<uint8_t>(aiePartitionPt.front().second.get<uint64_t>("start_col"));
    uint8_t startCol = 0; // Todo: Need to investigate segfault in the above line. 
    // uint8_t numCols  = static_cast<uint8_t>(aiePartitionPt.front().second.get<uint64_t>("num_cols"));
    uint8_t numCols = 3;

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
          if(col != startCol + numCols - 1) {
            XAie_EventBroadcastBlockDir(&aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId2, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH);
          }
          else {
            XAie_EventBroadcastBlockDir(&aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId2, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH);
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
            XAie_EventBroadcastBlockDir(&aieDevInst, loc, XAIE_CORE_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST);
          }
          else {
            XAie_EventBroadcastBlockDir(&aieDevInst, loc, XAIE_CORE_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST | XAIE_EVENT_BROADCAST_NORTH);
          }
        }
      }
    }
  }

  /***************************************************************************
  * Reset using broadcast network on specified channels
  ***************************************************************************/
  void AieTrace_NPU3Impl::reset2ChannelBroadcastNetwork(void *hwCtxImpl, uint8_t broadcastId1, 
                                                        uint8_t broadcastId2) 
  {
    boost::property_tree::ptree aiePartitionPt = xdp::aie::getAIEPartitionInfo(hwCtxImpl);
    // Currently, assuming only one Hw Context is alive at a time
    //uint8_t startCol = static_cast<uint8_t>(aiePartitionPt.back().second.get<uint64_t>("start_col"));
    uint8_t startCol = 0;
    //uint8_t numCols  = static_cast<uint8_t>(aiePartitionPt.back().second.get<uint64_t>("num_cols"));
    uint8_t numCols = 3;

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

  /***************************************************************************
  * Configure windowed event trace
  ***************************************************************************/
  bool AieTrace_NPU3Impl::configureWindowedEventTrace(void* hwCtxImpl) 
  {
    // Start recording the transaction
    if (!tranxHandler->initializeTransaction(&aieDevInst, "AieTraceWindow"))
      return false;
    
    boost::property_tree::ptree aiePartitionPt = xdp::aie::getAIEPartitionInfo(hwCtxImpl);
    // Currently, assuming only one Hw Context is alive at a time
    //uint8_t startCol = static_cast<uint8_t>(aiePartitionPt.back().second.get<uint64_t>("start_col"));
    uint8_t startCol = 0;

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
      if (tileType == module_type::shim) {
        if (startLayer != UINT_MAX) {
          if (col == startCol) 
            XAie_TraceStartEvent(&aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_PERF_CNT_0_PL);
          else 
            XAie_TraceStartEvent(&aieDevInst, loc, XAIE_PL_MOD, shimTraceStartEvent);
        }
      }
      else if (tileType == module_type::mem_tile) {
        if (startLayer != UINT_MAX)
          XAie_TraceStartEvent(&aieDevInst, loc, XAIE_MEM_MOD, memTileTraceStartEvent);
      }
      else {
        if (startLayer != UINT_MAX) {
          XAie_TraceStartEvent(&aieDevInst, loc, XAIE_CORE_MOD, coreModTraceStartEvent);
          XAie_TraceStartEvent(&aieDevInst, loc, XAIE_MEM_MOD, memTraceStartEvent);
        }
      }
    }

    if (startLayer != UINT_MAX) {
      XAie_PerfCounterControlSet(&aieDevInst, XAie_TileLoc(startCol, 0), XAIE_PL_MOD, 0, XAIE_EVENT_USER_EVENT_0_PL, XAIE_EVENT_USER_EVENT_0_PL);
      XAie_PerfCounterEventValueSet(&aieDevInst, XAie_TileLoc(startCol, 0), XAIE_PL_MOD, 0, startLayer);
    }

    build2ChannelBroadcastNetwork(hwCtxImpl, traceStartBroadcastChId1, traceStartBroadcastChId2, XAIE_EVENT_PERF_CNT_0_PL);

    xrt_core::message::send(severity_level::info, "XRT", "Finished AIE Windowed Trace Settings.");
    auto hwContext = metadata->getHwContext();
    tranxHandler->submitTransaction(&aieDevInst, hwContext);
    return true;
  }

  void AieTrace_NPU3Impl::updateDevice()
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
  uint64_t AieTrace_NPU3Impl::checkTraceBufSize(uint64_t size)
  {
    return size;
  }

  /****************************************************************************
   * Modify events in metric set based on type and channel
   ***************************************************************************/
  void AieTrace_NPU3Impl::modifyEvents(module_type type, io_type subtype, 
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

  void AieTrace_NPU3Impl::flushTraceModules()
  {
    //if (db->infoAvailable(xdp::info::ml_timeline)) {
    //  db->broadcast(VPDatabase::MessageType::READ_RECORD_TIMESTAMPS, nullptr);
    //  xrt_core::message::send(severity_level::debug, "XRT", "Done reading recorded timestamps.");
    //}

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

    // if (!tranxHandler->initializeTransaction(&aieDevInst, "AieTraceFlush"))
    //   return;

    // Flush trace by forcing end event
    // NOTE: this informs tiles to output remaining packets (even if partial)
    //for (const auto& loc : traceFlushLocs) 
    //  XAie_EventGenerate(&aieDevInst, loc, XAIE_CORE_MOD, coreTraceEndEvent);
    //for (const auto& loc : memoryTileTraceFlushLocs)
    //  XAie_EventGenerate(&aieDevInst, loc, XAIE_MEM_MOD, memoryTileTraceEndEvent);
    //for (const auto& loc : interfaceTileTraceFlushLocs)
    //  XAie_EventGenerate(&aieDevInst, loc, XAIE_PL_MOD, interfaceTileTraceEndEvent);

    traceFlushLocs.clear();
    memoryTileTraceFlushLocs.clear();
    interfaceTileTraceFlushLocs.clear();

    //xrt_core::message::send(severity_level::info, "XRT", "Before AIE trace flush.");

    //auto hwContext = metadata->getHwContext();
    // tranxHandler->submitTransaction(&aieDevInst, hwContext);
    //tranxHandler->submitELF(hwContext);

    //xrt_core::message::send(severity_level::info, "XRT", "Successfully scheduled AIE trace flush.");
  }

  void AieTrace_NPU3Impl::pollTimers(uint64_t index, void* handle) 
  {
    // TODO: Poll timers (needed for system timeline only)
    (void)index;
    (void)handle;
  }

  uint16_t AieTrace_NPU3Impl::getRelativeRow(uint16_t absRow)
  {
    auto rowOffset = metadata->getRowOffset();
    if (absRow == 0)
      return 0;
    if (absRow < rowOffset)
      return (absRow - 1);
    return (absRow - rowOffset);
  }

  module_type AieTrace_NPU3Impl::getTileType(uint8_t absRow)
  {
    if (absRow == 0)
      return module_type::shim;
    if (absRow < metadata->getRowOffset())
      return module_type::mem_tile;
    return module_type::core;
  }

  void AieTrace_NPU3Impl::freeResources() 
  {
    // Nothing to do
  }

  inline uint32_t AieTrace_NPU3Impl::bcIdToEvent(int bcId)
  {
    return bcId + CORE_BROADCAST_EVENT_BASE;
  }

  /****************************************************************************
   * Configure stream switch event ports for monitoring purposes
   ***************************************************************************/
  void
  AieTrace_NPU3Impl::configStreamSwitchPorts(const tile_type& tile, const XAie_LocType loc,
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
      if (!aie::isStreamSwitchPortEvent(event))
        continue;

      //bool newPort = false;
      auto portnum = aie::getPortNumberFromEvent(event);
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
        }
      }

      //auto switchPortRsc = switchPortMap[portnum];

      // Event options:
      //   getSSIdleEvent, getSSRunningEvent, getSSStalledEvent, & getSSTlastEvent
      // XAie_Events ssEvent;
      // if (aie::isPortRunningEvent(event))
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
  AieTrace_NPU3Impl::configComboEvents(const XAie_LocType loc, const XAie_ModuleType mod,
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
   * Configure group events (core modules only)
   ***************************************************************************/
  void
  AieTrace_NPU3Impl::configGroupEvents(const XAie_LocType loc, const XAie_ModuleType mod, 
                                       const module_type type, const std::string metricSet)
  {
    // Only needed for core module and metric sets that include DMA events
    if (!aie::isDmaSet(metricSet) || (type != module_type::core))
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
   * Configure event selection
   * NOTE: This supports memory tiles and interface tiles
   ***************************************************************************/
  void
  AieTrace_NPU3Impl::configEventSelections(const XAie_LocType loc, const module_type type,
                                           const std::string metricSet, std::vector<uint8_t>& channels,
                                           aie_cfg_base& config)
  {
    if ((type != module_type::mem_tile) && (type != module_type::shim))
      return;

    XAie_DmaDirection dmaDir = aie::isInputSet(type, metricSet) ? DMA_S2MM : DMA_MM2S;
    uint8_t numChannels = ((type == module_type::shim) && (dmaDir == DMA_MM2S))
                        ? NUM_CHANNEL_SELECTS_SHIM_NPU3 : NUM_CHANNEL_SELECTS;

    if (aie::isDebugVerbosity()) {
      std::string tileType = (type == module_type::shim) ? "interface" : "memory";
      std::string dmaType  = (dmaDir == DMA_S2MM) ? "S2MM" : "MM2S";
      std::stringstream channelsStr;
      std::copy(channels.begin(), channels.end(), std::ostream_iterator<uint8_t>(channelsStr, ", "));
      
      std::string msg = "Configuring event selections for " + tileType + " tile DMA " 
                      + dmaType + " channels " + channelsStr.str();
      xrt_core::message::send(severity_level::debug, "XRT", msg);
    }

    for (uint8_t c = 0; c < numChannels; ++c) {
      XAie_EventSelectDmaChannel(&aieDevInst, loc, c, dmaDir, channels.at(c));
     
      // Record for runtime config file
      config.port_trace_ids[c] = channels.at(c);
      if (aie::isInputSet(type, metricSet)) {
        config.port_trace_is_master[c] = true;
        config.s2mm_channels[c] = channels.at(c);
      }
      else {
        config.port_trace_is_master[c] = false;
        config.mm2s_channels[c] = channels.at(c);
      }
    }
  }

  /****************************************************************************
   * Configure edge detection events
   ***************************************************************************/
  void AieTrace_NPU3Impl::configEdgeEvents(const tile_type& tile, const module_type type,
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
      uint16_t eventNum = aie::isInputSet(type, metricSet)
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
    uint16_t eventNum = aie::isInputSet(type, metricSet)
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
  bool AieTrace_NPU3Impl::setMetricsSettings(uint64_t deviceId, void* hwCtxImpl)
  {
    (void)deviceId;

    // Get partition columns
    boost::property_tree::ptree aiePartitionPt = xdp::aie::getAIEPartitionInfo(hwCtxImpl);
    // Currently, assuming only one Hw Context is alive at a time
    //uint8_t startCol = static_cast<uint8_t>(aiePartitionPt.front().second.get<uint64_t>("start_col"));
    uint8_t startCol = 0;

    std::string startType = xrt_core::config::get_aie_trace_settings_start_type();
    unsigned int startLayer = xrt_core::config::get_aie_trace_settings_start_layer();
    
    // Initialize and start transaction
    std::string tranxName = "AieTraceMetrics";
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
      "Starting transaction " + tranxName);
    if (!tranxHandler->initializeTransaction(&aieDevInst, tranxName))
      return false;

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
      auto loc        = XAie_TileLoc(col, row);

      std::stringstream cmsg;
      cmsg << "Configuring tile (" << +col << "," << +row << ") in module type: " << aie::getModuleName(type) << ".";
      xrt_core::message::send(severity_level::info, "XRT", cmsg.str());

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

        // Set end event for trace capture
        // NOTE: This needs to be done first
        if (XAie_TraceStopEvent(&aieDevInst, loc, mod, coreTraceEndEvent) != XAIE_OK)
          break;

        for (uint8_t i = 0; i < coreEvents.size(); i++) {
          uint8_t slot = i;
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

        XAie_Packet pkt = {0, 0};
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
      // NOTE: This is applicable for memory modules in AIE tiles or memory tiles
      // NOTE 2: For NPU3, this configures the second trace stream that uses time packets
      if ((type == module_type::core) || (type == module_type::mem_tile)) {
        xrt_core::message::send(severity_level::info, "XRT", "Configuring Memory Trace Events");
        XAie_ModuleType mod = XAIE_MEM_MOD;

        // Set overall start/end for trace capture
        auto traceStartEvent = (type == module_type::core) ? coreTraceStartEvent : memoryTileTraceStartEvent;
        auto traceEndEvent = (type == module_type::core) ? coreTraceEndEvent : memoryTileTraceEndEvent;

        aie_cfg_base& aieConfig = cfgTile->core_trace_config;
        if (type == module_type::mem_tile)
          aieConfig = cfgTile->memory_tile_trace_config;

        // Configure combo events for metric sets that include DMA events        
        auto comboEvents = configComboEvents(loc, mod, type, metricSet, aieConfig);
        if (comboEvents.size() == 2) {
          traceStartEvent = comboEvents.at(0);
          traceEndEvent = comboEvents.at(1);
        }

        // Configure event ports on stream switch
        configStreamSwitchPorts(tile, loc, type, metricSet, 0, 0, memoryEvents, aieConfig);
        
        memoryModTraceStartEvent = traceStartEvent;
        if (XAie_TraceStopEvent(&aieDevInst, loc, mod, traceEndEvent) != XAIE_OK)
          break;

        {
          uint16_t phyEvent1 = 0;
          uint16_t phyEvent2 = 0;
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, XAIE_CORE_MOD, traceStartEvent, &phyEvent1);
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, XAIE_CORE_MOD, traceEndEvent, &phyEvent2);
          if (type == module_type::core) {
            cfgTile->memory_trace_config.start_event = phyEvent1;
            cfgTile->memory_trace_config.stop_event = phyEvent2;
          } else {
            cfgTile->memory_tile_trace_config.start_event = phyEvent1;
            cfgTile->memory_tile_trace_config.stop_event = phyEvent2;
          }
        }

        auto iter0 = configChannel0.find(tile);
        auto iter1 = configChannel1.find(tile);
        uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;
        uint8_t channel1 = (iter1 == configChannel1.end()) ? 1 : iter1->second;
        // TODO: for now, hard-code channels 2 and 3
        std::vector<uint8_t> channels = {channel0, channel1, 2, 3};

        // Specify Sel0/Sel1 for memory tiles
        if (type == module_type::mem_tile) {
          configEventSelections(loc, type, metricSet, channels, cfgTile->memory_tile_trace_config);
        }
        else {
          // Record if these are channel-specific events
          // NOTE: for now, check first event and assume single channel
          auto channelNum = aie::getChannelNumberFromEvent(memoryEvents.at(0));
          if (channelNum >= 0) {
            if (aie::isInputSet(type, metricSet))
              cfgTile->core_trace_config.mm2s_channels[0] = channelNum;
            else
              cfgTile->core_trace_config.s2mm_channels[0] = channelNum;
          }
        }

        // Configure memory trace events
        for (uint8_t i = 0; i < memoryEvents.size(); i++) {
          if (XAie_TraceEvent(&aieDevInst, loc, XAIE_MEM_MOD, memoryEvents[i], i) != XAIE_OK)
            break;
          numMemoryTraceEvents++;

          // Configure edge events (as needed)
          configEdgeEvents(&aieDevInst, tile, type, metricSet, memoryEvents[i], channel0);

          // Update config file
          uint16_t phyEvent = 0;
          auto phyMod = XAIE_CORE_MOD;
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, phyMod, memoryEvents[i], &phyEvent);

          if (type == module_type::mem_tile)
            cfgTile->memory_tile_trace_config.traced_events[i] = phyEvent;
          else
            cfgTile->memory_trace_config.traced_events[i] = phyEvent;
        }

        memoryEvents.clear();
        mNumTileTraceEvents[typeInt][numMemoryTraceEvents]++;
        
        uint8_t packetType = (type == module_type::mem_tile) ? 3 : 1;
        XAie_Packet pkt = {0, packetType};

        xrt_core::message::send(severity_level::info, "XRT", "Configuring Memory Trace Mode");

        if (XAie_TracePktConfig(&aieDevInst, loc, mod, pkt) != XAIE_OK)
          break;
        if ((startType != "layer") || (startLayer ==  UINT_MAX)) {
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

        // Get specified channel numbers
        auto iter0 = configChannel0.find(tile);
        auto iter1 = configChannel1.find(tile);
        uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;
        uint8_t channel1 = (iter1 == configChannel1.end()) ? 1 : iter1->second;
        // TODO: for now, hard-code channels 2 and 3
        std::vector<uint8_t> channels = {channel0, channel1, 2, 3};

        // Modify events as needed
        modifyEvents(type, subtype, metricSet, channel0, interfaceEvents);
        
        // Specify Sel0/Sel1 for interface tiles (new for NPU3)
        configEventSelections(loc, type, metricSet, channels, cfgTile->interface_tile_trace_config);
        configStreamSwitchPorts(tileMetric.first, loc, type, metricSet, channel0, channel1, 
                                interfaceEvents, cfgTile->interface_tile_trace_config);

        // Configure interface tile trace events
        for (size_t i = 0; i < interfaceEvents.size(); i++) {
          auto event = interfaceEvents.at(i);
          if (XAie_TraceEvent(&aieDevInst, loc, mod, event, static_cast<uint8_t>(i)) != XAIE_OK)
            break;
          numInterfaceTraceEvents++;

          // Update config file
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
        
        uint8_t packetType = 4;
        XAie_Packet pkt = {0, packetType};
        if (XAie_TracePktConfig(&aieDevInst, loc, mod, pkt) != XAIE_OK)
          break;
        if (startType != "layer" || startLayer ==  UINT_MAX) {
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
      (db->getStaticInfo()).addAIECfgTile(deviceId, cfgTile);
      xrt_core::message::send(severity_level::info, "XRT", "Debugging XDP: after (db->getStaticInfo()).addAIECfgTile");  
    }  // For tiles

    if (m_trace_start_broadcast) {
      xrt_core::message::send(severity_level::info, "XRT", "before build2ChannelBroadcastNetwork");  
      build2ChannelBroadcastNetwork(hwCtxImpl, traceStartBroadcastChId1, traceStartBroadcastChId2, interfaceTileTraceStartEvent);
      xrt_core::message::send(severity_level::info, "XRT", "before XAie_EventGenerate");
      XAie_EventGenerate(&aieDevInst, XAie_TileLoc(startCol, 0), XAIE_PL_MOD,  interfaceTileTraceStartEvent);
    }

    xrt_core::message::send(severity_level::info, "XRT", "before tranxHandler->submitTransaction");
    auto hwContext = metadata->getHwContext();
    tranxHandler->submitTransaction(&aieDevInst, hwContext);

    xrt_core::message::send(severity_level::info, "XRT", "Successfully scheduled AIE Trace.");

    if (!tranxHandler->initializeTransaction(&aieDevInst, "AieTraceFlush"))
      return false;

    // Flush trace by forcing end event
    // NOTE: this informs tiles to output remaining packets (even if partial)
    for (const auto& loc : traceFlushLocs) 
      XAie_EventGenerate(&aieDevInst, loc, XAIE_CORE_MOD, coreTraceEndEvent);
    for (const auto& loc : memoryTileTraceFlushLocs)
      XAie_EventGenerate(&aieDevInst, loc, XAIE_MEM_MOD, memoryTileTraceEndEvent);
    for (const auto& loc : interfaceTileTraceFlushLocs)
      XAie_EventGenerate(&aieDevInst, loc, XAIE_PL_MOD, interfaceTileTraceEndEvent);

    tranxHandler->completeASM(&aieDevInst);
    tranxHandler->generateELF();

    xrt_core::message::send(severity_level::info, "XRT", "Successfully generated ELF for AIE Trace Flush.");

    return true;
  }

  /****************************************************************************
   * Set AIE Device Instance (Currently unused in NPU3 implementation)
   ***************************************************************************/
  void* AieTrace_NPU3Impl::setAieDeviceInst(void*, uint64_t) {  return nullptr;}

}  // namespace xdp
