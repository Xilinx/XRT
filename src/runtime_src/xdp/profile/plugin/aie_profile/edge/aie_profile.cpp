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
    shimEndEvents[METRIC_BYTE_COUNT] = {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PERF_CNT_0_PL};

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
          uint8_t streamPortId = (portnum >= tile.stream_ids.size()) ?
              0 : static_cast<uint8_t>(tile.stream_ids.at(portnum));
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
  uint64_t 
  AieProfile_EdgeImpl::getCounterPayload(XAie_DevInst* aieDevInst, 
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
      auto portnum = getPortNumberFromEvent(static_cast<XAie_Events>(startEvent));
      uint8_t streamPortId = (portnum >= tile.stream_ids.size()) ?
          0 : static_cast<uint8_t>(tile.stream_ids.at(portnum));
      uint8_t idToReport = (tile.subtype == io_type::GMIO) ? channel : streamPortId;
      uint8_t isChannel  = (tile.subtype == io_type::GMIO) ? 1 : 0;
      return ((tile.is_master << PAYLOAD_IS_MASTER_SHIFT) 
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
  AieProfile_EdgeImpl::getAdfProfileAPIPayload(const tile_type& tile, const std::string metricSet)
  {
    if (metricSet == METRIC_LATENCY)
      return metadata->getIntfLatencyPayload(tile);

    return 0;
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

        configStreamSwitchPorts(aieDevInst, tileMetric.first, xaieTile, loc, type, numFreeCtrSS, 
                                metricSet, channel0, channel1, startEvents, endEvents);
       
        // Identify the profiling API metric sets and configure graph events
        if (metadata->getUseGraphIterator() && !graphItrBroadcastConfigDone) {
          XAie_Events bcEvent = XAIE_EVENT_NONE_CORE;
          bool status = configGraphIteratorAndBroadcast(xaieModule,
              loc, mod, type, metricSet, metadata->getIterationCount(), bcEvent);
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
          XAie_Events resetEvent = XAIE_EVENT_NONE_CORE;
          auto portnum       = getPortNumberFromEvent(startEvent);
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
            perfCounter = configProfileAPICounters(xaieModule, mod, type,
                            metricSet, startEvent, endEvent, resetEvent, i, 
                            threshold, retCounterEvent, tile);
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
            XAie_EventGenerate(aieDevInst, tileloc, mod, XAIE_EVENT_USER_EVENT_1_PL);
          }

          // Convert enums to physical event IDs for reporting purposes
          auto physicalEventIds = getEventPhysicalId(loc, mod, type, metricSet, startEvent, endEvent);
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
  }

  void AieProfile_EdgeImpl::freeResources() 
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

  std::shared_ptr<xaiefal::XAiePerfCounter>
  AieProfile_EdgeImpl::configProfileAPICounters(xaiefal::XAieMod& xaieModule,
                           XAie_ModuleType& xaieModType, const module_type xdpModType,
                           const std::string& metricSet, XAie_Events startEvent,
                           XAie_Events endEvent, XAie_Events resetEvent,
                           int pcIndex, size_t threshold, XAie_Events& retCounterEvent,
                           const tile_type& tile)
  {
    if (xdpModType != module_type::shim)
      return nullptr;

    if (metricSet == METRIC_LATENCY && pcIndex==0) {
      bool isSourceTile = true;
      auto pc = configIntfLatency(xaieModule, xaieModType, xdpModType,
                               metricSet, startEvent, endEvent, resetEvent,
                               pcIndex, threshold, retCounterEvent, tile, isSourceTile);
      std::string srcDestPairKey = metadata->getSrcDestPairKey(tile.col, tile.row);
      if (isSourceTile) {
        adfAPIResourceInfoMap[aie::profile::adfAPI::INTF_TILE_LATENCY][srcDestPairKey].isSourceTile = true; 
        adfAPIResourceInfoMap[aie::profile::adfAPI::INTF_TILE_LATENCY][srcDestPairKey].srcPcIdx = perfCounters.size();
      }
      else {
        adfAPIResourceInfoMap[aie::profile::adfAPI::INTF_TILE_LATENCY][srcDestPairKey].destPcIdx = perfCounters.size();
      }
      return pc;
    }

    if (metricSet == METRIC_BYTE_COUNT && pcIndex==0) {
      auto pc = configPCUsingComboEvents(xaieModule, xaieModType, xdpModType,
                               metricSet, startEvent, endEvent, resetEvent,
                               pcIndex, threshold, retCounterEvent);
      std::string srcKey = "(" + aie::uint8ToStr(tile.col) + "," + aie::uint8ToStr(tile.row) + ")";
      adfAPIResourceInfoMap[aie::profile::adfAPI::START_TO_BYTES_TRANSFERRED][srcKey].srcPcIdx = perfCounters.size();
      adfAPIResourceInfoMap[aie::profile::adfAPI::START_TO_BYTES_TRANSFERRED][srcKey].isSourceTile = true;
      return pc;
    }

    // Request counter from resource manager
    auto pc = xaieModule.perfCounter();
    auto ret = pc->initialize(xaieModType, startEvent, 
                              xaieModType, endEvent);
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

  std::shared_ptr<xaiefal::XAiePerfCounter>
  AieProfile_EdgeImpl::configPCUsingComboEvents(xaiefal::XAieMod& xaieModule,
                           XAie_ModuleType& xaieModType, const module_type xdpModType,
                           const std::string& metricSet, XAie_Events startEvent,
                           XAie_Events endEvent, XAie_Events resetEvent,
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
    XAie_Events eventB = startEvent;
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

  std::shared_ptr<xaiefal::XAiePerfCounter>
  AieProfile_EdgeImpl::configIntfLatency(xaiefal::XAieMod& xaieModule,
                                         XAie_ModuleType& xaieModType, const module_type xdpModType,
                                         const std::string& metricSet, XAie_Events startEvent,
                                         XAie_Events endEvent, XAie_Events resetEvent, int pcIndex,
                                         size_t threshold, XAie_Events& retCounterEvent,
                                         const tile_type& tile, bool& isSource)
  {
   // Request combo event from xaie module
    auto pc = xaieModule.perfCounter();

    if (!metadata->isValidLatencyTile(tile))
      return nullptr;
    
    startEvent = XAIE_EVENT_USER_EVENT_0_PL;
    if (!metadata->isSourceTile(tile)) {
      auto bcPair = setupBroadcastChannel(tile);
      startEvent = bcPair.second;
      isSource = false;
    }

    auto ret = pc->initialize(xaieModType, startEvent, 
                              xaieModType, endEvent);
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
      auto bc_pair = setupBroadcastChannel(tile);
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
   * Configure the individual AIE events for metric sets related to Profile APIs
   ***************************************************************************/
   bool AieProfile_EdgeImpl::configGraphIteratorAndBroadcast(xaiefal::XAieMod core,
                      XAie_LocType loc, const XAie_ModuleType xaieModType,
                      const module_type xdpModType, const std::string metricSet,
                      uint32_t iterCount, XAie_Events& bcEvent)
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
      auto& xaieTile   = aieDevice->tile(col, row);
      core = xaieTile.core();
      loc = XAie_TileLoc(col, row);
    }

    std::stringstream msg;
    msg << "Configuring AIE profile start_to_bytes_transferred to start on iteration " << iterCount
        << " using core tile (" << +loc.Col << "," << +loc.Row << ").\n";
    xrt_core::message::send(severity_level::debug, "XRT", msg.str());

    XAie_Events counterEvent;
    // Step 1: Configure the graph iterator event
    configStartIteration(core, iterCount, counterEvent);

    // Step 2: Configure the brodcast of the returned counter event
    XAie_Events bcChannelEvent;
    configEventBroadcast(loc, module_type::core, metricSet, XAIE_CORE_MOD,
                         counterEvent, bcChannelEvent);

    // Store the brodcasted channel event for later use
    bcEvent = bcChannelEvent;
    return true;
  }

  /****************************************************************************
   * Configure AIE Core module start on graph iteration count threshold
   ***************************************************************************/
  bool AieProfile_EdgeImpl::configStartIteration(xaiefal::XAieMod core, uint32_t iteration,
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

    // performance counter event to use it later for broadcasting
    retCounterEvent = counterEvent;
    return true;
  }

  /****************************************************************************
   * Configure the broadcasting of provided module and event
   * (Brodcasted from AIE Tile core module)
   ***************************************************************************/
  void AieProfile_EdgeImpl::configEventBroadcast(const XAie_LocType loc,
                                                 const module_type xdpModType,
                                                 const std::string metricSet,
                                                 const XAie_ModuleType xaieModType,
                                                 const XAie_Events bcEvent,
                                                 XAie_Events& bcChannelEvent)
  {
    auto bcPair = aie::profile::getPreferredPLBroadcastChannel();

    std::vector<XAie_LocType> vL;
    AieRC RC = AieRC::XAIE_OK;

    // vL.push_back(loc);
    // aie::profile::getAllInterfaceTileLocs(vL);
    std::vector<tile_type> allIntfTiles = metadata->getInterfaceTiles("all", "all", METRIC_BYTE_COUNT);
    std::set<tile_type> allIntfTilesSet(allIntfTiles.begin(), allIntfTiles.end());
    if (allIntfTilesSet.empty())
      return;

    for (auto &tile : allIntfTilesSet) {
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
      msg <<"Configuration of graph iteration event from core tile "<< +loc.Col << ", " << +loc.Row
          <<" is unavailable, graph ieration profiling will not be available.\n";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
      return;
    }

    // This is the broadcast channel event seen in interface tiles
    bcChannelEvent = channelEvent;
  }

  std::pair<uint16_t, uint16_t>
  AieProfile_EdgeImpl::getEventPhysicalId(XAie_LocType& tileLoc,
                     XAie_ModuleType& xaieModType, module_type xdpModType,
                     const std::string& metricSet,
                     XAie_Events startEvent, XAie_Events endEvent)
  {
    if (aie::profile::profileAPIMetricSet(metricSet)) {
      uint16_t eventId = aie::profile::getAdfApiReservedEventId(metricSet);
      return std::make_pair(eventId, eventId);
    }

    uint8_t tmpStart;
    uint8_t tmpEnd;
    XAie_EventLogicalToPhysicalConv(aieDevInst, tileLoc, xaieModType, startEvent, &tmpStart);
    XAie_EventLogicalToPhysicalConv(aieDevInst, tileLoc, xaieModType,   endEvent, &tmpEnd);
    uint16_t phyStartEvent = tmpStart + aie::profile::getCounterBase(xdpModType);
    uint16_t phyEndEvent   = tmpEnd   + aie::profile::getCounterBase(xdpModType);
    return std::make_pair(phyStartEvent, phyEndEvent);
  }

  std::pair<int, XAie_Events>
  AieProfile_EdgeImpl::setupBroadcastChannel(const tile_type& currTileLoc)
  {
    tile_type srcTile = currTileLoc;
    if (!metadata->isSourceTile(currTileLoc))
      if (!metadata->getSourceTile(currTileLoc, srcTile))
        return {-1, XAIE_EVENT_NONE_CORE};
    
    if (adfAPIBroadcastEventsMap.find(srcTile) == adfAPIBroadcastEventsMap.end()) {
      // auto bcPair = aie::profile::getPreferredPLBroadcastChannel();
      auto bcPair = getPLBroadcastChannel(srcTile);
      if (bcPair.first == -1 || bcPair.second == XAIE_EVENT_NONE_CORE) {
        return {-1, XAIE_EVENT_NONE_CORE};
      }
      adfAPIBroadcastEventsMap[srcTile] = bcPair;
    }
    return adfAPIBroadcastEventsMap.at(srcTile);
  }

  std::pair<int, XAie_Events>
  AieProfile_EdgeImpl::getPLBroadcastChannel(const tile_type& srcTile)
  {
    std::pair<int, XAie_Events> rc(-1, XAIE_EVENT_NONE_PL);
    AieRC RC = AieRC::XAIE_OK;
    tile_type destTile;
    
    metadata->getDestTile(srcTile, destTile);
    auto& tile = aieDevice->tile(srcTile.col, srcTile.row);
    XAie_LocType srctileLocation  = XAie_TileLoc(srcTile.col, srcTile.row);
    XAie_LocType DesttileLocation = XAie_TileLoc(destTile.col, destTile.row);

    std::vector<XAie_LocType> vL;
    vL.push_back(srctileLocation);
    vL.push_back(DesttileLocation);
    XAie_ModuleType StartM = XAIE_PL_MOD;
	  XAie_ModuleType EndM = XAIE_PL_MOD;
    
    auto BC  = aieDevice->broadcast(vL, StartM, EndM);
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
    RC = BC->getEvent(DesttileLocation, XAIE_PL_MOD, bcEvent);
    if (RC != XAIE_OK)
      return rc;

    std::pair<int, XAie_Events> bcPairSelected = std::make_pair(bcId, bcEvent);
    return bcPairSelected;
  }

  void AieProfile_EdgeImpl::displayAdfAPIResults()
  {
    for(auto &adfAPIType : adfAPIResourceInfoMap) {
      if (adfAPIType.first == aie::profile::adfAPI::START_TO_BYTES_TRANSFERRED) {
        for(auto &adfApiResource : adfAPIType.second) {
          std::stringstream msg;
          msg << "Total start to bytes transferred for tile " << adfApiResource.first << " is " 
              << +adfApiResource.second.profileResult <<" clock cycles for specified bytes.";
          xrt_core::message::send(severity_level::info, "XRT", msg.str());
        }
      }
      else if(adfAPIType.first == aie::profile::adfAPI::INTF_TILE_LATENCY) {
        for(auto &adfApiResource : adfAPIType.second) {
          GraphPortPair graphPortPair;
          try {
            graphPortPair = metadata->getSrcDestGraphPair(adfApiResource.first);
          }
          catch (...) {
            continue;
          }
          std::stringstream msg;
          msg << "Total latency between specified first beat of " <<graphPortPair.srcGraphName << ":" <<graphPortPair.srcGraphPort
              << " to first beat of " <<graphPortPair.destGraphName << ":" <<graphPortPair.destGraphPort << " is " 
              << +adfApiResource.second.profileResult <<" clock cycles.";
          xrt_core::message::send(severity_level::info, "XRT", msg.str());
        }
      }
    }
  }


  }
