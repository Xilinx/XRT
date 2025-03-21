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

    if ((type == module_type::shim) && (tile.subtype == io_type::PLIO) &&
        (switchPortMap.size() < tile.stream_ids.size())) {
      std::string msg = "Interface tile " + std::to_string(tile.col) + " has more "
                      + "PLIO than can be monitored by metric set " + metricSet + ". Please "
                      + "run again with different profile settings or choose a different set.";
      xrt_core::message::send(severity_level::warning, "XRT", msg);
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
                           std::map<adfAPI, std::map<std::string, adfAPIResourceInfo>>& adfAPIResourceInfoMap,
                           std::map<std::string, std::pair<int, XAie_Events>>& adfAPIBroadcastEventsMap)
  {
    if (xdpModType != module_type::shim)
      return nullptr;

    if ((metricSet == METRIC_LATENCY) && (pcIndex == 0)) {
      bool isSourceTile = true;
      auto pc = configInterfaceLatency(aieDevInst, aieDevice, metadata, xaieModule, xaieModType, xdpModType, 
                                       metricSet, startEvent, endEvent, resetEvent, pcIndex, threshold, 
                                       retCounterEvent, tile, isSourceTile, bcResourcesLatency, adfAPIBroadcastEventsMap);
      std::string srcDestPairKey = metadata->getSrcDestPairKey(tile.col, tile.row, (tile.stream_ids.empty() ? 0 : tile.stream_ids[0]));
      if (isSourceTile) {
        std::string srcDestPairKey = metadata->getSrcDestPairKey(tile.col, tile.row, (tile.stream_ids.empty() ? 0 : tile.stream_ids[0]));
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
  getShimBroadcastChannel(xaiefal::XAieDev* aieDevice, const tile_type& srcTile, const tile_type& destTile,
                        std::shared_ptr<AieProfileMetadata> metadata,
                        std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesLatency)
  {
    std::pair<int, XAie_Events> rc(-1, XAIE_EVENT_NONE_PL);
    AieRC RC = AieRC::XAIE_OK;
    
    // Check if source and destination tiles are at same location
    if (srcTile.col == destTile.col && srcTile.row == destTile.row) {
      return std::make_pair(0, XAIE_EVENT_USER_EVENT_0_PL);
    }

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
                        std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesLatency,
                        std::map<std::string, std::pair<int, XAie_Events>>& adfAPIBroadcastEventsMap)
  {
    tile_type srcTile, destTile;
    
    metadata->getSrcTile(currTileLoc, srcTile);
    metadata->getDestTile(currTileLoc, destTile);
    std::string srcDestTileKey = metadata->getSrcDestPairKey(srcTile.col, srcTile.row, (srcTile.stream_ids.empty() ? 0 : srcTile.stream_ids[0]));
    
    if (adfAPIBroadcastEventsMap.find(srcDestTileKey) == adfAPIBroadcastEventsMap.end()) {
      auto bcPair = getShimBroadcastChannel(aieDevice, srcTile, destTile, metadata, bcResourcesLatency);
      if (bcPair.first == -1 || bcPair.second == XAIE_EVENT_NONE_CORE) {
        return {-1, XAIE_EVENT_NONE_CORE};
      }
      adfAPIBroadcastEventsMap[srcDestTileKey] = bcPair;
    }
    return adfAPIBroadcastEventsMap.at(srcDestTileKey);
  }

  /****************************************************************************
  * Get configured broadcast channels for destination tiles
  * NOTE: This function is queried by the source tile.
  ***************************************************************************/
  std::pair<int, XAie_Events>
  getSetBroadcastChannel(xaiefal::XAieDev* aieDevice, const tile_type& currTileLoc, 
                        std::shared_ptr<AieProfileMetadata> metadata,
                        std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesLatency,
                        std::map<std::string, std::pair<int, XAie_Events>>& adfAPIBroadcastEventsMap)
  {
    tile_type srcTile = currTileLoc;
    std::string srcDestTileKey = metadata->getSrcDestPairKey(srcTile.col, srcTile.row, (srcTile.stream_ids.empty() ? 0 : srcTile.stream_ids[0]));
    
    if (adfAPIBroadcastEventsMap.find(srcDestTileKey) == adfAPIBroadcastEventsMap.end()) {
      return {-1, XAIE_EVENT_NONE_CORE};
    }
    return adfAPIBroadcastEventsMap.at(srcDestTileKey);
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
                         std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesLatency,
                         std::map<std::string, std::pair<int, XAie_Events>>& adfAPIBroadcastEventsMap)
  {
   // Request combo event from xaie module
    auto pc = xaieModule.perfCounter();

    if (!metadata->isValidLatencyTile(tile))
      return nullptr;
    
    startEvent = XAIE_EVENT_USER_EVENT_0_PL;
    if (!metadata->isSourceTile(tile)) {
      auto bcPair = setupBroadcastChannel(aieDevice, tile, metadata, bcResourcesLatency, adfAPIBroadcastEventsMap);
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
      // Fetch the configured broadcast channel for destination tile.
      auto bc_pair = getSetBroadcastChannel(aieDevice, tile, metadata, bcResourcesLatency, adfAPIBroadcastEventsMap);
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

}  // namespace xdp
