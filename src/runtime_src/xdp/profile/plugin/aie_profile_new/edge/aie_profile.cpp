/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <memory>
#include <cstring>

#include "core/common/message.h"
#include "core/common/xrt_profiling.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/plugin/aie_profile_new/aie_profile_metadata.h"

#include "core/include/xrt/xrt_kernel.h"

#include "aie_profile.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

  void AieProfile_EdgeImpl::updateDevice() {

      bool runtimeCounters = setMetricsSettings(metadata->getDeviceID(), metadata->getHandle());
      if(!runtimeCounters) 
        runtimeCounters = setMetrics(metadata->getDeviceID(), metadata->getHandle());

      if (!runtimeCounters) {
        std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);
        auto counters = xrt_core::edge::aie::get_profile_counters(device.get());

        if (counters.empty()) {
          xrt_core::message::send(severity_level::warning, "XRT", 
            "AIE Profile Counters were not found for this design. Please specify tile_based_[aie|aie_memory|interface_tile]_metrics under \"AIE_profile_settings\" section in your xrt.ini.");
          (db->getStaticInfo()).setIsAIECounterRead(deviceId,true);
          return;
        }
        else {
          XAie_DevInst* aieDevInst =
            static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle));

          for (auto& counter : counters) {
            tile_type tile;
            auto payload = getCounterPayload(aieDevInst, tile, counter.column, counter.row, 
                                             counter.startEvent);

            (db->getStaticInfo()).addAIECounter(deviceId, counter.id, counter.column,
                counter.row + 1, counter.counterNumber, counter.startEvent, counter.endEvent,
                counter.resetEvent, payload, counter.clockFreqMhz, counter.module, counter.name);
          }
        }
      }
  }

  uint32_t AieProfile_EdgeImpl::getNumFreeCtr(xaiefal::XAieDev* aieDevice,
                                             const std::vector<tile_type>& tiles,
                                             const XAie_ModuleType mod,
                                             const std::string& metricSet)
  {
    uint32_t numFreeCtr = 0;
    uint32_t tileId = 0;
    std::string moduleName = (mod == XAIE_CORE_MOD) ? "aie" 
                           : ((mod == XAIE_MEM_MOD) ? "aie_memory" 
                           : "interface_tile");
    auto stats = aieDevice->getRscStat(XAIEDEV_DEFAULT_GROUP_AVAIL);

    // Calculate number of free counters based on minimum available across tiles
    for (unsigned int i=0; i < tiles.size(); i++) {
      auto row = (mod == XAIE_PL_MOD) ? tiles[i].row : tiles[i].row + 1;
      auto loc = XAie_TileLoc(tiles[i].col, row);
      auto avail = stats.getNumRsc(loc, mod, XAIE_PERFCNT_RSC);
      if (i == 0) {
        numFreeCtr = avail;
      } else {
        if (avail < numFreeCtr) {
          numFreeCtr = avail;
          tileId = i;
        }
      }
    }

    auto requestedEvents = (mod == XAIE_CORE_MOD) ? mCoreStartEvents[metricSet]
                         : ((mod == XAIE_MEM_MOD) ? mMemoryStartEvents[metricSet] 
                         : mShimStartEvents[metricSet]);
    auto eventStrings    = (mod == XAIE_CORE_MOD) ? mCoreEventStrings[metricSet]
                         : ((mod == XAIE_MEM_MOD) ? mMemoryEventStrings[metricSet] 
                         : mShimEventStrings[metricSet]);

    auto numTotalEvents = requestedEvents.size();
    if (numFreeCtr < numTotalEvents) {
      std::stringstream msg;
      msg << "Only " << numFreeCtr << " out of " << numTotalEvents
          << " metrics were available for "
          << moduleName << " profiling due to resource constraints. "
          << "AIE profiling uses performance counters which could be already used by AIE trace, ECC, etc."
          << std::endl;

      msg << "Available metrics : ";
      for (unsigned int i=0; i < numFreeCtr; i++) {
        msg << eventStrings[i] << " ";
      }
      msg << std::endl;

      msg << "Unavailable metrics : ";
      for (unsigned int i=numFreeCtr; i < numTotalEvents; i++) {
        msg << eventStrings[i] << " ";
      }

      xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      
      if (tiles.size() > 0)
        printTileModStats(aieDevice, tiles[tileId], mod);
    }

    return numFreeCtr;
  }

  void AieProfile_EdgeImpl::configGroupEvents(XAie_DevInst* aieDevInst,
                                             const XAie_LocType loc,
                                             const XAie_ModuleType mod,
                                             const XAie_Events event,
                                             const std::string metricSet)
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
  }

    // Configure stream switch ports for monitoring purposes
  void AieProfile_EdgeImpl::configStreamSwitchPorts(XAie_DevInst* aieDevInst,
                                                   const tile_type& tile,
                                                   xaiefal::XAieTile& xaieTile,
                                                   const XAie_LocType loc,
                                                   const XAie_Events event,
                                                   const std::string metricSet)
  {
    // Currently only used to monitor trace and PL stream
    if ((metricSet != "aie_trace") && (metricSet != "input_bandwidths")
        && (metricSet != "output_bandwidths") && (metricSet != "packets"))
      return;

    if (metricSet == "aie_trace") {
      auto switchPortRsc = xaieTile.sswitchPort();
      auto ret = switchPortRsc->reserve();
      if (ret != AieRC::XAIE_OK)
        return;

      uint32_t rscId = 0;
      XAie_LocType tmpLoc;
      XAie_ModuleType tmpMod;
      switchPortRsc->getRscId(tmpLoc, tmpMod, rscId);
      uint8_t traceSelect = (event == XAIE_EVENT_PORT_RUNNING_0_CORE) ? 0 : 1;
      
      // Define stream switch port to monitor core or memory trace
      XAie_EventSelectStrmPort(aieDevInst, loc, rscId, XAIE_STRMSW_SLAVE, TRACE, traceSelect);
      return;
    }

    // Rest is support for PL/shim tiles
    auto switchPortRsc = xaieTile.sswitchPort();
    auto ret = switchPortRsc->reserve();
    if (ret != AieRC::XAIE_OK)
      return;

    uint32_t rscId = 0;
    XAie_LocType tmpLoc;
    XAie_ModuleType tmpMod;
    switchPortRsc->getRscId(tmpLoc, tmpMod, rscId);

    // Grab slave/master and stream ID
    // NOTE: stored in getTilesForProfiling() above
    auto slaveOrMaster = (tile.itr_mem_col == 0) ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
    auto streamPortId  = static_cast<uint8_t>(tile.itr_mem_row);

    // Define stream switch port to monitor PLIO 
    XAie_EventSelectStrmPort(aieDevInst, loc, rscId, slaveOrMaster, SOUTH, streamPortId);
  }

  // Get reportable payload specific for this tile and/or counter
  uint32_t AieProfile_EdgeImpl::getCounterPayload(XAie_DevInst* aieDevInst, 
      const tile_type& tile, uint16_t column, uint16_t row, uint16_t startEvent)
  {
    // First, catch stream ID for PLIO metrics
    // NOTE: value = ((master or slave) << 8) & (stream ID)
    if ((startEvent == XAIE_EVENT_PORT_RUNNING_0_PL)
        || (startEvent == XAIE_EVENT_PORT_TLAST_0_PL)
        || (startEvent == XAIE_EVENT_PORT_IDLE_0_PL)
        || (startEvent == XAIE_EVENT_PORT_STALLED_0_PL))
      return ((tile.itr_mem_col << 8) | tile.itr_mem_row);

    // Second, send DMA BD sizes
    if ((startEvent != XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM))
      return 0;

    uint32_t payloadValue = 0;

    constexpr int NUM_BDS = 8;
    constexpr uint32_t BYTES_PER_WORD = 4;
    constexpr uint32_t ACTUAL_OFFSET = 1;
    uint64_t offsets[NUM_BDS] = {XAIEGBL_MEM_DMABD0CTRL,            XAIEGBL_MEM_DMABD1CTRL,
                                 XAIEGBL_MEM_DMABD2CTRL,            XAIEGBL_MEM_DMABD3CTRL,
                                 XAIEGBL_MEM_DMABD4CTRL,            XAIEGBL_MEM_DMABD5CTRL,
                                 XAIEGBL_MEM_DMABD6CTRL,            XAIEGBL_MEM_DMABD7CTRL};
    uint32_t lsbs[NUM_BDS]    = {XAIEGBL_MEM_DMABD0CTRL_LEN_LSB,    XAIEGBL_MEM_DMABD1CTRL_LEN_LSB,
                                 XAIEGBL_MEM_DMABD2CTRL_LEN_LSB,    XAIEGBL_MEM_DMABD3CTRL_LEN_LSB,
                                 XAIEGBL_MEM_DMABD4CTRL_LEN_LSB,    XAIEGBL_MEM_DMABD5CTRL_LEN_LSB,
                                 XAIEGBL_MEM_DMABD6CTRL_LEN_LSB,    XAIEGBL_MEM_DMABD7CTRL_LEN_LSB};
    uint32_t masks[NUM_BDS]   = {XAIEGBL_MEM_DMABD0CTRL_LEN_MASK,   XAIEGBL_MEM_DMABD1CTRL_LEN_MASK,
                                 XAIEGBL_MEM_DMABD2CTRL_LEN_MASK,   XAIEGBL_MEM_DMABD3CTRL_LEN_MASK,
                                 XAIEGBL_MEM_DMABD4CTRL_LEN_MASK,   XAIEGBL_MEM_DMABD5CTRL_LEN_MASK,
                                 XAIEGBL_MEM_DMABD6CTRL_LEN_MASK,   XAIEGBL_MEM_DMABD7CTRL_LEN_MASK};
    uint32_t valids[NUM_BDS]  = {XAIEGBL_MEM_DMABD0CTRL_VALBD_MASK, XAIEGBL_MEM_DMABD1CTRL_VALBD_MASK,
                                 XAIEGBL_MEM_DMABD2CTRL_VALBD_MASK, XAIEGBL_MEM_DMABD3CTRL_VALBD_MASK,
                                 XAIEGBL_MEM_DMABD4CTRL_VALBD_MASK, XAIEGBL_MEM_DMABD5CTRL_VALBD_MASK,
                                 XAIEGBL_MEM_DMABD6CTRL_VALBD_MASK, XAIEGBL_MEM_DMABD7CTRL_VALBD_MASK};

    auto tileOffset = _XAie_GetTileAddr(aieDevInst, row + 1, column);
    for (int bd = 0; bd < NUM_BDS; ++bd) {
      uint32_t regValue = 0;
      XAie_Read32(aieDevInst, tileOffset + offsets[bd], &regValue);
      
      if (regValue & valids[bd]) {
        uint32_t bdBytes = BYTES_PER_WORD * (((regValue >> lsbs[bd]) & masks[bd]) + ACTUAL_OFFSET);
        payloadValue = std::max(bdBytes, payloadValue);
      }
    }

    return payloadValue;
  }

  void AieProfile_EdgeImpl::printTileModStats(xaiefal::XAieDev* aieDevice, 
      const tile_type& tile, XAie_ModuleType mod)
  {
    auto col = tile.col;
    auto row = tile.row + 1;
    auto loc = XAie_TileLoc(col, row);
    std::string moduleName = (mod == XAIE_CORE_MOD) ? "aie" 
                           : ((mod == XAIE_MEM_MOD) ? "aie_memory" 
                           : "interface_tile");
    const std::string groups[3] = {
      XAIEDEV_DEFAULT_GROUP_GENERIC,
      XAIEDEV_DEFAULT_GROUP_STATIC,
      XAIEDEV_DEFAULT_GROUP_AVAIL
    };

    std::stringstream msg;
    msg << "Resource usage stats for Tile : (" << col << "," << row << ") Module : " << moduleName << std::endl;
    for (auto&g : groups) {
      auto stats = aieDevice->getRscStat(g);
      auto pc = stats.getNumRsc(loc, mod, XAIE_PERFCNT_RSC);
      auto ts = stats.getNumRsc(loc, mod, xaiefal::XAIE_TRACE_EVENTS_RSC);
      auto bc = stats.getNumRsc(loc, mod, XAIE_BCAST_CHANNEL_RSC);
      msg << "Resource Group : " << std::left <<  std::setw(10) << g << " "
          << "Performance Counters : " << pc << " "
          << "Trace Slots : " << ts << " "
          << "Broadcast Channels : " << bc << " "
          << std::endl;
    }

    xrt_core::message::send(severity_level::info, "XRT", msg.str());
  }

  std::vector<tile_type>
  AieProfile_EdgeImpl::getAllTilesForCoreMemoryProfiling(const XAie_ModuleType mod,
                                                        const std::string &graph,
                                                        void* handle)
  {
    std::vector<tile_type> tiles;
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

    tiles = xrt_core::edge::aie::get_event_tiles(device.get(), graph,
                                 xrt_core::edge::aie::module_type::core);
    if (mod == XAIE_MEM_MOD) {
      auto dmaTiles = xrt_core::edge::aie::get_event_tiles(device.get(), graph,
          xrt_core::edge::aie::module_type::dma);
      std::move(dmaTiles.begin(), dmaTiles.end(), back_inserter(tiles));
    }
    return tiles;
  }

  std::vector<tile_type>
  AieProfile_EdgeImpl::getAllTilesForShimProfiling(void* handle, 
                        const std::string &metricsStr, 
                        int16_t channelId,
                        bool useColumn, uint32_t minCol, uint32_t maxCol)
  {
    std::vector<tile_type> tiles;

    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

    int plioCount = 0;
    auto plios = xrt_core::edge::aie::get_plios(device.get());
    for (auto& plio : plios) {
      auto isMaster = plio.second.slaveOrMaster;
      auto streamId = plio.second.streamId;
      auto shimCol  = plio.second.shimColumn;

      // If looking for specific ID, make sure it matches
      if ((channelId >= 0) && (channelId != streamId))
        continue;

      // Make sure it's desired polarity
      // NOTE: input = slave (data flowing from PLIO)
      //       output = master (data flowing to PLIO)
      if ((isMaster && (metricsStr == "input_bandwidths"))
          || (!isMaster && (metricsStr == "output_bandwidths")))
        continue;

      plioCount++;

      if (useColumn 
          && !( (minCol <= shimCol) && (shimCol <= maxCol) )) {
        // shimCol is not within minCol:maxCol range. So skip.
        continue;
      }

      xrt_core::edge::aie::tile_type tile;
      tile.col = shimCol;
      tile.row = 0;
      // Grab stream ID and slave/master (used in configStreamSwitchPorts())
      tile.itr_mem_col = isMaster;
      tile.itr_mem_row = streamId;
      tiles.push_back(tile);
    }
          
    if ((0 == plioCount) && (0 <= channelId)) {
      std::string msg = "No tiles used channel ID " + std::to_string(channelId)
                        + ". Please specify a valid channel ID.";
      xrt_core::message::send(severity_level::warning, "XRT", msg);
    }
    return tiles;
  }

    // Set metrics for all specified AIE counters on this device with configs given in AIE_profile_settings
  bool 
  AieProfile_EdgeImpl::setMetricsSettings(uint64_t deviceId, void* handle)
  {
    int counterId = 0;
    bool runtimeCounters = false;

    // Get AIE clock frequency
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);
    auto clockFreqMhz = xrt_core::edge::aie::get_clock_freq_mhz(device.get());

    // Currently supporting Core, Memory, Interface Tile metrics only. Need to add Memory Tile metrics
    constexpr int NUM_MODULES = 3;

    std::string moduleNames[NUM_MODULES] = {"aie", "aie_memory", "interface_tile"};

    int numCountersMod[NUM_MODULES] =
        {NUM_CORE_COUNTERS, NUM_MEMORY_COUNTERS, NUM_SHIM_COUNTERS};
    XAie_ModuleType falModuleTypes[NUM_MODULES] = 
        {XAIE_CORE_MOD, XAIE_MEM_MOD, XAIE_PL_MOD};

    // Get the metrics settings
    std::vector<std::string> metricsConfig;

    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_aie_metrics());
    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_aie_memory_metrics());
    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_interface_tile_metrics());
//    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_mem_tile_metrics());

    // Get the graph metrics settings
    std::vector<std::string> graphmetricsConfig;

    graphmetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_aie_metrics());
    graphmetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_aie_memory_metrics());
//    graphmetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_interface_tile_metrics());
//    graphmetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_mem_tile_metrics());

    // Process AIE_profile_settings metrics
    // Each of the metrics can have ; separated multiple values. Process and save all
    std::vector<std::vector<std::string>> metricsSettings(NUM_MODULES);
    std::vector<std::vector<std::string>> graphmetricsSettings(NUM_MODULES);

    mConfigMetrics.resize(NUM_MODULES);

    bool newConfigUsed = false;
    for(int module = 0; module < NUM_MODULES; ++module) {
      bool findTileMetric = false;
      if (!metricsConfig[module].empty()) {
        boost::replace_all(metricsConfig[module], " ", "");
        boost::split(metricsSettings[module], metricsConfig[module], boost::is_any_of(";"));
        findTileMetric = true;        
      } else {
#if 0
// Add warning later
// No need to add the warning message here, as all the tests are using configs under Debug
        std::string modName = moduleNames[module].substr(0, moduleNames[module].find(" "));
        std::string metricMsg = "No metric set specified for " + modName + " module. " +
                                "Please specify the AIE_profile_settings." + modName + "_metrics setting in your xrt.ini.";
        xrt_core::message::send(severity_level::warning, "XRT", metricMsg);
#endif
      }
      if ((module < graphmetricsConfig.size()) && !graphmetricsConfig[module].empty()) {
        /* interface_tile metrics is not supported for Graph based metrics.
         * Only aie and aie_memory are supported.
         */
        boost::replace_all(graphmetricsConfig[module], " ", "");
        boost::split(graphmetricsSettings[module], graphmetricsConfig[module], boost::is_any_of(";"));
        findTileMetric = true;        
      } else {
#if 0
// Add warning later
// No need to add the warning message here, as all the tests are using configs under Debug
        std::string modName = moduleNames[module].substr(0, moduleNames[module].find(" "));
        std::string metricMsg = "No graph metric set specified for " + modName + " module. " +
                                "Please specify the AIE_profile_settings.graph_" + modName + "_metrics setting in your xrt.ini.";
        xrt_core::message::send(severity_level::warning, "XRT", metricMsg);
#endif
      }
      if(findTileMetric) {
        newConfigUsed = true;

        if (XAIE_PL_MOD == falModuleTypes[module]) {
          getInterfaceConfigMetricsForTiles(module, 
                                       metricsSettings[module], 
                                       /* graphmetricsSettings[module], */
                                       handle);
        } else {
          getConfigMetricsForTiles(module, 
                                   metricsSettings[module], 
                                   graphmetricsSettings[module], 
                                   falModuleTypes[module],
                                   handle);
        }
      }
    }

    if (!newConfigUsed) {
      // None of the new style AIE profile metrics have been used. So check for old style.
      return false;
    }

    auto stats = aieDevice->getRscStat(XAIEDEV_DEFAULT_GROUP_AVAIL);

    for(int module = 0; module < NUM_MODULES; ++module) {
      int numTileCounters[numCountersMod[module]+1] = {0};
      XAie_ModuleType mod    = falModuleTypes[module];

      // Iterate over tiles and metrics to configure all desired counters
      for (auto& tileMetric : mConfigMetrics[module]) {
        int numCounters = 0;
        auto col = tileMetric.first.col;
        auto row = tileMetric.first.row;

        // NOTE: resource manager requires absolute row number
        auto loc        = (mod == XAIE_PL_MOD) ? XAie_TileLoc(col, 0) 
                        : XAie_TileLoc(col, row + 1);
        auto& xaieTile  = (mod == XAIE_PL_MOD) ? aieDevice->tile(col, 0) 
                        : aieDevice->tile(col, row + 1);
        auto xaieModule = (mod == XAIE_CORE_MOD) ? xaieTile.core()
                        : ((mod == XAIE_MEM_MOD) ? xaieTile.mem() 
                        : xaieTile.pl());

        auto numFreeCtr = stats.getNumRsc(loc, mod, XAIE_PERFCNT_RSC);
        
        auto startEvents = (mod == XAIE_CORE_MOD) ? mCoreStartEvents[tileMetric.second]
                         : ((mod == XAIE_MEM_MOD) ? mMemoryStartEvents[tileMetric.second] 
                         : mShimStartEvents[tileMetric.second]);
        auto endEvents   = (mod == XAIE_CORE_MOD) ? mCoreEndEvents[tileMetric.second]
                         : ((mod == XAIE_MEM_MOD) ? mMemoryEndEvents[tileMetric.second] 
                         : mShimEndEvents[tileMetric.second]);

        auto numTotalReqEvents = startEvents.size();
        if (numFreeCtr < numTotalReqEvents) {
          // warning
 #if 0
          std::stringstream msg;
          msg << "Only " << numFreeCtr << " out of " << numTotalEvents
              << " metrics were available for AIE "
              << moduleName << " profiling due to resource constraints. "
              << "AIE profiling uses performance counters which could be already used by AIE trace, ECC, etc."
              << std::endl;

          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
 #endif
        }
        for (int i=0; i < numFreeCtr; ++i) {

          // Get vector of pre-defined metrics for this set
          uint8_t resetEvent = 0;

          auto startEvent = startEvents.at(i);
          auto endEvent   = endEvents.at(i);

          // Request counter from resource manager
          auto perfCounter = xaieModule.perfCounter();
          auto ret = perfCounter->initialize(mod, startEvent, mod, endEvent);
          if (ret != XAIE_OK) break;
          ret = perfCounter->reserve();
          if (ret != XAIE_OK) break;
        
          configGroupEvents(aieDevInst, loc, mod, startEvent, tileMetric.second);
          configStreamSwitchPorts(aieDevInst, tileMetric.first, xaieTile, loc, startEvent, tileMetric.second);
        
          // Start the counters after group events have been configured
          ret = perfCounter->start();
          if (ret != XAIE_OK) break;
          mPerfCounters.push_back(perfCounter);

          // Convert enums to physical event IDs for reporting purposes
          uint8_t tmpStart;
          uint8_t tmpEnd;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, startEvent, &tmpStart);
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod,   endEvent, &tmpEnd);
          uint16_t phyStartEvent = (mod == XAIE_CORE_MOD) ? tmpStart
                                 : ((mod == XAIE_MEM_MOD) ? (tmpStart + BASE_MEMORY_COUNTER)
                                 : (tmpStart + BASE_SHIM_COUNTER));
          uint16_t phyEndEvent   = (mod == XAIE_CORE_MOD) ? tmpEnd
                                 : ((mod == XAIE_MEM_MOD) ? (tmpEnd + BASE_MEMORY_COUNTER)
                                 : (tmpEnd + BASE_SHIM_COUNTER));

          auto payload = getCounterPayload(aieDevInst, tileMetric.first, col, row, startEvent);

          // Store counter info in database
          std::string counterName = "AIE Counter " + std::to_string(counterId);
          (db->getStaticInfo()).addAIECounter(deviceId, counterId, col, row, i,
                phyStartEvent, phyEndEvent, resetEvent, payload, clockFreqMhz, 
                moduleNames[module], counterName);
          counterId++;
          numCounters++;
        }

        std::stringstream msg;
        msg << "Reserved " << numCounters << " counters for profiling AIE tile (" << col << "," << row << ").";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        numTileCounters[numCounters]++;
      }

      // Report counters reserved per tile
      {
        std::stringstream msg;
        msg << "AIE profile counters reserved in " << moduleNames[module] << " - ";
        for (int n=0; n <= numCountersMod[module]; ++n) {
          if (numTileCounters[n] == 0) continue;
          msg << n << ": " << numTileCounters[n] << " tiles";
          if (n != numCountersMod[module]) msg << ", ";

          (db->getStaticInfo()).addAIECounterResources(deviceId, n, numTileCounters[n], module);
        }
        xrt_core::message::send(severity_level::info, "XRT", msg.str());
      }

      runtimeCounters = true;
    } // modules

    return runtimeCounters;
  }

   // Set metrics for all specified AIE counters on this device
  bool AieProfile_EdgeImpl::setMetrics(uint64_t deviceId, void* handle)
  {
    int counterId = 0;
    bool runtimeCounters = false;
    constexpr int NUM_MODULES = 3;

    // Get AIE clock frequency
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);
    auto clockFreqMhz = xrt_core::edge::aie::get_clock_freq_mhz(device.get());

    auto interfaceMetricStr = xrt_core::config::get_aie_profile_interface_metrics();
    std::vector<std::string> interfaceVec;
    boost::split(interfaceVec, interfaceMetricStr, boost::is_any_of(":"));
    auto interfaceMetric = interfaceVec.at(0);
    try {mChannelId = std::stoi(interfaceVec.at(1));} catch (...) {mChannelId = -1;}

    int numCounters[NUM_MODULES] =
        {NUM_CORE_COUNTERS, NUM_MEMORY_COUNTERS, NUM_SHIM_COUNTERS};
    XAie_ModuleType falModuleTypes[NUM_MODULES] = 
        {XAIE_CORE_MOD, XAIE_MEM_MOD, XAIE_PL_MOD};
    std::string moduleNames[NUM_MODULES] = {"aie", "aie_memory", "interface_tile"};
    std::string metricSettings[NUM_MODULES] = 
        {xrt_core::config::get_aie_profile_core_metrics(),
         xrt_core::config::get_aie_profile_memory_metrics(),
         interfaceMetric};

    // Configure core, memory, and shim counters
    for (int module=0; module < NUM_MODULES; ++module) {
      std::string metricsStr = metricSettings[module];
      if (metricsStr.empty()){
        std::string metricMsg = "No metric set specified for " + moduleNames[module]
                              + ". Please specify tile_based_" + moduleNames[module] 
                              + "_metrics under \"AIE_profile_settings\" section in your xrt.ini.";
        xrt_core::message::send(severity_level::warning, "XRT", metricMsg);
        continue;
      } else {
        std::string oldModName[NUM_MODULES] = {"core", "memory", "interface"};
        std::string depMsg  = "The xrt.ini flag \"aie_profile_" + oldModName[module] + "_metrics\" is deprecated "
                              + " and will be removed in future release. Please use"
                              + " tile_based_" + moduleNames[module] + "_metrics"
                              + " under \"AIE_profile_settings\" section.";
        xrt_core::message::send(severity_level::warning, "XRT", depMsg);
      }
      int NUM_COUNTERS       = numCounters[module];
      XAie_ModuleType mod    = falModuleTypes[module];
      std::string moduleName = moduleNames[module];
      auto metricSet         = getMetricSet(mod, metricsStr);
      auto tiles             = getTilesForProfiling(mod, metricsStr, handle);

      // Ask Resource manager for resource availability
      auto numFreeCounters   = getNumFreeCtr(aieDevice, tiles, mod, metricSet);
      if (numFreeCounters == 0)
        continue;

      // Get vector of pre-defined metrics for this set
      uint8_t resetEvent = 0;
      auto startEvents = (mod == XAIE_CORE_MOD) ? mCoreStartEvents[metricSet]
                       : ((mod == XAIE_MEM_MOD) ? mMemoryStartEvents[metricSet] 
                       : mShimStartEvents[metricSet]);
      auto endEvents   = (mod == XAIE_CORE_MOD) ? mCoreEndEvents[metricSet]
                       : ((mod == XAIE_MEM_MOD) ? mMemoryEndEvents[metricSet] 
                       : mShimEndEvents[metricSet]);

      int numTileCounters[NUM_COUNTERS+1] = {0};
      
      // Iterate over tiles and metrics to configure all desired counters
      for (auto& tile : tiles) {
        int numCounters = 0;
        auto col = tile.col;
        auto row = tile.row;
        
        // NOTE: resource manager requires absolute row number
        auto loc        = (mod == XAIE_PL_MOD) ? XAie_TileLoc(col, 0) 
                        : XAie_TileLoc(col, row + 1);
        auto& xaieTile  = (mod == XAIE_PL_MOD) ? aieDevice->tile(col, 0) 
                        : aieDevice->tile(col, row + 1);
        auto xaieModule = (mod == XAIE_CORE_MOD) ? xaieTile.core()
                        : ((mod == XAIE_MEM_MOD) ? xaieTile.mem() 
                        : xaieTile.pl());
        
        for (int i=0; i < numFreeCounters; ++i) {
          auto startEvent = startEvents.at(i);
          auto endEvent   = endEvents.at(i);

          // Request counter from resource manager
          auto perfCounter = xaieModule.perfCounter();
          auto ret = perfCounter->initialize(mod, startEvent, mod, endEvent);
          if (ret != XAIE_OK) break;
          ret = perfCounter->reserve();
          if (ret != XAIE_OK) break;
          
          configGroupEvents(aieDevInst, loc, mod, startEvent, metricSet);
          configStreamSwitchPorts(aieDevInst, tile, xaieTile, loc, startEvent, metricSet);
          
          // Start the counters after group events have been configured
          ret = perfCounter->start();
          if (ret != XAIE_OK) break;
          mPerfCounters.push_back(perfCounter);

          // Convert enums to physical event IDs for reporting purposes
          uint8_t tmpStart;
          uint8_t tmpEnd;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, startEvent, &tmpStart);
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod,   endEvent, &tmpEnd);
          uint16_t phyStartEvent = (mod == XAIE_CORE_MOD) ? tmpStart
                                 : ((mod == XAIE_MEM_MOD) ? (tmpStart + BASE_MEMORY_COUNTER)
                                 : (tmpStart + BASE_SHIM_COUNTER));
          uint16_t phyEndEvent   = (mod == XAIE_CORE_MOD) ? tmpEnd
                                 : ((mod == XAIE_MEM_MOD) ? (tmpEnd + BASE_MEMORY_COUNTER)
                                 : (tmpEnd + BASE_SHIM_COUNTER));

          auto payload = getCounterPayload(aieDevInst, tile, col, row, startEvent);

          // Store counter info in database
          std::string counterName = "AIE Counter " + std::to_string(counterId);
          (db->getStaticInfo()).addAIECounter(deviceId, counterId, col, row, i,
              phyStartEvent, phyEndEvent, resetEvent, payload, clockFreqMhz, 
              moduleName, counterName);
          counterId++;
          numCounters++;
        }

        std::stringstream msg;
        msg << "Reserved " << numCounters << " counters for profiling AIE tile (" << col << "," << row << ").";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        numTileCounters[numCounters]++;
      }

      // Report counters reserved per tile
      {
        std::stringstream msg;
        msg << "AIE profile counters reserved in " << moduleName << " - ";
        for (int n=0; n <= NUM_COUNTERS; ++n) {
          if (numTileCounters[n] == 0) continue;
          msg << n << ": " << numTileCounters[n] << " tiles";
          if (n != NUM_COUNTERS) msg << ", ";

          (db->getStaticInfo()).addAIECounterResources(deviceId, n, numTileCounters[n], module);
        }
        xrt_core::message::send(severity_level::info, "XRT", msg.str());
      }

      runtimeCounters = true;
    } // for module

    return runtimeCounters;
  }

  void AieProfile_EdgeImpl::pollAIECounters(uint32_t index, void* handle)
  {
    auto it = mThreadCtrlMap.find(handle);
    if (it == mThreadCtrlMap.end())
      return;

    auto& should_continue = it->second;
    while (should_continue) {
      // Wait until xclbin has been loaded and device has been updated in database
      if (!(db->getStaticInfo().isDeviceReady(index)))
        continue;
      XAie_DevInst* aieDevInst =
        static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle)) ;
      if (!aieDevInst)
        continue;

      uint32_t prevColumn = 0;
      uint32_t prevRow = 0;
      uint64_t timerValue = 0;

      // Iterate over all AIE Counters & Timers
      auto numCounters = db->getStaticInfo().getNumAIECounter(index);
      for (uint64_t c=0; c < numCounters; c++) {
        auto aie = db->getStaticInfo().getAIECounter(index, c);
        if (!aie)
          continue;

        std::vector<uint64_t> values;
        values.push_back(aie->column);
        values.push_back(aie->row);
        values.push_back(aie->startEvent);
        values.push_back(aie->endEvent);
        values.push_back(aie->resetEvent);

        // Read counter value from device
        uint32_t counterValue;
        if (mPerfCounters.empty()) {
          // Compiler-defined counters
          XAie_LocType tileLocation = XAie_TileLoc(aie->column, aie->row + 1);
          XAie_PerfCounterGet(aieDevInst, tileLocation, XAIE_CORE_MOD, aie->counterNumber, &counterValue);
        }
        else {
          // Runtime-defined counters
          auto perfCounter = mPerfCounters.at(c);
          perfCounter->readResult(counterValue);
        }
        values.push_back(counterValue);

        // Read tile timer (once per tile to minimize overhead)
        if ((aie->column != prevColumn) || (aie->row != prevRow)) {
          prevColumn = aie->column;
          prevRow = aie->row;
          XAie_LocType tileLocation = XAie_TileLoc(aie->column, aie->row + 1);
          XAie_ReadTimer(aieDevInst, tileLocation, XAIE_CORE_MOD, &timerValue);
        }
        values.push_back(timerValue);
        values.push_back(aie->payload);

        // Get timestamp in milliseconds
        double timestamp = xrt_core::time_ns() / 1.0e6;
        db->getDynamicInfo().addAIESample(index, timestamp, values);
      }

      std::this_thread::sleep_for(std::chrono::microseconds(mPollingInterval));     
    }
  }

}