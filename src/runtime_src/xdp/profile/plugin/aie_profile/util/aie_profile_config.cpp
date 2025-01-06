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

#include "xdp/profile/plugin/aie_profile/util/aie_profile_config.h"
#include "xdp/profile/plugin/aie_profile/util/aie_profile_util.h"
#include "xdp/profile/database/static_info/aie_util.h"

#include <cmath>
#include <cstring>
#include <memory>
#include "core/common/message.h"

namespace xdp::aie::profile {
  using severity_level = xrt_core::message::severity_level;

  /****************************************************************************
   * Configure stream switch ports for monitoring purposes
   * NOTE: Used to monitor streams: trace, interfaces, and memory tiles
   ***************************************************************************/
  void configStreamSwitchPorts(const tile_type& tile, xaiefal::XAieTile& xaieTile, 
                               const XAie_LocType loc, const module_type type, 
                               const uint32_t numCounters, const std::string metricSet, 
                               const uint8_t channel0, const uint8_t channel1, 
                               std::vector<XAie_Events>& startEvents,
                               std::vector<XAie_Events>& endEvents,
                               std::vector<std::shared_ptr<xaiefal::XAieStreamPortSelect>>& streamPorts)
  {
    std::map<uint8_t, std::shared_ptr<xaiefal::XAieStreamPortSelect>> switchPortMap;

    // Traverse all counters and request monitor ports as needed
    for (uint32_t i=0; i < numCounters; ++i) {
      // Ensure applicable event
      auto startEvent = startEvents.at(i);
      auto endEvent = endEvents.at(i);
      if (!aie::profile::isStreamSwitchPortEvent(startEvent))
        continue;

      bool newPort = false;
      auto portnum = getPortNumberFromEvent(startEvent);
      uint8_t channel = (portnum == 0) ? channel0 : channel1;

      // New port needed: reserver, configure, and store
      if (switchPortMap.find(portnum) == switchPortMap.end()) {
        auto switchPortRsc = xaieTile.sswitchPort();
        if (switchPortRsc->reserve() != AieRC::XAIE_OK)
          continue;
        newPort = true;
        switchPortMap[portnum] = switchPortRsc;

        if (type == module_type::core) {
          int channelNum = 0;
          std::string portName;

          // AIE Tiles
          if (metricSet.find("trace") != std::string::npos) {
            // Monitor memory or core trace (memory:1, core:0)
            uint8_t traceSelect = (startEvent == XAIE_EVENT_PORT_RUNNING_0_CORE) ? 1 : 0;
            switchPortRsc->setPortToSelect(XAIE_STRMSW_SLAVE, TRACE, traceSelect);
            
            channelNum = traceSelect;
            portName = (traceSelect == 0) ? "core trace" : "memory trace";
          }
          else {
            auto slaveOrMaster = aie::isInputSet(type, metricSet) ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
            switchPortRsc->setPortToSelect(slaveOrMaster, DMA, channel);

            channelNum = channel;
            portName = aie::isInputSet(type, metricSet) ? "DMA MM2S" : "DMA S2MM";
          }

          if (aie::isDebugVerbosity()) {
              std::stringstream msg;
              msg << "Configured core module stream switch to monitor " << portName 
                  << " for metric set " << metricSet << " and channel " << channelNum;
              xrt_core::message::send(severity_level::debug, "XRT", msg.str());
          }
        }
        // Interface tiles (e.g., PLIO, GMIO)
        else if (type == module_type::shim) {
          // NOTE: skip configuration of extra ports for tile if stream_ids are not available.
          if (portnum >= tile.stream_ids.size())
            continue;
          // Grab slave/master and stream ID
          auto slaveOrMaster = (tile.is_master_vec.at(portnum) == 0) ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
          uint8_t streamPortId = static_cast<uint8_t>(tile.stream_ids.at(portnum));
          switchPortRsc->setPortToSelect(slaveOrMaster, SOUTH, streamPortId);

          if (aie::isDebugVerbosity()) {
            std::string typeName = (tile.is_master_vec.at(portnum) == 0) ? "slave" : "master";
            std::string msg = "Configuring interface tile stream switch to monitor " 
                            + typeName + " stream port " + std::to_string(streamPortId);
            xrt_core::message::send(severity_level::debug, "XRT", msg);
          }
        }
        else {
          // Memory tiles
          std::string typeName;
          uint32_t channelNum = 0;

          if (metricSet.find("trace") != std::string::npos) {
            typeName = "trace";
            switchPortRsc->setPortToSelect(XAIE_STRMSW_SLAVE, TRACE, 0);
          }
          else {
            auto slaveOrMaster = aie::isInputSet(type, metricSet) ? XAIE_STRMSW_MASTER : XAIE_STRMSW_SLAVE;
            switchPortRsc->setPortToSelect(slaveOrMaster, DMA, channel);

            typeName = (slaveOrMaster == XAIE_STRMSW_MASTER) ? "master" : "slave";
            channelNum = channel;
          }

          if (aie::isDebugVerbosity()) {
            std::string msg = "Configuring memory tile stream switch to monitor " 
                            + typeName + " stream port " + std::to_string(channelNum);
            xrt_core::message::send(severity_level::debug, "XRT", msg);
          }
        }
      }

      auto switchPortRsc = switchPortMap[portnum];

      // Event options:
      //   getSSIdleEvent, getSSRunningEvent, getSSStalledEvent, & getSSTlastEvent
      XAie_Events ssEvent;
      if (aie::profile::isPortRunningEvent(startEvent))
        switchPortRsc->getSSRunningEvent(ssEvent);
      else if (aie::profile::isPortTlastEvent(startEvent))
        switchPortRsc->getSSTlastEvent(ssEvent);
      else if (aie::profile::isPortStalledEvent(startEvent))
        switchPortRsc->getSSStalledEvent(ssEvent);
      else
        switchPortRsc->getSSIdleEvent(ssEvent);

      startEvents.at(i) = ssEvent;
      endEvents.at(i) = ssEvent;

      if (newPort) {
        switchPortRsc->start();
        streamPorts.push_back(switchPortRsc);
      }
    }

    switchPortMap.clear();
  }

  /****************************************************************************
   * Configure performance counter for profile API
   ***************************************************************************/
  std::shared_ptr<xaiefal::XAiePerfCounter>
  configProfileAPICounters(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice,
                           std::shared_ptr<AieProfileMetadata> metadata,
                           xaiefal::XAieMod& xaieModule, XAie_ModuleType& xaieModType, 
                           const module_type xdpModType, const std::string& metricSet, 
                           XAie_Events startEvent, XAie_Events endEvent, 
                           XAie_Events resetEvent, int pcIndex, size_t counterIndex, 
                           size_t threshold, XAie_Events& retCounterEvent, const tile_type& tile,
                           std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesLatency,
                           std::map<adfAPI, std::map<std::string, adfAPIResourceInfo>>& adfAPIResourceInfoMap)
  {
    if (xdpModType != module_type::shim)
      return nullptr;

    if ((metricSet == METRIC_LATENCY) && (pcIndex == 0)) {
      bool isSourceTile = true;
      auto pc = configInterfaceLatency(aieDevInst, aieDevice, metadata, xaieModule, xaieModType, xdpModType, 
                                       metricSet, startEvent, endEvent, resetEvent, pcIndex, threshold, 
                                       retCounterEvent, tile, isSourceTile, bcResourcesLatency);
      std::string srcDestPairKey = metadata->getSrcDestPairKey(tile.col, tile.row);
      if (isSourceTile) {
        adfAPIResourceInfoMap[aie::profile::adfAPI::INTF_TILE_LATENCY][srcDestPairKey].isSourceTile = true; 
        adfAPIResourceInfoMap[aie::profile::adfAPI::INTF_TILE_LATENCY][srcDestPairKey].srcPcIdx = counterIndex;
      }
      else {
        adfAPIResourceInfoMap[aie::profile::adfAPI::INTF_TILE_LATENCY][srcDestPairKey].destPcIdx = counterIndex;
      }
      return pc;
    }

    if ((metricSet == METRIC_BYTE_COUNT) && (pcIndex == 0)) {
      auto pc = configPCUsingComboEvents(xaieModule, xaieModType, xdpModType,
                                         metricSet, startEvent, endEvent, resetEvent,
                                         pcIndex, threshold, retCounterEvent);
      std::string srcKey = "(" + aie::uint8ToStr(tile.col) + "," + aie::uint8ToStr(tile.row) + ")";
      adfAPIResourceInfoMap[aie::profile::adfAPI::START_TO_BYTES_TRANSFERRED][srcKey].srcPcIdx = counterIndex;
      adfAPIResourceInfoMap[aie::profile::adfAPI::START_TO_BYTES_TRANSFERRED][srcKey].isSourceTile = true;
      return pc;
    }

    // Request counter from resource manager
    auto pc = xaieModule.perfCounter();
    auto ret = pc->initialize(xaieModType, startEvent, xaieModType, endEvent);
    if (ret != XAIE_OK)
      return nullptr;

    ret = pc->reserve();
    if (ret != XAIE_OK)
      return nullptr;

    if (resetEvent != XAIE_EVENT_NONE_CORE)
      pc->changeRstEvent(xaieModType, resetEvent);

    if (threshold > 0)
      pc->changeThreshold(threshold);

    XAie_Events counterEvent;
    pc->getCounterEvent(xaieModType, counterEvent);

    // Start the counter
    ret = pc->start();
    if (ret != XAIE_OK) return nullptr;
    
    // Respond back with this performance counter event 
    // to use it later for broadcasting
    retCounterEvent = counterEvent;
    return pc;
  }

  /****************************************************************************
   * Start a performance counter
   ***************************************************************************/
  std::shared_ptr<xaiefal::XAiePerfCounter>
  startCounter(std::shared_ptr<xaiefal::XAiePerfCounter>& pc,
               XAie_Events counterEvent, XAie_Events& retCounterEvent)
  {
    if (!pc)
      return nullptr;
    
    auto ret = pc->start();
    if (ret != XAIE_OK)
      return nullptr;
    
    // Return the known counter event
    retCounterEvent = counterEvent;
    return pc;
  }

  /****************************************************************************
   * Configure performance counter using combo event 3 FSM
   ***************************************************************************/
  std::shared_ptr<xaiefal::XAiePerfCounter>
  configPCUsingComboEvents(xaiefal::XAieMod& xaieModule, XAie_ModuleType& xaieModType, 
                           const module_type xdpModType, const std::string& metricSet, 
                           XAie_Events startEvent, XAie_Events endEvent, XAie_Events resetEvent,
                           int pcIndex, size_t threshold, XAie_Events& retCounterEvent)
  {
    if ((xdpModType != module_type::shim) || (xaieModType != XAIE_PL_MOD))
      return nullptr;
    
    std::shared_ptr<xaiefal::XAieComboEvent>  comboEvent0 = nullptr;
    std::vector<XAie_Events>         combo_events;
    std::vector<XAie_EventComboOps>  combo_opts;
    std::vector<XAie_Events>         comboConfigedEvents;
    XAie_Events newStartEvent = XAIE_EVENT_NONE_CORE;

    // Request combo event from xaie module
    auto pc = xaieModule.perfCounter();
    auto ret = pc->initialize(xaieModType, startEvent, 
                              xaieModType, endEvent);

    if (ret != XAIE_OK) return nullptr;
    ret = pc->reserve();
    if (ret != XAIE_OK) return nullptr;

    XAie_Events counterEvent;
    pc->getCounterEvent(xaieModType, counterEvent);

    if (resetEvent != XAIE_EVENT_NONE_CORE)
      pc->changeRstEvent(xaieModType, resetEvent);

    // Configure the combo events if user has specified valid non zero threshold
    // if (threshold==0)
    //   return startCounter(pc, counterEvent, retCounterEvent);

    // Set up a combo event using start & count event type
    comboEvent0 = xaieModule.comboEvent(4);
    ret = comboEvent0->reserve();
    if (ret != XAIE_OK)
      return nullptr;

    // Set up the combo event with FSM type using 4 events state machine
    XAie_Events eventA = (resetEvent != XAIE_EVENT_NONE_CORE) ? resetEvent : XAIE_EVENT_USER_EVENT_1_PL;
    XAie_Events eventB = XAIE_EVENT_USER_EVENT_1_PL;
    XAie_Events eventC = startEvent;
    XAie_Events eventD = endEvent;

    combo_events.push_back(eventA);
    combo_events.push_back(eventB);
    combo_events.push_back(eventC);
    combo_events.push_back(eventD);

    // This is NO-OP for COMBO3, necessary for FAL & generates COMBO 1 & 2 events as well
    combo_opts.push_back(XAIE_EVENT_COMBO_E1_OR_E2);
    combo_opts.push_back(XAIE_EVENT_COMBO_E1_OR_E2);
    combo_opts.push_back(XAIE_EVENT_COMBO_E1_OR_E2);

    ret = comboEvent0->setEvents(combo_events, combo_opts);
    if (ret != XAIE_OK)
      return nullptr;

    ret = comboEvent0->getEvents(comboConfigedEvents);
    if (ret != XAIE_OK)
      return nullptr;
    
    // Change the start event to above combo event type
    newStartEvent = XAIE_EVENT_COMBO_EVENT_3_PL;
    ret = pc->changeStartEvent(xaieModType, newStartEvent);
    if (ret != XAIE_OK)
      return nullptr;

    // Start the combo event 0
    ret = comboEvent0->start();
    if (ret != XAIE_OK)
      return nullptr;

    return startCounter(pc, counterEvent, retCounterEvent);
  }

  /****************************************************************************
   * Get and configure broadcast channels from source to destination tiles
   * NOTE: This function applies to interface tiles only
   ***************************************************************************/
  std::pair<int, XAie_Events>
  getPLBroadcastChannel(xaiefal::XAieDev* aieDevice, const tile_type& srcTile, 
                        std::shared_ptr<AieProfileMetadata> metadata,
                        std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesLatency)
  {
    std::pair<int, XAie_Events> rc(-1, XAIE_EVENT_NONE_PL);
    AieRC RC = AieRC::XAIE_OK;
    tile_type destTile;
    
    metadata->getDestTile(srcTile, destTile);
    XAie_LocType destTileLocation = XAie_TileLoc(destTile.col, destTile.row);

    // Include all tiles between source and destination
    std::vector<XAie_LocType> bcTileVec;
    for (uint8_t c = std::min(srcTile.col, destTile.col); c <= std::max(srcTile.col, destTile.col); ++c) {
      auto tileLocation = XAie_TileLoc(c, srcTile.row);
      bcTileVec.push_back(tileLocation);
    }
    
    auto BC = aieDevice->broadcast(bcTileVec, XAIE_PL_MOD, XAIE_PL_MOD);
    if (!BC)
      return rc;
    bcResourcesLatency.push_back(BC);

    auto bcPair = aie::profile::getPreferredPLBroadcastChannel();
    BC->setPreferredId(bcPair.first);

    RC = BC->reserve();
    if (RC != XAIE_OK)
      return rc;

    RC = BC->start();
    if (RC != XAIE_OK)
      return rc;

    uint8_t bcId = BC->getBc();
    XAie_Events bcEvent;
    RC = BC->getEvent(destTileLocation, XAIE_PL_MOD, bcEvent);
    if (RC != XAIE_OK)
      return rc;

    std::pair<int, XAie_Events> bcPairSelected = std::make_pair(bcId, bcEvent);
    return bcPairSelected;
  }

  /****************************************************************************
   * Initialize broadcast channels
   ***************************************************************************/
  std::pair<int, XAie_Events>
  setupBroadcastChannel(xaiefal::XAieDev* aieDevice, const tile_type& currTileLoc, 
                        std::shared_ptr<AieProfileMetadata> metadata,
                        std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesLatency)
  {
    // This stores the map of location of tile and configured broadcast channel event
    static std::map<tile_type, std::pair<int, XAie_Events>> adfAPIBroadcastEventsMap;

    tile_type srcTile = currTileLoc;
    if (!metadata->isSourceTile(currTileLoc))
      if (!metadata->getSourceTile(currTileLoc, srcTile))
        return {-1, XAIE_EVENT_NONE_CORE};
    
    if (adfAPIBroadcastEventsMap.find(srcTile) == adfAPIBroadcastEventsMap.end()) {
      // auto bcPair = aie::profile::getPreferredPLBroadcastChannel();
      auto bcPair = getPLBroadcastChannel(aieDevice, srcTile, metadata, bcResourcesLatency);
      if (bcPair.first == -1 || bcPair.second == XAIE_EVENT_NONE_CORE) {
        return {-1, XAIE_EVENT_NONE_CORE};
      }
      adfAPIBroadcastEventsMap[srcTile] = bcPair;
    }
    return adfAPIBroadcastEventsMap.at(srcTile);
  }

  /****************************************************************************
   * Configure interface tile counter for latency
   ***************************************************************************/
  std::shared_ptr<xaiefal::XAiePerfCounter>
  configInterfaceLatency(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice,
                         std::shared_ptr<AieProfileMetadata> metadata,
                         xaiefal::XAieMod& xaieModule, XAie_ModuleType& xaieModType, 
                         const module_type xdpModType, const std::string& metricSet, 
                         XAie_Events startEvent, XAie_Events endEvent, XAie_Events resetEvent, 
                         int pcIndex, size_t threshold, XAie_Events& retCounterEvent,
                         const tile_type& tile, bool& isSource,
                         std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesLatency)
  {
   // Request combo event from xaie module
    auto pc = xaieModule.perfCounter();

    if (!metadata->isValidLatencyTile(tile))
      return nullptr;
    
    startEvent = XAIE_EVENT_USER_EVENT_0_PL;
    if (!metadata->isSourceTile(tile)) {
      auto bcPair = setupBroadcastChannel(aieDevice, tile, metadata, bcResourcesLatency);
      startEvent = bcPair.second;
      isSource = false;
    }

    auto ret = pc->initialize(xaieModType, startEvent, xaieModType, endEvent);
    if (ret != XAIE_OK)
      return nullptr;

    ret = pc->reserve();
    if (ret != XAIE_OK)
      return nullptr;

   // Start the counter
    ret = pc->start();
    if (ret != XAIE_OK)
      return nullptr;

    XAie_LocType tileloc = XAie_TileLoc(tile.col, tile.row);

    // uint8_t status = -1;
    // uint8_t broadcastId  = 10;

    if (isSource) {
      auto bc_pair = setupBroadcastChannel(aieDevice, tile, metadata, bcResourcesLatency);
      if (bc_pair.first == -1)
        return nullptr;

      uint8_t broadcastId  = static_cast<uint8_t>(bc_pair.first);
      // Set up of the brodcast of event over channel
      XAie_EventBroadcast(aieDevInst, tileloc, XAIE_PL_MOD, broadcastId, XAIE_EVENT_USER_EVENT_0_PL);

      XAie_EventGenerate(aieDevInst, tileloc, xaieModType, XAIE_EVENT_USER_EVENT_0_PL);
    }

    // to use it later for broadcasting
    return pc;
  }

  /****************************************************************************
   * Configure individual AIE events for metric sets related to Profile APIs
   ***************************************************************************/
   bool 
   configGraphIteratorAndBroadcast(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice,
                                   std::shared_ptr<AieProfileMetadata> metadata,
                                   xaiefal::XAieMod core, XAie_LocType loc, 
                                   const XAie_ModuleType xaieModType, const module_type xdpModType, 
                                   const std::string metricSet, XAie_Events& bcEvent,
                                   std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesBytesTx)
  {
    bool rc = false;
    if (!aie::profile::metricSupportsGraphIterator(metricSet))
      return rc;

    if (xdpModType != module_type::core) {
      auto aieCoreTilesVec = metadata->getTiles("all", module_type::core, "all");
      if (aieCoreTilesVec.empty()) {
        std::stringstream msg;
        msg << "No core tiles available, graph ieration profiling will not be available.\n";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        return rc;
      }

      // Use the first available core tile to configure the broadcasting
      uint8_t col = aieCoreTilesVec.begin()->col;
      uint8_t row = aieCoreTilesVec.begin()->row;
      auto& xaieTile = aieDevice->tile(col, row);
      core = xaieTile.core();
      loc = XAie_TileLoc(col, row);
    }

    auto iterCount = metadata->getIterationCount();

    std::stringstream msg;
    msg << "Configuring AIE profile start_to_bytes_transferred to start on iteration " << iterCount
        << " using core tile (" << +loc.Col << "," << +loc.Row << ").\n";
    xrt_core::message::send(severity_level::debug, "XRT", msg.str());

    XAie_Events counterEvent;
    // Step 1: Configure the graph iterator event
    configStartIteration(core, iterCount, counterEvent);

    // Step 2: Configure the brodcast of the returned counter event
    XAie_Events bcChannelEvent;
    configEventBroadcast(aieDevInst, aieDevice, metadata, loc, module_type::core, metricSet, 
                         XAIE_CORE_MOD, counterEvent, bcChannelEvent, bcResourcesBytesTx);

    // Store the brodcasted channel event for later use
    bcEvent = bcChannelEvent;
    return true;
  }

  /****************************************************************************
   * Configure AIE Core module start on graph iteration count threshold
   ***************************************************************************/
  bool configStartIteration(xaiefal::XAieMod core, uint32_t iteration,
                            XAie_Events& retCounterEvent)
  {
    XAie_ModuleType mod = XAIE_CORE_MOD;
    // Count up by 1 for every iteration
    auto pc = core.perfCounter();
    if (pc->initialize(mod, XAIE_EVENT_INSTR_EVENT_0_CORE,
                       mod, XAIE_EVENT_INSTR_EVENT_0_CORE) != XAIE_OK)
      return false;
    if (pc->reserve() != XAIE_OK)
      return false;

    pc->changeThreshold(iteration);

    XAie_Events counterEvent;
    pc->getCounterEvent(mod, counterEvent);

    if (pc->start() != XAIE_OK)
      return false;
    
    // performance counter event to use it later for broadcasting
    retCounterEvent = counterEvent;
    return true;
  }

  /****************************************************************************
   * Configure the broadcasting of provided module and event
   * (Brodcasted from AIE Tile core module)
   ***************************************************************************/
  void configEventBroadcast(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice,
                            std::shared_ptr<AieProfileMetadata> metadata,
                            const XAie_LocType loc, const module_type xdpModType, 
                            const std::string metricSet, const XAie_ModuleType xaieModType, 
                            const XAie_Events bcEvent, XAie_Events& bcChannelEvent,
                            std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesBytesTx)
  {
    auto bcPair = aie::profile::getPreferredPLBroadcastChannel();

    std::vector<XAie_LocType> vL;
    AieRC RC = AieRC::XAIE_OK;

    auto allIntfTiles = metadata->getInterfaceTiles("all", "all", METRIC_BYTE_COUNT);
    if (allIntfTiles.empty())
      return;

    for (auto &tile : allIntfTiles) {
      vL.push_back(XAie_TileLoc(tile.col, tile.row));
    }

    auto BC = aieDevice->broadcast(vL, XAIE_PL_MOD, XAIE_PL_MOD);
    if (!BC)
      return;

    bcResourcesBytesTx.push_back(BC);
    BC->setPreferredId(bcPair.first);
    
    RC = BC->reserve();
    if (RC != XAIE_OK)
      return;

    RC = BC->start();
    if (RC != XAIE_OK)
      return;

    uint8_t bcId = BC->getBc();
    XAie_Events channelEvent;
    RC = BC->getEvent(vL.front(), XAIE_PL_MOD, channelEvent);
    if (RC != XAIE_OK)
      return;

    uint8_t brodcastId = bcId;
    int driverStatus   = AieRC::XAIE_OK;
    driverStatus |= XAie_EventBroadcast(aieDevInst, loc, XAIE_CORE_MOD, brodcastId, bcEvent);
    if (driverStatus != XAIE_OK) {
      std::stringstream msg;
      msg << "Configuration of graph iteration event from core tile "<< +loc.Col << "," << +loc.Row
          << " is unavailable, graph ieration profiling will not be available.\n";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
      return;
    }

    // This is the broadcast channel event seen in interface tiles
    bcChannelEvent = channelEvent;
  }

  /****************************************************************************
   * Configure the individual AIE events for metric sets that use group events
   ***************************************************************************/
  void configGroupEvents(XAie_DevInst* aieDevInst, const XAie_LocType loc,
                         const XAie_ModuleType mod, const module_type type,
                         const std::string metricSet, const XAie_Events event,
                         const uint8_t channel) 
  {
    // Set masks for group events
    // NOTE: Group error enable register is blocked, so ignoring
    if (event == XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_DMA_MASK);
    else if (event == XAIE_EVENT_GROUP_LOCK_MEM)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_LOCK_MASK);
    else if (event == XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_CONFLICT_MASK);
    else if (event == XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_CORE_PROGRAM_FLOW_MASK);
    else if (event == XAIE_EVENT_GROUP_CORE_STALL_CORE)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_CORE_STALL_MASK);
    else if (event == XAIE_EVENT_GROUP_DMA_ACTIVITY_PL) {
      uint32_t bitMask = aie::isInputSet(type, metricSet) 
          ? ((channel == 0) ? GROUP_SHIM_S2MM0_STALL_MASK : GROUP_SHIM_S2MM1_STALL_MASK)
          : ((channel == 0) ? GROUP_SHIM_MM2S0_STALL_MASK : GROUP_SHIM_MM2S1_STALL_MASK);
      XAie_EventGroupControl(aieDevInst, loc, mod, event, bitMask);
    }                                    
  }

  /****************************************************************************
   * Configure the selection index to monitor channel number in memory tiles
   ***************************************************************************/
  void configEventSelections(XAie_DevInst* aieDevInst, const XAie_LocType loc,
                             const module_type type, const std::string metricSet,
                             const uint8_t channel)
  {
    if (type != module_type::mem_tile)
      return;

    XAie_DmaDirection dmaDir = aie::isInputSet(type, metricSet) ? DMA_S2MM : DMA_MM2S;
    XAie_EventSelectDmaChannel(aieDevInst, loc, 0, dmaDir, channel);

    std::stringstream msg;
    msg << "Configured mem tile " << (aie::isInputSet(type,metricSet) ? "S2MM" : "MM2S") 
    << "DMA  for metricset " << metricSet << ", channel " << (int)channel << ".";
    xrt_core::message::send(severity_level::debug, "XRT", msg.str());
  } 

  /****************************************************************************
   * Configure counters in Microblaze Debug Module (MDM)
   * TODO: convert to using XAie_Events once support is available from driver
   ***************************************************************************/
  void configMDMCounters(XAie_DevInst* aieDevInst, uint8_t col, uint8_t row, 
                         const std::vector<uint32_t> events)
  {
    auto tileOffset = XAie_GetTileAddr(aieDevInst, row, col);
    
    // Use MDM protocol to program counters
    // 1. Reset to first counter
    XAie_Write32(aieDevInst, tileOffset + UC_MDM_PCCMDR, 1 << UC_MDM_PCCMDR_RESET_BIT);
    
    // 2. Write events for all counters
    for (auto event : events)
      XAie_Write32(aieDevInst, tileOffset + UC_MDM_PCCTRLR, event);

    // 3. Clear and start counters
    XAie_Write32(aieDevInst, tileOffset + UC_MDM_PCCMDR, 1 << UC_MDM_PCCMDR_CLEAR_BIT);
    XAie_Write32(aieDevInst, tileOffset + UC_MDM_PCCMDR, 1 << UC_MDM_PCCMDR_START_BIT);
  }

  /****************************************************************************
   * Read counters in Microblaze Debug Module (MDM)
   ***************************************************************************/
  void readMDMCounters(XAie_DevInst* aieDevInst, uint8_t col, uint8_t row, 
                       std::vector<uint64_t>& values)
  {
    auto tileOffset = XAie_GetTileAddr(aieDevInst, row, col);

    // Use MDM protocol to program counters
    // 1. Reset to first counter
    XAie_Write32(aieDevInst, tileOffset + UC_MDM_PCCMDR, 1 << UC_MDM_PCCMDR_RESET_BIT);

    // 2. Read status of all counters
    std::vector<bool> overflows;
    uint32_t numCounters = UC_NUM_EVENT_COUNTERS + UC_NUM_LATENCY_COUNTERS;
    for (uint32_t c=0; c < numCounters; ++c) {
      uint32_t val;
      XAie_Read32(aieDevInst, tileOffset + UC_MDM_PCSR, &val);
      bool overflow = (((val >> UC_MDM_PCSR_OVERFLOW_BIT) & 0x1) == 1);
      overflows.push_back(overflow);

      if ((val >> UC_MDM_PCSR_FULL_BIT) & 0x1) {
        std::cout << "Full bit of tile " << col << "," << row << " microcontroller counter " 
                  << c << " is high" << std::endl;
      }
    }

    // 3. Read values of event counters
    for (uint32_t c=0; c < UC_NUM_EVENT_COUNTERS; ++c) {
      uint32_t val;
      XAie_Read32(aieDevInst, tileOffset + UC_MDM_PCDRR, &val);
      uint64_t val2 = (overflows.at(c)) ? (val + OVERFLOW_32BIT) : val;
      values.push_back(val2);
    }

    // 4. Read four values from latency counter
    //    Read 1 - The number of times the event occurred
    //    Read 2 - The sum of each event latency
    //    Read 3 - The sum of each event latency squared
    //    Read 4 - 31:16 Minimum measured latency, 16 bits
    //             15:0  Maximum measured latency, 16 bits
    std::vector<uint32_t> latencyValues;
    for (uint32_t c=0; c < UC_MDM_PCDRR_LATENCY_READS; ++c) {
      uint32_t val;
      XAie_Read32(aieDevInst, tileOffset + UC_MDM_PCDRR, &val);
      uint64_t val2 = (overflows.at(UC_NUM_EVENT_COUNTERS)) ? (val + OVERFLOW_32BIT) : val;
      latencyValues.push_back(val2);
    }

    // 5. Calculate average latency
    // NOTE: for now, only report average (we also have min and max; see above)
    uint32_t numValues = latencyValues.at(0);
    uint32_t totalLatency = latencyValues.at(1);
    uint64_t avgLatency = (numValues == 0) ? 0 : (totalLatency / numValues);
    values.push_back(avgLatency);
  }

}  // namespace xdp
