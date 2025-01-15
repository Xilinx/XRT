/* Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#include "profile_event_configuration.h"
#include "xaiengine.h"

#include "core/common/time.h"
#include "core/edge/include/pscontext.h"
#include "core/edge/common/aie_parser.h"
#include "core/edge/user/shim.h"
#include "xaiefal/xaiefal.hpp"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_profile/x86/aie_profile_kernel_config.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"

extern "C"{
#include <xaiengine/xaiegbl_params.h>
}

// User private data structure container (context object) definition
class xrtHandles : public pscontext
{
  public:
    XAie_DevInst* aieDevInst = nullptr;
    xaiefal::XAieDev* aieDev = nullptr;
    xclDeviceHandle handle = nullptr;
    std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> mPerfCounters;
    std::vector<std::shared_ptr<xaiefal::XAieStreamPortSelect>> mStreamPorts;
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
  using tile_type = xdp::tile_type;
  using CoreMetrics = xdp::built_in::CoreMetrics;
  using MemoryMetrics = xdp::built_in::MemoryMetrics;
  using InterfaceMetrics = xdp::built_in::InterfaceMetrics;

  std::map<tile_type, uint8_t> processMetrics(const xdp::built_in::ProfileInputConfiguration* params, uint8_t module){
  
    std::map<tile_type, uint8_t> tiles;

    for (int i = 0; i < params->numTiles; i++) {
      if (params->tiles[i].tile_mod == module){
        auto tile = tile_type();
        tile.row = params->tiles[i].row;
        tile.col = params->tiles[i].col;
        tile.stream_ids = params->tiles[i].stream_ids;
        tile.is_master_vec = params->tiles[i].is_master_vec;
        tile.itr_mem_addr = params->tiles[i].itr_mem_addr;
        tile.is_trigger = params->tiles[i].is_trigger;
        tiles.insert({tile, params->tiles[i].metricSet});
       
      }
    }  
    return tiles;
  }

  xdp::module_type getModuleType(uint16_t absRow, uint16_t offset, XAie_ModuleType mod)
  {
    if (absRow == 0)
      return xdp::module_type::shim;
    if (absRow < offset)
      return xdp::module_type::mem_tile;
    return ((mod == XAIE_CORE_MOD) ? xdp::module_type::core : xdp::module_type::dma);
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
    auto slaveOrMaster = (tile.is_master_vec.at(0) == 0) ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
    auto streamPortId  = static_cast<uint8_t>(tile.stream_ids.at(0));

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
      return ((tile.is_master_vec.at(0) << 8) | tile.stream_ids.at(0));

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

    auto tileOffset = XAie_GetTileAddr(aieDevInst, row, column);
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


  bool isValidType(xdp::module_type type, XAie_ModuleType mod)
  {
    if ((mod == XAIE_CORE_MOD) && ((type == xdp::module_type::core) 
        || (type == xdp::module_type::dma)))
      return true;
    if ((mod == XAIE_MEM_MOD) && ((type == xdp::module_type::dma) 
        || (type == xdp::module_type::mem_tile)))
      return true;
    if ((mod == XAIE_PL_MOD) && (type == xdp::module_type::shim)) 
      return true;
    return false;
  }

  bool setMetricsSettings(xrtHandles* constructs, 
                          EventConfiguration& config,
                          const xdp::built_in::ProfileInputConfiguration* params,
                          xdp::built_in::ProfileOutputConfiguration* outputcfg)
  
  {
    XAie_DevInst* aieDevInst = constructs->aieDevInst;
    xaiefal::XAieDev* aieDevice = constructs->aieDev;
    std::vector<xdp::built_in::PSCounterInfo>& counterData = constructs->counterData;
    std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>>& mPerfCounters = constructs->mPerfCounters;
    std::vector<std::shared_ptr<xaiefal::XAieStreamPortSelect>>& mStreamPorts = constructs->mStreamPorts;

    int counterId = 0;
    bool runtimeCounters = false;

    // Currently supporting Core, Memory, Interface Tile metrics only. Need to add Memory Tile metrics
    constexpr int NUM_MODULES = 3;

    std::string moduleNames[NUM_MODULES] = {"aie", "aie_memory", "interface_tile"};

    int numCountersMod[NUM_MODULES] =
        {xdp::built_in::ProfileInputConfiguration::NUM_CORE_COUNTERS,
        xdp::built_in::ProfileInputConfiguration::NUM_MEMORY_COUNTERS,
        xdp::built_in::ProfileInputConfiguration::NUM_SHIM_COUNTERS};
    XAie_ModuleType falModuleTypes[NUM_MODULES] = 
        {XAIE_CORE_MOD, XAIE_MEM_MOD, XAIE_PL_MOD};

    
    auto stats = aieDevice->getRscStat(XAIEDEV_DEFAULT_GROUP_AVAIL);

    for(int module = 0; module < NUM_MODULES; ++module) {
      int numCounters = 0;

      int numTileCounters[numCountersMod[module]+1] = {0};
      XAie_ModuleType mod    = falModuleTypes[module];

      auto mConfigMetrics = processMetrics(params, module);

      // Iterate over tiles and metrics to configure all desired counters
      for (auto& tileMetric : mConfigMetrics) {
        auto col = tileMetric.first.col;
        auto row = tileMetric.first.row;

        auto type = getModuleType(row, params->offset, mod);
        if (!isValidType(type, mod))
          continue;

        // NOTE: resource manager requires absolute row number
        auto loc        = XAie_TileLoc(col, row);
        auto& xaieTile  = aieDevice->tile(col, row);
        auto xaieModule = (mod == XAIE_CORE_MOD) ? xaieTile.core()
                        : ((mod == XAIE_MEM_MOD) ? xaieTile.mem() 
                        : xaieTile.pl());

        auto metricSet = tileMetric.second;
        
        std::vector<XAie_Events> startEvents = (mod == XAIE_CORE_MOD) ? config.mCoreStartEvents[static_cast<CoreMetrics>(metricSet)]
                         : ((mod == XAIE_MEM_MOD) ? config.mMemoryStartEvents[static_cast<MemoryMetrics>(metricSet)] 
                         : config.mShimStartEvents[static_cast<InterfaceMetrics>(metricSet)]);
        std::vector<XAie_Events> endEvents   = (mod == XAIE_CORE_MOD) ? config.mCoreEndEvents[static_cast<CoreMetrics>(metricSet)]
                         : ((mod == XAIE_MEM_MOD) ? config.mMemoryEndEvents[static_cast<MemoryMetrics>(metricSet)] 
                         : config.mShimEndEvents[static_cast<InterfaceMetrics>(metricSet)]);

        auto numFreeCtr = stats.getNumRsc(loc, mod, xaiefal::XAIE_PERFCOUNT);

        for (int i=0; i < numFreeCtr; ++i) {
          auto startEvent = startEvents.at(i);
          auto endEvent   = endEvents.at(i);

          uint8_t resetEvent = 0;

          // Request counter from resource manager
          auto perfCounter = xaieModule.perfCounter();
          auto ret = perfCounter->initialize(mod, startEvent, mod, endEvent);
          if (ret != XAIE_OK) break;
          ret = perfCounter->reserve();
          if (ret != XAIE_OK) break;
        
          configGroupEvents(aieDevInst, loc, mod, startEvent);
          configStreamSwitchPorts(aieDevInst, tileMetric.first, xaieTile, loc, startEvent, metricSet);
        
          // Start the counters after group events have been configured
          ret = perfCounter->start();
          if (ret != XAIE_OK) break;
          mPerfCounters.push_back(perfCounter);

          // Convert enums to physical event IDs for reporting purposes
          uint16_t tmpStart;
          uint16_t tmpEnd;
          XAie_EventLogicalToPhysicalConv_16(aieDevInst, loc, mod, startEvent, &tmpStart);
          XAie_EventLogicalToPhysicalConv_16(aieDevInst, loc, mod,   endEvent, &tmpEnd);
          uint16_t phyStartEvent = tmpStart + config.mCounterBases[type];
          uint16_t phyEndEvent   = tmpEnd + config.mCounterBases[type];

          auto payload = getCounterPayload(aieDevInst, tileMetric.first, col, row, startEvent);

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

          counterId++;
          numCounters++;
          counterData.push_back(outputCounter);

          outputcfg->counters[counterId] = outputCounter;
          outputcfg->numCounters = counterId;
        }
      }
      runtimeCounters = true;
    } // modules

    return runtimeCounters;
  }

  void pollAIECounters(xrtHandles* constructs, xdp::built_in::ProfileOutputConfiguration* countercfg)
    {
    XAie_DevInst* aieDevInst = constructs->aieDevInst;
    std::vector<xdp::built_in::PSCounterInfo>& counterData = constructs->counterData;
    std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>>& mPerfCounters = constructs->mPerfCounters;


    if (!aieDevInst)
      return;

    uint32_t prevColumn = 0;
    uint32_t prevRow = 0;
    uint64_t timerValue = 0;

    // Iterate over all AIE Counters & Timers
    //auto numCounters = db->getStaticInfo().getNumAIECounter(index);
    auto numCounters = counterData.size();
    countercfg->numCounters = numCounters;
    for (uint64_t c=0; c < numCounters; c++) {


      xdp::built_in::PSCounterInfo pscfg;
      pscfg.col = counterData[c].col;
      pscfg.row = counterData[c].row;
      pscfg.startEvent = counterData[c].startEvent;
      pscfg.endEvent = counterData[c].endEvent;
      pscfg.resetEvent = counterData[c].resetEvent;
      
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

      // Read tile timer (once per tile to minimize overhead)
      if ((counterData[c].col != prevColumn) || (counterData[c].row != prevRow)) {
        prevColumn = counterData[c].col;
        prevRow = counterData[c].row;
        XAie_LocType tileLocation = XAie_TileLoc(counterData[c].col, counterData[c].row);
        XAie_ReadTimer(aieDevInst, tileLocation, XAIE_CORE_MOD, &timerValue);
      }
      pscfg.timerValue = timerValue;
      pscfg.payload = counterData[c].payload;

      countercfg->counters[c] = pscfg;
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

  constructs->aieDevInst = aieArray->get_dev();
  if (!constructs->aieDevInst)
    return 0;

  if (constructs->aieDev == nullptr)
    constructs->aieDev = new xaiefal::XAieDev(constructs->aieDevInst, false);

  EventConfiguration config;
  config.initialize();
  
  // Run-time Setup Iteration
  if (iteration == 0) {
    xdp::built_in::ProfileInputConfiguration* params =
    reinterpret_cast<xdp::built_in::ProfileInputConfiguration*>(input);
    // Using malloc/free instead of new/delete because the struct treats the
    // last element as a variable sized array
    int total_tiles = params->numTiles;
    if (total_tiles == 0)
      return 1;
  
    std::size_t total_size = sizeof(xdp::built_in::ProfileOutputConfiguration)
     + sizeof(xdp::built_in::PSCounterInfo[total_tiles * 4 - 1]);
    xdp::built_in::ProfileOutputConfiguration* outputcfg =
      (xdp::built_in::ProfileOutputConfiguration*)malloc(total_size);
    
    bool success = setMetricsSettings(constructs, config, params, outputcfg);

    uint8_t* out = reinterpret_cast<uint8_t*>(outputcfg);
    std::memcpy(output, out, total_size);   
    free (outputcfg);

  // Polling Iteration
  } else if (iteration == 1) {
    if (constructs->counterData.size() == 0)
      return 1;
    
    std::size_t total_size = sizeof(xdp::built_in::ProfileOutputConfiguration)
     + (sizeof(xdp::built_in::PSCounterInfo) * (constructs->counterData.size() - 1));
    xdp::built_in::ProfileOutputConfiguration* outputcfg =
      (xdp::built_in::ProfileOutputConfiguration*)malloc(total_size);

    pollAIECounters(constructs, outputcfg);
    uint8_t* out = reinterpret_cast<uint8_t*>(outputcfg);
    std::memcpy(output, out, total_size);   
    free (outputcfg);
  
  //Cleanup Iteration
  } else if (iteration == 2) {
    for (auto& c : constructs->mPerfCounters){
      c->stop();
      c->release();
    }

    for (auto& c : constructs->mStreamPorts){
      c->stop();
      c->release();
    }
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
