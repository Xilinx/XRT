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

#include "xdp/profile/plugin/aie_profile/edge/aie_profile.h"
#include "xdp/profile/plugin/aie_profile/util/aie_profile_util.h"
#include "xdp/profile/plugin/aie_profile/util/aie_profile_config.h"

#include "xdp/profile/database/static_info/aie_util.h"

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <memory>
#include <cstring>
#include <map>

#include "core/common/message.h"
#include "core/common/time.h"
#include "core/edge/user/shim.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_metadata.h"

namespace {
  static void* fetchAieDevInst(void* devHandle)
  {
    auto drv = ZYNQ::shim::handleCheck(devHandle);
    if (!drv)
      return nullptr ;
    auto aieArray = drv->getAieArray() ;
    if (!aieArray)
      return nullptr ;
    return aieArray->getDevInst() ;
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

  AieProfile_EdgeImpl::AieProfile_EdgeImpl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata)
      : AieProfileImpl(database, metadata)
  {
    auto hwGen = metadata->getHardwareGen();

    coreStartEvents = aie::profile::getCoreEventSets(hwGen);
    coreEndEvents = coreStartEvents;

    memoryStartEvents = aie::profile::getMemoryEventSets(hwGen);
    memoryEndEvents = memoryStartEvents;

    shimStartEvents = aie::profile::getInterfaceTileEventSets(hwGen);
    shimEndEvents = shimStartEvents;

    memTileStartEvents = aie::profile::getMemoryTileEventSets();
    memTileEndEvents = memTileStartEvents;
  }

  bool AieProfile_EdgeImpl::checkAieDevice(const uint64_t deviceId, void* handle)
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

  void AieProfile_EdgeImpl::updateDevice() {

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

  uint8_t AieProfile_EdgeImpl::getPortNumberFromEvent(const XAie_Events event)
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


  // Configure stream switch ports for monitoring purposes
  // NOTE: Used to monitor streams: trace, interfaces, and memory tiles
  void
  AieProfile_EdgeImpl::configStreamSwitchPorts(XAie_DevInst* aieDevInst, const tile_type& tile,
                                               xaiefal::XAieTile& xaieTile, const XAie_LocType loc,
                                               const module_type type, const uint32_t numCounters,
                                               const std::string metricSet, const uint8_t channel0, 
                                               const uint8_t channel1, std::vector<XAie_Events>& startEvents, 
                                               std::vector<XAie_Events>& endEvents)
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
        else if (type == module_type::shim) {
          // Interface tiles (e.g., PLIO, GMIO)
          // Grab slave/master and stream ID
          auto slaveOrMaster = (tile.is_master == 0) ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
          auto streamPortId  = tile.stream_id;
          switchPortRsc->setPortToSelect(slaveOrMaster, SOUTH, streamPortId);

          if (aie::isDebugVerbosity()) {
            std::string typeName = (tile.is_master == 0) ? "slave" : "master"; 
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

  // Get reportable payload specific for this tile and/or counter
  uint32_t 
  AieProfile_EdgeImpl::getCounterPayload(XAie_DevInst* aieDevInst, 
                                         const tile_type& tile, 
                                         const module_type type, 
                                         uint8_t column, 
                                         uint8_t row, 
                                         uint16_t startEvent, 
                                         const std::string metricSet,
                                         const uint8_t channel)
  {
    // 1. Stream IDs for interface tiles
    if (type == module_type::shim) {
      // NOTE: value = ((master or slave) << 8) & (stream ID)
      return ((tile.is_master << 8) | tile.stream_id);
    }

    // 2. Channel IDs for memory tiles
    if (type == module_type::mem_tile) {
      // NOTE: value = ((master or slave) << 8) & (channel ID)
      uint8_t isMaster = aie::isInputSet(type, metricSet) ? 1 : 0;
      return ((isMaster << 8) | channel);
    }

    // 3. DMA BD sizes for AIE tiles
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

    auto tileOffset = _XAie_GetTileAddr(aieDevInst, row, column);
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
  AieProfile_EdgeImpl::setMetricsSettings(const uint64_t deviceId, void* handle)
  {
    int counterId = 0;
    bool runtimeCounters = false;

    auto stats = aieDevice->getRscStat(XAIEDEV_DEFAULT_GROUP_AVAIL);
    auto configChannel0 = metadata->getConfigChannel0();
    auto configChannel1 = metadata->getConfigChannel1();

    for (int module = 0; module < metadata->getNumModules(); ++module) {
      auto configMetrics = metadata->getConfigMetrics(module);
      if (configMetrics.empty())
        continue;
      
      int numTileCounters[metadata->getNumCountersMod(module)+1] = {0};
      XAie_ModuleType mod = aie::profile::getFalModuleType(module);
      
      // Iterate over tiles and metrics to configure all desired counters
      for (auto& tileMetric : configMetrics) {
        auto& metricSet  = tileMetric.second;
        auto tile        = tileMetric.first;
        auto col         = tile.col;
        auto row         = tile.row;
        auto subtype     = tile.subtype;
        auto type        = aie::getModuleType(row, metadata->getAIETileRowOffset());
        if ((mod == XAIE_MEM_MOD) && (type == module_type::core))
          type = module_type::dma;

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

        int numCounters  = 0;
        auto numFreeCtr  = stats.getNumRsc(loc, mod, xaiefal::XAIE_PERFCOUNT);
        numFreeCtr = (startEvents.size() < numFreeCtr) ? startEvents.size() : numFreeCtr;

        // Specify Sel0/Sel1 for memory tile events 21-44
        auto iter0 = configChannel0.find(tile);
        auto iter1 = configChannel1.find(tile);
        uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;
        uint8_t channel1 = (iter1 == configChannel1.end()) ? 1 : iter1->second;
        
        // Modify events as needed
        aie::profile::modifyEvents(type, subtype, channel0, startEvents, metadata->getHardwareGen());
        endEvents = startEvents;

        aie::profile::configEventSelections(aieDevInst, loc, type, metricSet, channel0);
        configStreamSwitchPorts(aieDevInst, tileMetric.first, xaieTile, loc, type, numFreeCtr, 
                                metricSet, channel0, channel1, startEvents, endEvents);

        // Request and configure all available counters for this tile
        for (int i=0; i < numFreeCtr; ++i) {
          auto startEvent    = startEvents.at(i);
          auto endEvent      = endEvents.at(i);
          uint8_t resetEvent = 0;
          auto portnum       = getPortNumberFromEvent(startEvent);
          uint8_t channel    = (portnum == 0) ? channel0 : channel1;

          // Configure group event before reserving and starting counter
          aie::profile::configGroupEvents(aieDevInst, loc, mod, type, metricSet, startEvent, channel);

          // Request counter from resource manager
          auto perfCounter = xaieModule.perfCounter();
          auto ret = perfCounter->initialize(mod, startEvent, mod, endEvent);
          if (ret != XAIE_OK) break;
          ret = perfCounter->reserve();
          if (ret != XAIE_OK) break;

          // Start the counter
          ret = perfCounter->start();
          if (ret != XAIE_OK) break;
          perfCounters.push_back(perfCounter);

          // Convert enums to physical event IDs for reporting purposes
          uint8_t tmpStart;
          uint8_t tmpEnd;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, startEvent, &tmpStart);
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod,   endEvent, &tmpEnd);
          uint16_t phyStartEvent = tmpStart + aie::profile::getCounterBase(type);
          uint16_t phyEndEvent   = tmpEnd   + aie::profile::getCounterBase(type);

          // Get payload for reporting purposes
          auto payload = getCounterPayload(aieDevInst, tileMetric.first, type, col, row, 
                                           startEvent, metricSet, channel);

          // Store counter info in database
          std::string counterName = "AIE Counter " + std::to_string(counterId);
          (db->getStaticInfo()).addAIECounter(deviceId, counterId, col, row, i,
                phyStartEvent, phyEndEvent, resetEvent, payload, metadata->getClockFreqMhz(), 
                metadata->getModuleName(module), counterName);
          counterId++;
          numCounters++;
        }

        std::stringstream msg;
        msg << "Reserved " << numCounters << " counters for profiling AIE tile (" << +col 
            << "," << +row << ") using metric set " << metricSet << ".";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        numTileCounters[numCounters]++;
      }
    
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

  void AieProfile_EdgeImpl::poll(const uint32_t index, void* handle)
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
        auto perfCounter = perfCounters.at(c);
        perfCounter->readResult(counterValue);
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
  }

  void AieProfile_EdgeImpl::freeResources() 
  {
    for (auto& c : perfCounters){
      c->stop();
      c->release();
    }

    for (auto& c : streamPorts){
      c->stop();
      c->release();
    }
  }

}
