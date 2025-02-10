/**
 * Copyright (C) 2022-2025 Advanced Micro Devices, Inc. - All rights reserved
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

#include "xdp/profile/plugin/aie_profile/ve2/aie_profile.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"
#include "xdp/profile/plugin/aie_profile/util/aie_profile_util.h"
#include "xdp/profile/plugin/aie_profile/util/aie_profile_config.h"

#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/static_info/aie_constructs.h"

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <memory>
#include <cstring>
#include <map>

#include "core/common/message.h"
#include "core/common/time.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_metadata.h"
#include "shim/shim.h"

namespace {
  static void* fetchAieDevInst(void* devHandle)
  {
    auto drv = aiarm::shim::handleCheck(devHandle);
    if (!drv)
      return nullptr ;
    auto aieArray = drv->get_aie_array() ;
    if (!aieArray)
      return nullptr ;
    return aieArray->get_dev() ;
  }

  static void* allocateAieDevice(void* devHandle)
  {
    auto aieDevInst = static_cast<XAie_DevInst*>(fetchAieDevInst(devHandle)) ;
    if (!aieDevInst)
      return nullptr;
    return new xaiefal::XAieDev(aieDevInst, false) ;
  }

  static void deallocateAieDevice(void* aieDevice)
  {
    auto object = static_cast<xaiefal::XAieDev*>(aieDevice) ;
    if (object != nullptr)
      delete object ;
  }
} // end anonymous namespace

namespace xdp {
  using tile_type = xdp::tile_type;
  using module_type = xdp::module_type;
  using severity_level = xrt_core::message::severity_level;

  AieProfile_VE2Impl::AieProfile_VE2Impl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata)
      : AieProfileImpl(database, metadata)
  {
    auto hwGen = metadata->getHardwareGen();

    coreStartEvents = aie::profile::getCoreEventSets(hwGen);
    coreEndEvents = coreStartEvents;

    memoryStartEvents = aie::profile::getMemoryEventSets(hwGen);
    memoryEndEvents = memoryStartEvents;

    shimStartEvents = aie::profile::getInterfaceTileEventSets(hwGen);
    shimEndEvents = shimStartEvents;
    shimEndEvents[METRIC_BYTE_COUNT] = {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PERF_CNT_0_PL};

    memTileStartEvents = aie::profile::getMemoryTileEventSets(hwGen);
    memTileEndEvents = memTileStartEvents;
    
    microcontrollerEvents = aie::profile::getMicrocontrollerEventSets(hwGen);
  }

  bool AieProfile_VE2Impl::checkAieDevice(const uint64_t deviceId, void* handle)
  {
    aieDevInst = static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle)) ;
    aieDevice  = static_cast<xaiefal::XAieDev*>(db->getStaticInfo().getAieDevice(allocateAieDevice, deallocateAieDevice, handle)) ;
    if (!aieDevInst || !aieDevice) {
      xrt_core::message::send(severity_level::warning, "XRT", 
          "Unable to get AIE device. There will be no AIE profiling.");
      return false;
    }
    return true;
  }

  void AieProfile_VE2Impl::updateDevice() {

      if(!checkAieDevice(metadata->getDeviceID(), metadata->getHandle()))
              return;

      bool runtimeCounters = setMetricsSettings(metadata->getDeviceID(), metadata->getHandle());
  
      if (!runtimeCounters) {
        std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(metadata->getHandle());
        auto counters = xrt_core::edge::aie::get_profile_counters(device.get());

        if (counters.empty()) {
          xrt_core::message::send(severity_level::warning, "XRT", 
            "AIE Profile Counters were not found for this design. Please specify tile_based_[aie|aie_memory|interface_tile]_metrics under \"AIE_profile_settings\" section in your xrt.ini.");
          (db->getStaticInfo()).setIsAIECounterRead(metadata->getDeviceID(),true);
          return;
        }
        else {
          XAie_DevInst* aieDevInst =
            static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, metadata->getHandle()));

          for (auto& counter : counters) {
            tile_type tile;
            auto payload = getCounterPayload(aieDevInst, tile, module_type::core, counter.column, 
                                             counter.row, counter.startEvent, "N/A", 0);

            (db->getStaticInfo()).addAIECounter(metadata->getDeviceID(), counter.id, counter.column,
                counter.row, counter.counterNumber, counter.startEvent, counter.endEvent,
                counter.resetEvent, payload, counter.clockFreqMhz, counter.module, counter.name);
          }
        }
      }
  }

  // Get reportable payload specific for this tile and/or counter
  uint64_t 
  AieProfile_VE2Impl::getCounterPayload(XAie_DevInst* aieDevInst, 
                                         const tile_type& tile, 
                                         const module_type type, 
                                         uint8_t column, 
                                         uint8_t row, 
                                         uint16_t startEvent, 
                                         const std::string metricSet,
                                         const uint8_t channel)
  {
    // 1. Profile API specific values
    if (aie::profile::profileAPIMetricSet(metricSet))
      return getAdfProfileAPIPayload(tile, metricSet);
    
    // 2. Channel/stream IDs for interface tiles
    if (type == module_type::shim) {
      // NOTE: value = ((isMaster) << 8) & (isChannel << 7) & (channel/stream ID)
      auto portnum = aie::profile::getPortNumberFromEvent(static_cast<XAie_Events>(startEvent));
      uint8_t streamPortId = (portnum >= tile.stream_ids.size()) ?
          0 : static_cast<uint8_t>(tile.stream_ids.at(portnum));
      uint8_t idToReport = (tile.subtype == io_type::GMIO) ? channel : streamPortId;
      uint8_t isChannel  = (tile.subtype == io_type::GMIO) ? 1 : 0;
      uint8_t isMaster   = (portnum >= tile.is_master_vec.size()) ?
          0 : static_cast<uint8_t>(tile.is_master_vec.at(portnum));

      return ((isMaster << PAYLOAD_IS_MASTER_SHIFT)
             | (isChannel << PAYLOAD_IS_CHANNEL_SHIFT) | idToReport);
    }

    // 3. Channel IDs for memory tiles
    if (type == module_type::mem_tile) {
      // NOTE: value = ((isMaster) << 8) & (isChannel << 7) & (channel ID)
      uint8_t isChannel = 1;
      uint8_t isMaster = aie::isInputSet(type, metricSet) ? 1 : 0;
      return ((isMaster << PAYLOAD_IS_MASTER_SHIFT) 
             | (isChannel << PAYLOAD_IS_CHANNEL_SHIFT) | channel);
    }

    // 4. DMA BD sizes for AIE tiles
    // NOTE: value = ((max BD size) << 16) & ((isMaster) << 8) & (isChannel << 7) & (channel ID)
    uint8_t isChannel = 1;
    uint8_t isMaster  = aie::isInputSet(type, metricSet) ? 1 : 0;
    uint32_t payloadValue = ((isMaster << PAYLOAD_IS_MASTER_SHIFT) 
                            | (isChannel << PAYLOAD_IS_CHANNEL_SHIFT) | channel);

    if ((metadata->getHardwareGen() != 1)
        || ((startEvent != XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM)))
      return payloadValue;

    // Get average BD size for throughput calculations (AIE1 only)
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

    uint32_t maxBDSize = 0;
    auto tileOffset = XAie_GetTileAddr(aieDevInst, row, column);
    for (int bd = 0; bd < NUM_BDS; ++bd) {
      uint32_t regValue = 0;
      XAie_Read32(aieDevInst, tileOffset + offsets[bd], &regValue);
      
      if (regValue & valids[bd]) {
        uint32_t bdBytes = BYTES_PER_WORD * (((regValue >> lsbs[bd]) & masks[bd]) + ACTUAL_OFFSET);
        maxBDSize = std::max(bdBytes, maxBDSize);
      }
    }

    payloadValue |= (maxBDSize << PAYLOAD_BD_SIZE_SHIFT);
    return payloadValue;
  }
  
  uint64_t 
  AieProfile_VE2Impl::getAdfProfileAPIPayload(const tile_type& tile, const std::string metricSet)
  {
    if (metricSet == METRIC_LATENCY)
      return metadata->getIntfLatencyPayload(tile);

    return 0;
  }

  void AieProfile_VE2Impl::printTileModStats(xaiefal::XAieDev* aieDevice, 
      const tile_type& tile, XAie_ModuleType mod)
  {
    auto col = tile.col;
    auto row = tile.row;
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
    msg << "Resource usage stats for Tile : (" << +col << "," << +row 
        << ") Module : " << moduleName << std::endl;
    for (auto&g : groups) {
      auto stats = aieDevice->getRscStat(g);
      auto pc = stats.getNumRsc(loc, mod, xaiefal::XAIE_PERFCOUNT);
      auto ts = stats.getNumRsc(loc, mod, xaiefal::XAIE_TRACEEVENT);
      auto bc = stats.getNumRsc(loc, mod, xaiefal::XAIE_BROADCAST);
      msg << "Resource Group : " << std::left <<  std::setw(10) << g << " "
          << "Performance Counters : " << pc << " "
          << "Trace Slots : " << ts << " "
          << "Broadcast Channels : " << bc << " "
          << std::endl;
    }

    xrt_core::message::send(severity_level::info, "XRT", msg.str());
  }

  // Set metrics for all specified AIE counters on this device with configs given in AIE_profile_settings
  bool 
  AieProfile_VE2Impl::setMetricsSettings(const uint64_t deviceId, void* handle)
  {
    int counterId = 0;
    bool runtimeCounters = false;

    auto stats = aieDevice->getRscStat(XAIEDEV_DEFAULT_GROUP_AVAIL);
    auto hwGen = metadata->getHardwareGen();
    auto configChannel0 = metadata->getConfigChannel0();
    auto configChannel1 = metadata->getConfigChannel1();
    uint8_t startColShift = metadata->getPartitionOverlayStartCols().front();
    aie::displayColShiftInfo(startColShift);

    for (int module = 0; module < metadata->getNumModules(); ++module) {
      auto configMetrics = metadata->getConfigMetricsVec(module);
      if (configMetrics.empty())
        continue;
      
      int numTileCounters[metadata->getNumCountersMod(module)+1] = {0};
      XAie_ModuleType mod = aie::profile::getFalModuleType(module);
      
      // Iterate over tiles and metrics to configure all desired counters
      for (auto& tileMetric : configMetrics) {
        auto& metricSet  = tileMetric.second;
        auto tile        = tileMetric.first;
        auto col         = tile.col + startColShift;
        auto row         = tile.row;
        auto subtype     = tile.subtype;
        auto type        = aie::getModuleType(row, metadata->getAIETileRowOffset());
        if ((mod == XAIE_MEM_MOD) && (type == module_type::core))
          type = module_type::dma;
        
        // Catch microcontroller event sets for MDM
        if (module == static_cast<int>(module_type::uc)) {
          // Configure
          auto events = microcontrollerEvents[metricSet];
          aie::profile::configMDMCounters(aieDevInst, hwGen, col, row, events);
          // Record
          tile_type recordTile;
          recordTile.col = col;
          recordTile.row = row;
          microcontrollerTileEvents[recordTile] = events;
          runtimeCounters = true;
          continue;
        }

        // Ignore invalid types and inactive modules
        // NOTE: Inactive core modules are configured when utilizing
        //       stream switch monitor ports to profile DMA channels
        if (!aie::profile::isValidType(type, mod))
          continue;
        if ((type == module_type::dma) && !tile.active_memory)
          continue;
        if ((type == module_type::core) && !tile.active_core) {
          if (metadata->getPairModuleIndex(metricSet, type) < 0)
            continue;
        }

        auto loc         = XAie_TileLoc(col, row);
        auto& xaieTile   = aieDevice->tile(col, row);
        auto xaieModule  = (mod == XAIE_CORE_MOD) ? xaieTile.core()
                         : ((mod == XAIE_MEM_MOD) ? xaieTile.mem() 
                         : xaieTile.pl());

        auto startEvents = (type  == module_type::core) ? coreStartEvents[metricSet]
                         : ((type == module_type::dma)  ? memoryStartEvents[metricSet]
                         : ((type == module_type::shim) ? shimStartEvents[metricSet]
                         : memTileStartEvents[metricSet]));
        auto endEvents   = (type  == module_type::core) ? coreEndEvents[metricSet]
                         : ((type == module_type::dma)  ? memoryEndEvents[metricSet]
                         : ((type == module_type::shim) ? shimEndEvents[metricSet]
                         : memTileEndEvents[metricSet]));
        std::vector<XAie_Events> resetEvents = {};

        int numCounters  = 0;
        auto numFreeCtr  = stats.getNumRsc(loc, mod, xaiefal::XAIE_PERFCOUNT);
        numFreeCtr = (startEvents.size() < numFreeCtr) ? startEvents.size() : numFreeCtr;

        int numFreeCtrSS = numFreeCtr;
        if (aie::profile::profileAPIMetricSet(metricSet)) {
          if (numFreeCtr < 2) {
            continue;
          }
          // We need to monitor single stream switch monitor port
          // numFreeCtrSS = 1 ;
        }

        // Specify Sel0/Sel1 for memory tile events 21-44
        auto iter0 = configChannel0.find(tile);
        auto iter1 = configChannel1.find(tile);
        uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;
        uint8_t channel1 = (iter1 == configChannel1.end()) ? 1 : iter1->second;
        
        // Modify events as needed
        aie::profile::modifyEvents(type, subtype, channel0, startEvents, metadata->getHardwareGen());
        endEvents = startEvents;

        // TBD : Placeholder to configure AIE core with required profile counters.
        aie::profile::configEventSelections(aieDevInst, loc, type, metricSet, channel0);
        // TBD : Placeholder to configure shim tile with required profile counters.

        aie::profile::configStreamSwitchPorts(tileMetric.first, xaieTile, loc, type, 
            numFreeCtrSS, metricSet, channel0, channel1, startEvents, endEvents, streamPorts);
       
        // Identify the profiling API metric sets and configure graph events
        if (metadata->getUseGraphIterator() && !graphItrBroadcastConfigDone) {
          XAie_Events bcEvent = XAIE_EVENT_NONE_CORE;
          bool status = aie::profile::configGraphIteratorAndBroadcast(aieDevInst, aieDevice,
              metadata, xaieModule, loc, mod, type, metricSet, bcEvent, bcResourcesBytesTx);
          if (status) {
            graphIteratorBrodcastChannelEvent = bcEvent;
            graphItrBroadcastConfigDone = true;
          }
        }

        if (aie::profile::profileAPIMetricSet(metricSet)) {
          // Re-use the existing port running event for both the counters
          startEvents[startEvents.size()-1] = startEvents[0];
          
          // Use start events as End events for profile counters if threshold is not provided
          endEvents[endEvents.size()-1] = endEvents[0];

          // Use the set values broadcast events for the reset of counter
          resetEvents = {XAIE_EVENT_NONE_CORE, XAIE_EVENT_NONE_CORE};
          if (type == module_type::shim) {
            if (metadata->getUseGraphIterator())
              resetEvents = {graphIteratorBrodcastChannelEvent, graphIteratorBrodcastChannelEvent};
            else
              resetEvents = {XAIE_EVENT_NONE_CORE, XAIE_EVENT_USER_EVENT_1_PL};
          }
        }

        uint32_t threshold = 0;
        // Request and configure all available counters for this tile
        for (int i=0; i < numFreeCtr; ++i) {
          auto startEvent    = startEvents.at(i);
          auto endEvent      = endEvents.at(i);
          auto resetEvent    = XAIE_EVENT_NONE_CORE;
          auto portnum       = aie::profile::getPortNumberFromEvent(startEvent);
          uint8_t channel    = (portnum == 0) ? channel0 : channel1;

          // Configure group event before reserving and starting counter
          aie::profile::configGroupEvents(aieDevInst, loc, mod, type, metricSet, startEvent, channel);

          // Configure the profile counters for profile APIs metric sets.
          std::shared_ptr<xaiefal::XAiePerfCounter> perfCounter = nullptr;
          if (aie::profile::profileAPIMetricSet(metricSet)) {
            resetEvent = resetEvents.at(i);
            threshold = metadata->getUserSpecifiedThreshold(tileMetric.first, tileMetric.second);
            threshold = aie::profile::convertToBeats(tileMetric.second, threshold, metadata->getHardwareGen());

            if (i==0 && threshold>0)
              endEvent = XAIE_EVENT_PERF_CNT_1_PL;
              
            if (i==1 && threshold == 0)
              continue;
            
            XAie_Events retCounterEvent = XAIE_EVENT_NONE_CORE;
            perfCounter = aie::profile::configProfileAPICounters(aieDevInst, aieDevice, metadata, xaieModule, 
                            mod, type, metricSet, startEvent, endEvent, resetEvent, i, perfCounters.size(),
                            threshold, retCounterEvent, tile, bcResourcesLatency, adfAPIResourceInfoMap);
          }
          else {
            // Request counter from resource manager
            perfCounter = xaieModule.perfCounter();
            auto ret = perfCounter->initialize(mod, startEvent, mod, endEvent);
            if (ret != XAIE_OK) break;
            ret = perfCounter->reserve();
            if (ret != XAIE_OK) break;

            // Start the counter
            ret = perfCounter->start();
            if (ret != XAIE_OK) break;
          }
          if (!perfCounter)
            continue;
          perfCounters.push_back(perfCounter);

          // Generate user_event_1 for byte count metric set after configuration
          if ((metricSet == METRIC_BYTE_COUNT) && (i == 1) && !graphItrBroadcastConfigDone) {
            XAie_LocType tileloc = XAie_TileLoc(tile.col, tile.row);
            //Note: For BYTE_COUNT metric, user_event_1 is used twice as eventA & eventB to
            //      to transition the FSM from Idle->State0->State1.
            //      eventC = Port Running and eventD = stop event (counter event).
            XAie_EventGenerate(aieDevInst, tileloc, mod, XAIE_EVENT_USER_EVENT_1_PL);
            XAie_EventGenerate(aieDevInst, tileloc, mod, XAIE_EVENT_USER_EVENT_1_PL);
          }

          // Convert enums to physical event IDs for reporting purposes
          auto physicalEventIds  = aie::profile::getEventPhysicalId(aieDevInst, loc, mod, type, metricSet, 
                                                                    startEvent, endEvent);
          uint16_t phyStartEvent = physicalEventIds.first;
          uint16_t phyEndEvent   = physicalEventIds.second;

          // Get payload for reporting purposes
          uint64_t payload = getCounterPayload(aieDevInst, tileMetric.first, type, col, row, 
                                               startEvent, metricSet, channel);
          // Store counter info in database
          std::string counterName = "AIE Counter " + std::to_string(counterId);
          (db->getStaticInfo()).addAIECounter(deviceId, counterId, col, row, i,
                phyStartEvent, phyEndEvent, resetEvent, payload, metadata->getClockFreqMhz(), 
                metadata->getModuleName(module), counterName);
          counterId++;
          numCounters++;
        } // numFreeCtr

        std::stringstream msg;
        msg << "Reserved " << numCounters << " counters for profiling AIE tile (" << +col 
            << "," << +row << ") using metric set " << metricSet << ".";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        numTileCounters[numCounters]++;
      } // configMetrics
    
      // Report counters reserved per tile
      {
        std::stringstream msg;
        msg << "AIE profile counters reserved in " << metadata->getModuleName(module) << " - ";
        for (int n=0; n <= metadata->getNumCountersMod(module); ++n) {
          if (numTileCounters[n] == 0)
            continue;
          msg << n << ": " << numTileCounters[n] << " tiles, ";
          (db->getStaticInfo()).addAIECounterResources(deviceId, n, numTileCounters[n], module);
        }
        xrt_core::message::send(severity_level::info, "XRT", msg.str().substr(0, msg.str().size()-2));
      }

      runtimeCounters = true;
    } // modules

    return runtimeCounters;
  }

  void AieProfile_VE2Impl::poll(const uint32_t index, void* handle)
  {
    // Wait until xclbin has been loaded and device has been updated in database
    if (!(db->getStaticInfo().isDeviceReady(index)))
      return;
    XAie_DevInst* aieDevInst =
      static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle)) ;
    if (!aieDevInst)
      return;

    uint32_t prevColumn = 0;
    uint32_t prevRow = 0;
    uint64_t timerValue = 0;
    auto hwGen = metadata->getHardwareGen();

    // Iterate over all AIE Counters & Timers
    auto numCounters = db->getStaticInfo().getNumAIECounter(index);
    for (uint64_t c=0; c < numCounters; c++) {
      auto aie = db->getStaticInfo().getAIECounter(index, c);
      if (!aie)
        continue;

      std::vector<uint64_t> values;
      values.push_back(aie->column);
      values.push_back(aie::getRelativeRow(aie->row, metadata->getAIETileRowOffset()));
      values.push_back(aie->startEvent);
      values.push_back(aie->endEvent);
      values.push_back(aie->resetEvent);

      // Read counter value from device
      uint32_t counterValue;
      if (perfCounters.empty()) {
        // Compiler-defined counters
        XAie_LocType tileLocation = XAie_TileLoc(aie->column, aie->row);
        XAie_PerfCounterGet(aieDevInst, tileLocation, XAIE_CORE_MOD, aie->counterNumber, &counterValue);
      }
      else {
        // Runtime-defined counters
        if (aie::profile::adfAPILatencyConfigEvent(aie->startEvent))
        {
          uint32_t srcCounterValue = 0;
          uint32_t destCounterValue = 0;
          try {
            std::string srcDestPairKey = metadata->getSrcDestPairKey(aie->column, aie->row);
            uint8_t srcPcIdx = adfAPIResourceInfoMap.at(aie::profile::adfAPI::INTF_TILE_LATENCY).at(srcDestPairKey).srcPcIdx;
            uint8_t destPcIdx = adfAPIResourceInfoMap.at(aie::profile::adfAPI::INTF_TILE_LATENCY).at(srcDestPairKey).destPcIdx;
            auto srcPerfCount = perfCounters.at(srcPcIdx);
            auto destPerfCount = perfCounters.at(destPcIdx);
            srcPerfCount->readResult(srcCounterValue);
            destPerfCount->readResult(destCounterValue);
            counterValue = (destCounterValue > srcCounterValue) ? (destCounterValue-srcCounterValue) : (srcCounterValue-destCounterValue);
            uint64_t storedValue = adfAPIResourceInfoMap[aie::profile::adfAPI::INTF_TILE_LATENCY][srcDestPairKey].profileResult;
            if (counterValue != storedValue)
              adfAPIResourceInfoMap[aie::profile::adfAPI::INTF_TILE_LATENCY][srcDestPairKey].profileResult = counterValue;
          } catch(...) {
            continue;
          }
        }
        else if (aie::profile::adfAPIStartToTransferredConfigEvent(aie->startEvent))
        {
          try {
            std::string srcKey = "(" + aie::uint8ToStr(aie->column) + "," + aie::uint8ToStr(aie->row) + ")";
            uint8_t srcPcIdx = adfAPIResourceInfoMap.at(aie::profile::adfAPI::START_TO_BYTES_TRANSFERRED).at(srcKey).srcPcIdx;
            auto perfCounter = perfCounters.at(srcPcIdx);
            perfCounter->readResult(counterValue);
            uint64_t storedValue = adfAPIResourceInfoMap[aie::profile::adfAPI::START_TO_BYTES_TRANSFERRED][srcKey].profileResult;
            if (counterValue != storedValue)
              adfAPIResourceInfoMap[aie::profile::adfAPI::START_TO_BYTES_TRANSFERRED][srcKey].profileResult = counterValue;
          } catch(...) {
            continue;
          }
        } 
        else {
          auto perfCounter = perfCounters.at(c);
          perfCounter->readResult(counterValue);
        }
      }
      values.push_back(counterValue);

      // Read tile timer (once per tile to minimize overhead)
      if ((aie->column != prevColumn) || (aie->row != prevRow)) {
        prevColumn = aie->column;
        prevRow = aie->row;
        auto moduleType = aie::getModuleType(aie->row, metadata->getAIETileRowOffset());
        auto falModuleType =  (moduleType == module_type::core) ? XAIE_CORE_MOD 
                            : ((moduleType == module_type::shim) ? XAIE_PL_MOD 
                            : XAIE_MEM_MOD);
        XAie_LocType tileLocation = XAie_TileLoc(aie->column, aie->row);
        XAie_ReadTimer(aieDevInst, tileLocation, falModuleType, &timerValue);
      }
      values.push_back(timerValue);
      values.push_back(aie->payload);

      // Get timestamp in milliseconds
      double timestamp = xrt_core::time_ns() / 1.0e6;
      db->getDynamicInfo().addAIESample(index, timestamp, values);
    }

    // Read and record MDM counters (if available)
    // NOTE: all MDM counters in a given tile are sampled in same read sequence
    for (auto& ucTile : microcontrollerTileEvents) {
      auto tile = ucTile.first;
      auto events = ucTile.second;

      std::vector<uint64_t> counterValues;
      aie::profile::readMDMCounters(aieDevInst, hwGen, tile.col, tile.row, counterValues);

      double timestamp = xrt_core::time_ns() / 1.0e6;

      for (uint64_t c=0; c < counterValues.size(); c++) {
        std::vector<uint64_t> values;
        values.push_back(tile.col);
        values.push_back(0);
        values.push_back(events.at(c));
        values.push_back(events.at(c));
        values.push_back(0);
        values.push_back(counterValues.at(c));
      
        db->getDynamicInfo().addAIESample(index, timestamp, values);
      }
    }
  }

  void AieProfile_VE2Impl::freeResources() 
  {
    displayAdfAPIResults();
    for (auto& c : perfCounters){
      c->stop();
      c->release();
    }

    for (auto& c : streamPorts){
      c->stop();
      c->release();
    }

    for (auto &bc : bcResourcesBytesTx) {
      bc->stop();
      bc->release();
    }

    for (auto &bc : bcResourcesLatency) {
      bc->stop();
      bc->release();
    }
  }

  /****************************************************************************
   * Display start to bytes or latency results to output transcript
   ***************************************************************************/
  void AieProfile_VE2Impl::displayAdfAPIResults()
  {
    for (auto &adfAPIType : adfAPIResourceInfoMap) {
      if (adfAPIType.first == aie::profile::adfAPI::START_TO_BYTES_TRANSFERRED) {
        for (auto &adfApiResource : adfAPIType.second) {
          std::stringstream msg;
          msg << "Total start to bytes transferred for tile " << adfApiResource.first << " is " 
              << +adfApiResource.second.profileResult << " clock cycles for specified bytes.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
      else if (adfAPIType.first == aie::profile::adfAPI::INTF_TILE_LATENCY) {
        for(auto &adfApiResource : adfAPIType.second) {
          GraphPortPair graphPortPair;
          try {
            graphPortPair = metadata->getSrcDestGraphPair(adfApiResource.first);
          }
          catch (...) {
            continue;
          }

          std::stringstream msg;
          msg << "Total latency between " << graphPortPair.srcGraphName 
              << ":" << graphPortPair.srcGraphPort << " and "
              << graphPortPair.destGraphName << ":" << graphPortPair.destGraphPort 
              << " is " << +adfApiResource.second.profileResult << " clock cycles.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    }
  }

  }
