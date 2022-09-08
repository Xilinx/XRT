/* Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#include <cstring>

#include "core/edge/include/sk_types.h"
#include "core/edge/user/shim.h"
#include "profile_event_configuration.h"
#include "xaiefal/xaiefal.hpp"
#include "core/common/time.h"
#include "xaiengine.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_profile_new/x86/aie_profile_kernel_config.h"

extern "C"{
#include <xaiengine/xaiegbl_params.h>
}

constexpr unsigned int BASE_MEMORY_COUNTER = 128;
constexpr unsigned int BASE_SHIM_COUNTER =   256;

constexpr uint32_t GROUP_DMA_MASK                   = 0x0000f000;
constexpr uint32_t GROUP_LOCK_MASK                  = 0x55555555;
constexpr uint32_t GROUP_CONFLICT_MASK              = 0x000000ff;
constexpr uint32_t GROUP_ERROR_MASK                 = 0x00003fff;
constexpr uint32_t GROUP_STREAM_SWITCH_IDLE_MASK    = 0x11111111;
constexpr uint32_t GROUP_STREAM_SWITCH_RUNNING_MASK = 0x22222222;
constexpr uint32_t GROUP_STREAM_SWITCH_STALLED_MASK = 0x44444444;
constexpr uint32_t GROUP_STREAM_SWITCH_TLAST_MASK   = 0x88888888;
constexpr uint32_t GROUP_CORE_PROGRAM_FLOW_MASK     = 0x00001FE0;
constexpr uint32_t GROUP_CORE_STALL_MASK            = 0x0000000F;

// User private data structure container (context object) definition
class xrtHandles : public pscontext
{
  public:
    XAie_DevInst* aieDevInst = nullptr;
    xaiefal::XAieDev* aieDev = nullptr;
    xclDeviceHandle handle = nullptr;
    std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> mPerfCounters;
    std::vector<xdp::built_in::PSCounterInfo> counterData;

    xrtHandles() = default;
    ~xrtHandles()
    {
      // aieDevInst is not owned by xrtHandles, so don't delete here
      if (aieDev != nullptr)
        delete aieDev;
      // handle is not owned by xrtHandles, so don't close or delete here
    }
};

// Anonymous namespace for helper functions used in this file
namespace {
  using tile_type = xrt_core::edge::aie::tile_type;
  using CoreMetrics = xdp::built_in::CoreMetrics;
  using MemoryMetrics = xdp::built_in::MemoryMetrics;
  using InterfaceMetrics = xdp::built_in::InterfaceMetrics;

  std::vector<tile_type> processTiles(const xdp::built_in::ProfileInputConfiguration* params){
    std::vector<tile_type> tiles;

    for (int i = 0; i < params->numTiles; i++) {
      std::cout << "Tile Added!" << std::endl;
      tiles.push_back(tile_type());
      tiles[i].row = params->tiles[i].row;
      tiles[i].col = params->tiles[i].col;
      tiles[i].itr_mem_row = params->tiles[i].itr_mem_row;
      tiles[i].itr_mem_col = params->tiles[i].itr_mem_col;
      tiles[i].itr_mem_addr = params->tiles[i].itr_mem_addr;
      tiles[i].is_trigger = params->tiles[i].is_trigger;
      // tiles.push_back(params->tiles[i]);
      // std::cout << "original row: " << params->tiles[i].row << std::endl;
      // std::cout << "original col: " << params->tiles[i].col << std::endl;
      // std::cout << "new row: " << tiles[i].row << std::endl;
      // std::cout << "new col: " << tiles[i].col << std::endl;
    }

    return tiles;
  }

  uint32_t getNumFreeCtr(xaiefal::XAieDev* aieDevice,
                                             const std::vector<tile_type>& tiles,
                                             const XAie_ModuleType mod,
                                             const uint8_t metricSet,
                                             EventConfiguration& config)
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

    auto requestedEvents = (mod == XAIE_CORE_MOD) ? config.mCoreStartEvents[static_cast<CoreMetrics>(metricSet)]
                         : ((mod == XAIE_MEM_MOD) ? config.mMemoryStartEvents[static_cast<MemoryMetrics>(metricSet)] 
                         : config.mShimStartEvents[static_cast<InterfaceMetrics>(metricSet)]);
    auto eventStrings    = (mod == XAIE_CORE_MOD) ? config.mCoreEventStrings[static_cast<CoreMetrics>(metricSet)]
                         : ((mod == XAIE_MEM_MOD) ? config.mMemoryEventStrings[static_cast<MemoryMetrics>(metricSet)] 
                         : config.mShimEventStrings[static_cast<InterfaceMetrics>(metricSet)]);

    auto numTotalEvents = requestedEvents.size();
    if (numFreeCtr < numTotalEvents) {
      std::stringstream msg;
      std::cout << "Not enough counters!" << std::endl;
      // msg << "Only " << numFreeCtr << " out of " << numTotalEvents
      //     << " metrics were available for "
      //     << moduleName << " profiling due to resource constraints. "
      //     << "AIE profiling uses performance counters which could be already used by AIE trace, ECC, etc."
      //     << std::endl;

      // msg << "Available metrics : ";
      // for (unsigned int i=0; i < numFreeCtr; i++) {
      //   msg << eventStrings[i] << " ";
      // }
      // msg << std::endl;

      // msg << "Unavailable metrics : ";
      // for (unsigned int i=numFreeCtr; i < numTotalEvents; i++) {
      //   msg << eventStrings[i] << " ";
      // }
      //@TODO SEND MESSAGE BACK TO HOST
      // xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      
      //if (tiles.size() > 0)
        //printTileModStats(aieDevice, tiles[tileId], mod);
    }

    return numFreeCtr;
  }

  void configGroupEvents(XAie_DevInst* aieDevInst,
                              const XAie_LocType loc,
                              const XAie_ModuleType mod,
                              const XAie_Events event)
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
  void configStreamSwitchPorts(XAie_DevInst* aieDevInst,
                                                   const tile_type& tile,
                                                   xaiefal::XAieTile& xaieTile,
                                                   const XAie_LocType loc,
                                                   const XAie_Events event,
                                                   const uint8_t metricSet)
  {
    // Currently only used to monitor trace and PL stream
    if ((static_cast<CoreMetrics>(metricSet) != CoreMetrics::AIE_TRACE) 
        && (static_cast<InterfaceMetrics>(metricSet) != InterfaceMetrics::INPUT_BANDWIDTHS)
        && (static_cast<InterfaceMetrics>(metricSet) != InterfaceMetrics::OUTPUT_BANDWIDTHS)
        && (static_cast<InterfaceMetrics>(metricSet) != InterfaceMetrics::PACKETS))
      return;

    if (static_cast<CoreMetrics>(metricSet) == CoreMetrics::AIE_TRACE) {
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
  uint32_t getCounterPayload(XAie_DevInst* aieDevInst, 
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

  int setMetrics(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice,
                 EventConfiguration& config,
                 const xdp::built_in::ProfileInputConfiguration* params,
                 std::vector<xdp::built_in::PSCounterInfo>& counterData,     
                 std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>>& mPerfCounters,
                 xdp::built_in::ProfileOutputConfiguration* outputcfg)
  {
    int counterId = 0;
    bool runtimeCounters = false;
    constexpr int NUM_MODULES = xdp::built_in::ProfileInputConfiguration::NUM_MODULES;


   int numCounters[NUM_MODULES] =
        {xdp::built_in::ProfileInputConfiguration::NUM_CORE_COUNTERS, 
         xdp::built_in::ProfileInputConfiguration::NUM_MEMORY_COUNTERS,
         xdp::built_in::ProfileInputConfiguration::NUM_SHIM_COUNTERS};
    XAie_ModuleType falModuleTypes[NUM_MODULES] = 
        {XAIE_CORE_MOD, XAIE_MEM_MOD, XAIE_PL_MOD};

    std::string moduleNames[NUM_MODULES] = {"aie", "aie_memory", "interface_tile"};

    // @TODO: Convert MetricSettings input ENUM to string
    // Configure core, memory, and shim counters
    for (int module=0; module < NUM_MODULES; ++module) {
      // std::string metricsStr = "Test!"; //params->metricSettings[module];
      uint8_t metricsStr = params->metricSettings[module];
      std::cout << "MetricStr: Module: " << (int)metricsStr << std::endl;
      if (metricsStr == 0){ // 0 means empty
        //@TODO Pass Message Back to Host
        //std::string metricMsg = "No metric set specified for " + moduleNames[module]
        //                      + ". Please specify tile_based_" + moduleNames[module] 
        //                      + "_metrics under \"AIE_profile_settings\" section in your xrt.ini.";
        //xrt_core::message::send(severity_level::warning, "XRT", metricMsg);
        continue;
      } else {
        //@TODO Pass Message Back to Host
        //std::string oldModName[NUM_MODULES] = {"core", "memory", "interface"};
        //std::string depMsg  = "The xrt.ini flag \"aie_profile_" + oldModName[module] + "_metrics\" is deprecated "
        //                      + " and will be removed in future release. Please use"
        //                      + " tile_based_" + moduleNames[module] + "_metrics"
        //                      + " under \"AIE_profile_settings\" section.";
        //xrt_core::message::send(severity_level::warning, "XRT", depMsg);
      }
      int NUM_COUNTERS       = numCounters[module];
      XAie_ModuleType mod    = falModuleTypes[module];
      std::string moduleName = moduleNames[module];
      auto metricSet         = metricsStr; //getMetricSet(mod, metricsStr);
      auto tiles             = processTiles(params);

       // Ask Resource manager for resource availability
      auto numFreeCounters   = getNumFreeCtr(aieDevice, tiles, mod, metricSet, config);
      if (numFreeCounters == 0)
        continue;

      // Get vector of pre-defined metrics for this set
      uint8_t resetEvent = 0;
      auto startEvents = (mod == XAIE_CORE_MOD) ? config.mCoreStartEvents[static_cast<CoreMetrics>(metricSet)]
                       : ((mod == XAIE_MEM_MOD) ? config.mMemoryStartEvents[static_cast<MemoryMetrics>(metricSet)] 
                       : config.mShimStartEvents[static_cast<InterfaceMetrics>(metricSet)]);
      auto endEvents   = (mod == XAIE_CORE_MOD) ? config.mCoreEndEvents[static_cast<CoreMetrics>(metricSet)]
                       : ((mod == XAIE_MEM_MOD) ? config.mMemoryEndEvents[static_cast<MemoryMetrics>(metricSet)] 
                       : config.mShimEndEvents[static_cast<InterfaceMetrics>(metricSet)]);

      int numTileCounters[NUM_COUNTERS+1] = {0};
      
      // Iterate over tiles and metrics to configure all desired counters
      for (auto& tile : tiles) {
        std::cout << "Tile info row: " << tile.row << std::endl;
        std::cout << "Tile info col: " << tile.col << std::endl; 
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
          
          configGroupEvents(aieDevInst, loc, mod, startEvent);
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



          // @TODO SEND MESSAG/DATA BACK TO HOST
          // Store counter info in database
          // std::string counterName = "AIE Counter " + std::to_string(counterId);
          // (db->getStaticInfo()).addAIECounter(deviceId, counterId, col, row, i,
          //     phyStartEvent, phyEndEvent, resetEvent, payload, clockFreqMhz, 
          //     moduleName, counterName);
          xdp::built_in::PSCounterInfo outputCounter;
          outputCounter.counterId = counterId;
          outputCounter.col = col;
          outputCounter.row = row;
          outputCounter.counterNum = i;
          outputCounter.startEvent = phyStartEvent;
          outputCounter.endEvent = phyEndEvent;
          outputCounter.resetEvent = resetEvent;
          outputCounter.payload = payload;
          outputCounter.moduleName = module;
          
          std::cout << "Creating Output Struct" << std::endl;
          std::cout << (uint64_t) col << std::endl;
          std::cout << (uint64_t) row << std::endl;
          std::cout << (uint64_t) i << std::endl;
          std::cout << (uint64_t) phyStartEvent << std::endl;
          std::cout << (uint64_t) phyEndEvent << std::endl;
          std::cout << (uint64_t) resetEvent << std::endl;
          std::cout << (uint64_t) payload << std::endl;
          std::cout << (uint64_t) module << std::endl;

          outputcfg->counters[counterId] = outputCounter;
          counterData.push_back(outputCounter);
          std::cout << "Finished the Counter" << std::endl;
          counterId++;
          numCounters++;
        }

        //@TODO SEND MESSAGE BACK TO HOST
        // std::stringstream msg;
        // msg << "Reserved " << numCounters << " counters for profiling AIE tile (" << col << "," << row << ").";
        // xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        // numTileCounters[numCounters]++;
      }

       // Report counters reserved per tile
      {
        //@TODO UPDATE DB and SEND MESSAGE BACK TO HOST.
        std::stringstream msg;
        msg << "AIE profile counters reserved in " << moduleName << " - ";
        for (int n=0; n <= NUM_COUNTERS; ++n) {
          //if (numTileCounters[n] == 0) continue;
          //msg << n << ": " << numTileCounters[n] << " tiles";
          //if (n != NUM_COUNTERS) msg << ", ";

          //(db->getStaticInfo()).addAIECounterResources(deviceId, n, numTileCounters[n], module);
        }
        //xrt_core::message::send(severity_level::info, "XRT", msg.str());
      }

      runtimeCounters = true;

    } // for module

    return runtimeCounters;
  }

  void pollAIECounters(XAie_DevInst* aieDevInst,
                        xdp::built_in::ProfileOutputConfiguration* countercfg,
                        std::vector<xdp::built_in::PSCounterInfo>& counterData,     
                        std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>>& mPerfCounters)
    {
    // Wait until xclbin has been loaded and device has been updated in database
    // if (!(db->getStaticInfo().isDeviceReady(index)))
    //   continue;

    if (!aieDevInst)
      return;

    uint32_t prevColumn = 0;
    uint32_t prevRow = 0;
    uint64_t timerValue = 0;

    // Iterate over all AIE Counters & Timers
    //auto numCounters = db->getStaticInfo().getNumAIECounter(index);
    auto numCounters = counterData.size();
    for (uint64_t c=0; c < numCounters; c++) {
      // auto aie = db->getStaticInfo().getAIECounter(index, c);
      // if (!aie)
      //   continue;

      xdp::built_in::PSCounterInfo pscfg;
      pscfg.col = counterData[c].col;
      pscfg.row = counterData[c].row;
      pscfg.startEvent = counterData[c].startEvent;
      pscfg.endEvent = counterData[c].endEvent;
      pscfg.resetEvent = counterData[c].resetEvent;
      
      // std::vector<uint64_t> values;
      // values.push_back(aie->column);
      // values.push_back(aie->row);
      // values.push_back(aie->startEvent);
      // values.push_back(aie->endEvent);
      // values.push_back(aie->resetEvent);

      // Read counter value from device
      uint32_t counterValue;
      if (mPerfCounters.empty()) {
        // Compiler-defined counters
        XAie_LocType tileLocation = XAie_TileLoc(counterData[c].col, counterData[c].row);
        XAie_PerfCounterGet(aieDevInst, tileLocation, XAIE_CORE_MOD, counterData[c].counterNum, &counterValue);
      }
      else {
        // Runtime-defined counters
        auto perfCounter = mPerfCounters.at(c);
        perfCounter->readResult(counterValue);
      }
      pscfg.counterValue = counterValue;
      // values.push_back(counterValue);

      // Read tile timer (once per tile to minimize overhead)
      if ((counterData[c].col != prevColumn) || (counterData[c].row != prevRow)) {
        prevColumn = counterData[c].col;
        prevRow = counterData[c].row;
        XAie_LocType tileLocation = XAie_TileLoc(counterData[c].col, counterData[c].row + 1);
        XAie_ReadTimer(aieDevInst, tileLocation, XAIE_CORE_MOD, &timerValue);
      }
      pscfg.timerValue = timerValue;
      pscfg.payload = counterData[c].payload;

      // values.push_back(timerValue);
      // values.push_back(aie->payload);

      // Get timestamp in milliseconds
      double timestamp = xrt_core::time_ns() / 1.0e6;
      // db->getDynamicInfo().addAIESample(index, timestamp, values);
      pscfg.timestamp = timestamp;
      countercfg->counters[c] = pscfg;

      std::cout << "Finished Writing to counter cfg" << std::endl;
      std::cout << "Col: " << pscfg.col << std::endl;
      std::cout << "Row: " << pscfg.row << std::endl;
      std::cout << "startEvent: " <<  pscfg.startEvent << std::endl;
      std::cout << "endEvent: " << pscfg.endEvent << std::endl;
      std::cout << "resetEvent: " << pscfg.resetEvent << std::endl;
      std::cout << "timerVal: " << pscfg.timerValue << std::endl;
      std::cout << "Payload: " << pscfg.payload << std::endl;
      std::cout << "counterValue: " << pscfg.counterValue << std::endl;
    }

  }


} // end anonymous namespace

#ifdef __cplusplus
extern "C" {
#endif

// The PS kernel initialization function
__attribute__((visibility("default")))
xrtHandles* aie_profile_config_init (xclDeviceHandle handle, const xuid_t xclbin_uuid) {

    xrtHandles* constructs = new xrtHandles;
    if (!constructs)
        return nullptr;
   
    constructs->handle = handle; 
    return constructs;
}

// The main PS kernel functionality
__attribute__((visibility("default")))
int aie_profile_config(uint8_t* input, uint8_t* output, uint8_t iteration, xrtHandles* constructs)
{
  if (constructs == nullptr)
    return 0;

  auto drv = ZYNQ::shim::handleCheck(constructs->handle);
  if(!drv)
    return 0;

  auto aieArray = drv->getAieArray();
  if (!aieArray)
    return 0;

  constructs->aieDevInst = aieArray->getDevInst();
  if (!constructs->aieDevInst)
    return 0;

  if (constructs->aieDev == nullptr)
    constructs->aieDev = new xaiefal::XAieDev(constructs->aieDevInst, false);


  std::cout << "I reached the main function!" << std::endl;
  if (iteration == 0) {

    xdp::built_in::ProfileInputConfiguration* params =
    reinterpret_cast<xdp::built_in::ProfileInputConfiguration*>(input);

    EventConfiguration config;
    config.initialize();
    // Using malloc/free instead of new/delete because the struct treats the
    // last element as a variable sized array
    std::size_t total_size = sizeof(xdp::built_in::ProfileOutputConfiguration)
     + sizeof(xdp::built_in::PSCounterInfo[params->numTiles * 4 - 1]);
    std::cout << "Total size of PSCounterInfo in iteration 0 is: " << std::endl;
    xdp::built_in::ProfileOutputConfiguration* outputcfg =
      (xdp::built_in::ProfileOutputConfiguration*)malloc(total_size);

    // tilecfg->numTiles = params->numTiles;
    std::cout << "Setting the Metrics!" << std::endl;
    int success = setMetrics(constructs->aieDevInst, constructs->aieDev,
                            config, params, constructs->counterData, constructs->mPerfCounters, outputcfg);
    uint8_t* out = reinterpret_cast<uint8_t*>(outputcfg);
    std::memcpy(output, out, total_size);   
    std::cout << "setMetrics Function finished with a value of : " << success << std::endl;
    // free(tilecfg); 
    free (outputcfg);
  } else if (iteration == 1) {
    std::cout << "Finished Iteration 1" << std::endl;
    std::size_t total_size = sizeof(xdp::built_in::ProfileOutputConfiguration)
     + (sizeof(xdp::built_in::PSCounterInfo) * (constructs->counterData.size() - 1));
    std::cout << "Total size of PSCounterInfo in iteration 0 is: " << std::endl;
    xdp::built_in::ProfileOutputConfiguration* outputcfg =
      (xdp::built_in::ProfileOutputConfiguration*)malloc(total_size);

    std::cout << "Polling!" << std::endl;
    pollAIECounters(constructs->aieDevInst, outputcfg, constructs->counterData, constructs->mPerfCounters);
    uint8_t* out = reinterpret_cast<uint8_t*>(outputcfg);
    std::memcpy(output, out, total_size);   
    std::cout << "PollAIE Function finished! with a value of"  << std::endl;
    free (outputcfg);

  }
  return 0;
}

// The final function for the PS kernel
__attribute__((visibility("default")))
int aie_profile_config_fini(xrtHandles* handles)
{
  if (handles != nullptr)
    delete handles;
  return 0;
}

#ifdef __cplusplus
}
#endif


