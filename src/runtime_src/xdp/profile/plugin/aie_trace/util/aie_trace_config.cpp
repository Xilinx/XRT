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

#include "xdp/profile/plugin/aie_trace/util/aie_trace_config.h"
#include "xdp/profile/plugin/aie_trace/util/aie_trace_util.h"
#include "xdp/profile/database/static_info/aie_util.h"

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <regex>

#include "core/common/message.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/device/pl_device_intf.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/database/static_info/aie_constructs.h"

namespace xdp::aie::trace {
  using severity_level = xrt_core::message::severity_level;

  /****************************************************************************
   * Print out resource usage statistics for a given tile
   ***************************************************************************/
  void printTileStats(xaiefal::XAieDev* aieDevice, const tile_type& tile)
  {
    if (xrt_core::config::get_verbosity() < static_cast<uint32_t>(severity_level::info))
      return;

    auto col = tile.col;
    auto row = tile.row;
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
      auto pc = stats.getNumRsc(loc, XAIE_CORE_MOD, xaiefal::XAIE_PERFCOUNT);
      auto ts = stats.getNumRsc(loc, XAIE_CORE_MOD, xaiefal::XAIE_TRACEEVENT);
      auto bc = stats.getNumRsc(loc, XAIE_CORE_MOD, xaiefal::XAIE_BROADCAST);
      msg << "Resource Group : " << std::left << std::setw(10) << g << " "
          << "Performance Counters : " << pc << " "
          << "Trace Slots : " << ts << " "
          << "Broadcast Channels : " << bc << " " 
          << std::endl;
    }
    msg << "Resource usage stats for Tile : (" << col << "," << row << ") Module : Memory" << std::endl;
    for (auto& g : groups) {
      auto stats = aieDevice->getRscStat(g);
      auto pc = stats.getNumRsc(loc, XAIE_MEM_MOD, xaiefal::XAIE_PERFCOUNT);
      auto ts = stats.getNumRsc(loc, XAIE_MEM_MOD, xaiefal::XAIE_TRACEEVENT);
      auto bc = stats.getNumRsc(loc, XAIE_MEM_MOD, xaiefal::XAIE_BROADCAST);
      msg << "Resource Group : " << std::left << std::setw(10) << g << " "
          << "Performance Counters : " << pc << " "
          << "Trace Slots : " << ts << " "
          << "Broadcast Channels : " << bc << " " 
          << std::endl;
    }
    xrt_core::message::send(severity_level::info, "XRT", msg.str());
  }

  /****************************************************************************
   * Configure stream switch event ports for monitoring purposes
   ***************************************************************************/
  std::vector<std::shared_ptr<xaiefal::XAieStreamPortSelect>>
  configStreamSwitchPorts(XAie_DevInst* aieDevInst, const tile_type& tile,
                          xaiefal::XAieTile& xaieTile, const XAie_LocType loc,
                          const module_type type, const std::string metricSet,
                          const uint8_t channel0, const uint8_t channel1, 
                          std::vector<XAie_Events>& events, aie_cfg_base& config)
  {
    std::vector<std::shared_ptr<xaiefal::XAieStreamPortSelect>> streamPorts;
    std::map<uint8_t, std::shared_ptr<xaiefal::XAieStreamPortSelect>> switchPortMap;

    // Traverse all counters and request monitor ports as needed
    for (int i=0; i < events.size(); ++i) {
      // Ensure applicable event
      auto event = events.at(i);
      if (!isStreamSwitchPortEvent(event))
        continue;

      bool newPort = false;
      auto portnum = getPortNumberFromEvent(event);
      uint8_t channelNum = portnum % 2;
      uint8_t channel = (channelNum == 0) ? channel0 : channel1;

      // New port needed: reserve, configure, and store
      if (switchPortMap.find(portnum) == switchPortMap.end()) {
        auto switchPortRsc = xaieTile.sswitchPort();
        if (switchPortRsc->reserve() != AieRC::XAIE_OK)
          continue;
        newPort = true;
        switchPortMap[portnum] = switchPortRsc;

        if (type == module_type::core) {
          // AIE Tiles - Monitor DMA channels
          bool isMaster = ((portnum >= 2) || (metricSet.find("s2mm") != std::string::npos));
          auto slaveOrMaster = isMaster ? XAIE_STRMSW_MASTER : XAIE_STRMSW_SLAVE;
          std::string typeName = isMaster ? "S2MM" : "MM2S";
          std::string msg = "Configuring core module stream switch to monitor DMA " 
                          + typeName + " channel " + std::to_string(channelNum);
          xrt_core::message::send(severity_level::debug, "XRT", msg);
          switchPortRsc->setPortToSelect(slaveOrMaster, DMA, channelNum);

          // Record for runtime config file
          // NOTE: channel info informs back-end there will be events on that channel
          config.port_trace_ids[portnum] = channelNum;
          config.port_trace_is_master[portnum] = isMaster;
          if (isMaster)
            config.s2mm_channels[channelNum] = channelNum;
          else
            config.mm2s_channels[channelNum] = channelNum;
        }
        // Interface tiles (e.g., PLIO, GMIO)
        else if (type == module_type::shim) {
          // NOTE: skip configuration of extra ports for tile if stream_ids are not available.
          if (portnum >= tile.stream_ids.size())
            continue;

          auto slaveOrMaster = (tile.is_master_vec.at(portnum) == 0)   ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
          std::string typeName = (tile.is_master_vec.at(portnum) == 0) ? "slave" : "master";
          uint8_t streamPortId = static_cast<uint8_t>(tile.stream_ids.at(portnum));

          std::string msg = "Configuring interface tile stream switch to monitor " 
                          + typeName + " stream port " + std::to_string(streamPortId);
          xrt_core::message::send(severity_level::debug, "XRT", msg);
          switchPortRsc->setPortToSelect(slaveOrMaster, SOUTH, streamPortId);

          // Record for runtime config file
          config.port_trace_ids[portnum] = (tile.subtype == io_type::PLIO) ? portnum : channel;
          config.port_trace_is_master[portnum] = (tile.is_master_vec.at(portnum) != 0);

          if (tile.is_master_vec.at(portnum) == 0)
            config.mm2s_channels[channelNum] = channel;
          else
            config.s2mm_channels[channelNum] = channel;
        }
        else {
          // Memory tiles
          auto slaveOrMaster = isInputSet(type, metricSet) ? XAIE_STRMSW_MASTER : XAIE_STRMSW_SLAVE;
          std::string typeName = (slaveOrMaster == XAIE_STRMSW_MASTER) ? "master" : "slave";
          std::string msg = "Configuring memory tile stream switch to monitor " 
                          + typeName + " stream port " + std::to_string(channel);
          xrt_core::message::send(severity_level::debug, "XRT", msg);
          switchPortRsc->setPortToSelect(slaveOrMaster, DMA, channel);

          // Record for runtime config file
          config.port_trace_ids[portnum] = channel;
          config.port_trace_is_master[portnum] = (slaveOrMaster == XAIE_STRMSW_MASTER);
        }
      }

      auto switchPortRsc = switchPortMap[portnum];

      // Event options:
      //   getSSIdleEvent, getSSRunningEvent, getSSStalledEvent, & getSSTlastEvent
      XAie_Events ssEvent;
      if (isPortRunningEvent(event))
        switchPortRsc->getSSRunningEvent(ssEvent);
      else
        switchPortRsc->getSSStalledEvent(ssEvent);
      events.at(i) = ssEvent;

      if (newPort) {
        switchPortRsc->start();
        streamPorts.push_back(switchPortRsc);
      }
    }

    if ((type == module_type::shim) && (tile.subtype == io_type::PLIO) &&
        (switchPortMap.size() < tile.stream_ids.size())) {
      std::string msg = "Interface tile " + std::to_string(tile.col) + " has more "
                      + "PLIO than can be monitored by metric set " + metricSet + "."
                      + "Please run again with different settings or choose a different set.";
      xrt_core::message::send(severity_level::warning, "XRT", msg);
    }

    switchPortMap.clear();
    return streamPorts;
  }

  /****************************************************************************
   * Configure combo events (AIE tiles only)
   ***************************************************************************/
  std::vector<XAie_Events>
  configComboEvents(XAie_DevInst* aieDevInst, xaiefal::XAieTile& xaieTile, 
                    const XAie_LocType loc, const XAie_ModuleType mod,
                    const module_type type, const std::string metricSet,
                    aie_cfg_base& config)
  {
    // Only needed for core/memory modules and metric sets that include DMA events
    if (!isDmaSet(metricSet) || ((type != module_type::core) && (type != module_type::dma)))
      return {};

    std::vector<XAie_Events> comboEvents;

    if (mod == XAIE_CORE_MOD) {
      auto comboEvent = xaieTile.core().comboEvent(4);
      comboEvents.push_back(XAIE_EVENT_COMBO_EVENT_2_CORE);

      // Combo2 = Port_Idle_0 OR Port_Idle_1 OR Port_Idle_2 OR Port_Idle_3
      std::vector<XAie_Events> events = {XAIE_EVENT_PORT_IDLE_0_CORE,
          XAIE_EVENT_PORT_IDLE_1_CORE, XAIE_EVENT_PORT_IDLE_2_CORE,
          XAIE_EVENT_PORT_IDLE_3_CORE};
      std::vector<XAie_EventComboOps> opts = {XAIE_EVENT_COMBO_E1_OR_E2, 
          XAIE_EVENT_COMBO_E1_OR_E2, XAIE_EVENT_COMBO_E1_OR_E2};

      // Capture in config class to report later
      for (int i=0; i < NUM_COMBO_EVENT_CONTROL; ++i)
        config.combo_event_control[i] = 2;
      for (int i=0; i < events.size(); ++i) {
        uint16_t phyEvent = 0;
        XAie_EventLogicalToPhysicalConv_16(aieDevInst, loc, mod, events.at(i), &phyEvent);
        config.combo_event_input[i] = phyEvent;
      }

      // Set events and trigger on OR of events
      comboEvent->setEvents(events, opts);
      return comboEvents;
    }

    // Combo events do not auto-broadcast from core to memory module,
    // so let's avoid the complexity and find a different method. 
#if 0
    // Below is for memory modules

    // Memory_Combo0 = (Active OR Group_Stream_Switch)
    auto comboEvent0 = xaieTile.mem().comboEvent();
    comboEvents.push_back(XAIE_EVENT_COMBO_EVENT_0_MEM);

    std::vector<XAie_Events> events0;
    events0.push_back(XAIE_EVENT_ACTIVE_CORE);
    events0.push_back(XAIE_EVENT_GROUP_STREAM_SWITCH_CORE);
    std::vector<XAie_EventComboOps> opts0;
    opts0.push_back(XAIE_EVENT_COMBO_E1_OR_E2);
    
    comboEvent0->setEvents(events0, opts0);

    // Memory_Combo1 = (Group_Core_Program_Flow AND Core_Combo2)
    auto comboEvent1 = xaieTile.mem().comboEvent();
    comboEvents.push_back(XAIE_EVENT_COMBO_EVENT_1_MEM);

    std::vector<XAie_Events> events1;
    events1.push_back(XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE);
    events1.push_back(XAIE_EVENT_COMBO_EVENT_2_CORE);
    std::vector<XAie_EventComboOps> opts1;
    opts1.push_back(XAIE_EVENT_COMBO_E1_AND_E2);
    
    comboEvent1->setEvents(events1, opts1);
#else
    // Since we're tracing DMA events, start trace right away.
    // Specify user event 0 as trace end so we can flush after run.
    comboEvents.push_back(XAIE_EVENT_TRUE_MEM);
    comboEvents.push_back(XAIE_EVENT_USER_EVENT_0_MEM);
#endif
    return comboEvents;
  }

  /****************************************************************************
   * Configure group events (core modules only)
   ***************************************************************************/
  void configGroupEvents(XAie_DevInst* aieDevInst, const XAie_LocType loc,
                         const XAie_ModuleType mod, const module_type type, 
                         const std::string metricSet)
  {
    // Only needed for core module and metric sets that include DMA events
    if (!isDmaSet(metricSet) || (type != module_type::core))
      return;

    // Set masks for group events
    XAie_EventGroupControl(aieDevInst, loc, mod, XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE, 
                           GROUP_CORE_FUNCTIONS_MASK);
    XAie_EventGroupControl(aieDevInst, loc, mod, XAIE_EVENT_GROUP_CORE_STALL_CORE, 
                           GROUP_CORE_STALL_MASK);
    XAie_EventGroupControl(aieDevInst, loc, mod, XAIE_EVENT_GROUP_STREAM_SWITCH_CORE, 
                           GROUP_STREAM_SWITCH_RUNNING_MASK);
  }

  /****************************************************************************
   * Configure event selection (memory tiles only)
   ***************************************************************************/
  void configEventSelections(XAie_DevInst* aieDevInst, const XAie_LocType loc,
                             const module_type type, const std::string metricSet, 
                             const uint8_t channel0, const uint8_t channel1,
                             aie_cfg_base& config)
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

    XAie_EventSelectDmaChannel(aieDevInst, loc, 0, dmaDir, channel0);
    XAie_EventSelectDmaChannel(aieDevInst, loc, 1, dmaDir, channel1);

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
  void configEdgeEvents(XAie_DevInst* aieDevInst, const tile_type& tile,
                        const module_type type, const std::string metricSet, 
                        const XAie_Events event, const uint8_t channel)
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

      auto tileOffset = XAie_GetTileAddr(aieDevInst, tile.row, tile.col);
      XAie_Write32(aieDevInst, tileOffset + AIE_OFFSET_EDGE_CONTROL_MEM_TILE, 
                   edgeEventsValue);
      return;
    }

    // Below is AIE tile support
    
    // Event is DMA_MM2S_stalled_lock or DMA_S2MM_stream_starvation
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

    auto tileOffset = XAie_GetTileAddr(aieDevInst, tile.row, tile.col);
    XAie_Write32(aieDevInst, tileOffset + AIE_OFFSET_EDGE_CONTROL_MEM, 
                 edgeEventsValue);
  }

  /****************************************************************************
   * Configure delay for trace start event
   ***************************************************************************/
  bool configStartDelay(xaiefal::XAieMod& core, uint64_t delay,
                        XAie_Events& startEvent)
  {
    if (delay == 0)
      return false;

    // This algorithm daisy chains counters to get an effective 64 bit delay
    // counterLow -> counterHigh -> trace start
    uint32_t delayCyclesHigh = 0;
    uint32_t delayCyclesLow = 0;
    XAie_ModuleType mod = XAIE_CORE_MOD;
    bool useTwoCounters = (delay > std::numeric_limits<uint32_t>::max());

    if (useTwoCounters) {
      // ceil(x/y) where x and y are  positive integers
      delayCyclesHigh = static_cast<uint32_t>(1 + ((delay - 1) / std::numeric_limits<uint32_t>::max()));
      delayCyclesLow = static_cast<uint32_t>(delay / delayCyclesHigh);
    } else {
      delayCyclesLow = static_cast<uint32_t>(delay);
    }

    if (isDebugVerbosity()) {
      std::stringstream msg;
      msg << "Configuring AIE trace to start after delay of " << delay << " (low: " 
          << delayCyclesLow << ", high: " << delayCyclesHigh << ")" << std::endl;
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
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
    if (useTwoCounters && delayCyclesHigh) {
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

    startEvent = counterEvent;
    return true;
  }

  /****************************************************************************
   * Configure trace start on graph iteration
   ***************************************************************************/
  bool configStartIteration(xaiefal::XAieMod& core, uint32_t iteration,
                            XAie_Events& startEvent)
  {
    XAie_ModuleType mod = XAIE_CORE_MOD;
    // Count up by 1 for every iteration
    auto pc = core.perfCounter();
    if (pc->initialize(mod, XAIE_EVENT_INSTR_EVENT_0_CORE, 
                       mod, XAIE_EVENT_INSTR_EVENT_0_CORE) != XAIE_OK)
      return false;
    if (pc->reserve() != XAIE_OK)
      return false;

    xrt_core::message::send(severity_level::debug, "XRT", 
        "Configuring AIE trace to start on iteration " + std::to_string(iteration));

    pc->changeThreshold(iteration);
    
    XAie_Events counterEvent;
    pc->getCounterEvent(mod, counterEvent);
    // Reset when done counting
    pc->changeRstEvent(mod, counterEvent);
    if (pc->start() != XAIE_OK)
      return false;

    startEvent = counterEvent;
    return true;
  }

  /****************************************************************************
   * Reset timer for the specified tile range
   ***************************************************************************/
  void timerSyncronization(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice, std::shared_ptr<AieTraceMetadata> metadata, uint8_t startCol, uint8_t numCols)
  {
    std::shared_ptr<xaiefal::XAieBroadcast> traceStartBroadcastCh1 = nullptr, traceStartBroadcastCh2 = nullptr;
    std::vector<XAie_LocType> vL;
    traceStartBroadcastCh1 = aieDevice->broadcast(vL, XAIE_PL_MOD, XAIE_CORE_MOD);
    traceStartBroadcastCh1->reserve();
    traceStartBroadcastCh2 = aieDevice->broadcast(vL, XAIE_PL_MOD, XAIE_CORE_MOD);
    traceStartBroadcastCh2->reserve();

    uint8_t broadcastId1 = traceStartBroadcastCh1->getBc();
    uint8_t broadcastId2 = traceStartBroadcastCh2->getBc();

    //build broadcast network
    aie::trace::build2ChannelBroadcastNetwork(aieDevInst, metadata, broadcastId1, broadcastId2, XAIE_EVENT_USER_EVENT_0_PL, startCol, numCols);

    //set timer control register
    for (auto& tileMetric : metadata->getConfigMetrics()) {
      auto tile       = tileMetric.first;
      auto col        = tile.col + startCol; 
      auto row        = tile.row;
      auto type       = aie::getModuleType(row, metadata->getRowOffset());
      auto loc        = XAie_TileLoc(col, row);

      if(type == module_type::shim) {
        XAie_Events resetEvent = (XAie_Events)(XAIE_EVENT_BROADCAST_A_0_PL + broadcastId2);
        if(col == startCol)
        {
          resetEvent = XAIE_EVENT_USER_EVENT_0_PL;
        }

        XAie_SetTimerResetEvent(aieDevInst, loc, XAIE_PL_MOD, resetEvent, XAIE_RESETDISABLE);
      }
      else if(type == module_type::mem_tile) {
        XAie_Events resetEvent = (XAie_Events) (XAIE_EVENT_BROADCAST_0_MEM_TILE + broadcastId1);
        XAie_SetTimerResetEvent(aieDevInst, loc, XAIE_MEM_MOD, resetEvent, XAIE_RESETDISABLE);
      }
      else {
        XAie_Events resetEvent = (XAie_Events) (XAIE_EVENT_BROADCAST_0_CORE + broadcastId1);
        XAie_SetTimerResetEvent(aieDevInst, loc, XAIE_CORE_MOD, resetEvent, XAIE_RESETDISABLE);
        resetEvent = (XAie_Events) (XAIE_EVENT_BROADCAST_0_MEM + broadcastId1);
        XAie_SetTimerResetEvent(aieDevInst, loc, XAIE_MEM_MOD, resetEvent, XAIE_RESETDISABLE);
      }
    }

    //Generate the event to trigger broadcast network to reset timer
    XAie_EventGenerate(aieDevInst, XAie_TileLoc(startCol, 0), XAIE_PL_MOD, XAIE_EVENT_USER_EVENT_0_PL);

    //reset timer control register so that timer are not reset after this point
    for (auto& tileMetric : metadata->getConfigMetrics()) {
      auto tile       = tileMetric.first;
      auto col        = tile.col + startCol;
      auto row        = tile.row;
      auto type       = aie::getModuleType(row, metadata->getRowOffset());
      auto loc        = XAie_TileLoc(col, row);

      if(type == module_type::shim) {
        XAie_Events resetEvent = XAIE_EVENT_NONE_PL ;
        XAie_SetTimerResetEvent(aieDevInst, loc, XAIE_PL_MOD, resetEvent, XAIE_RESETDISABLE);
      }
      else if(type == module_type::mem_tile) {
        XAie_Events resetEvent = XAIE_EVENT_NONE_MEM_TILE;
        XAie_SetTimerResetEvent(aieDevInst, loc, XAIE_MEM_MOD, resetEvent, XAIE_RESETDISABLE);
      }
      else {
        XAie_Events resetEvent = XAIE_EVENT_NONE_CORE;
        XAie_SetTimerResetEvent(aieDevInst, loc, XAIE_CORE_MOD, resetEvent, XAIE_RESETDISABLE);
        resetEvent = XAIE_EVENT_NONE_MEM;
        XAie_SetTimerResetEvent(aieDevInst, loc, XAIE_MEM_MOD, resetEvent, XAIE_RESETDISABLE);
      }
    }

    //reset broadcast network
    reset2ChannelBroadcastNetwork(aieDevInst, metadata, broadcastId1, broadcastId2, startCol, numCols);

    //release the channels used for timer sync
    traceStartBroadcastCh1->release();
    traceStartBroadcastCh2->release();
  }

}  // namespace xdp
