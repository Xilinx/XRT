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
#include "event_configuration.h"
#include "xaiefal/xaiefal.hpp"
#include "xaiengine.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_trace_new/x86/aie_trace_kernel_config.h"


// User private data structure container (context object) definition
class xrtHandles : public pscontext
{
  public:
    XAie_DevInst* aieDevInst = nullptr;
    xaiefal::XAieDev* aieDev = nullptr;
    xclDeviceHandle handle = nullptr;
    std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> mPerfCounters;
    std::vector<AIECounter*> counterData;

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

  std::vector<tile_type> processTiles(const xdp::built_in::InputConfiguration* params){
    std::vector<tile_type> tiles;

    for (int i = 0; i < params->numTiles; i++) {
      tiles[i].push_back(params->tiles[i]);
    }
    return tiles;
  }

  uint32_t AIEProfilingPlugin::getNumFreeCtr(xaiefal::XAieDev* aieDevice,
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
      //@TODO SEND MESSAGE BACK TO HOST
      // xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      
      //if (tiles.size() > 0)
        //printTileModStats(aieDevice, tiles[tileId], mod);
    }

    return numFreeCtr;
  }

  int setMetrics(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice,
                 EventConfiguration& config,
                 const xdp::built_in::InputConfiguration* params,
                 std::vector<AIECounter> counterData)
  {
    int counterId = 0;
    bool runtimeCounters = false;
    constexpr int NUM_MODULES = xdp::built_in::InputConfiguration::NUM_MODULES;


   int numCounters[NUM_MODULES] =
        {xdp::built_in::InputConfiguration::NUM_CORE_COUNTERS, 
         xdp::built_in::InputConfiguration::NUM_MEMORY_COUNTERS,
         xdp::built_in::InputConfiguration::NUM_SHIM_COUNTERS};
    XAieModuleType falModuleTypes[NUM_MODULES] = 
        {XAIE_CORE_MOD, XAIE_MEM_MOD, XAIE_PL_MOD};

    std::string moduleNames[NUM_MODULES] = {"aie", "aie_memory", "interface_tile"};

    // @TODO: Convert MetricSettings input ENUM to string
    // Configure core, memory, and shim counters
    for (int module=0; module < NUM_MODULES; ++module) {
      std::string metricsStr = params->metricSettings[module];
      if (metricsStr.empty()){
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
      auto metricSet         = getMetricSet(mod, metricsStr);
      auto tiles             = processTiles(params);

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



          // @TODO SEND MESSAG/DATA BACK TO HOST
          // Store counter info in database
          // std::string counterName = "AIE Counter " + std::to_string(counterId);
          // (db->getStaticInfo()).addAIECounter(deviceId, counterId, col, row, i,
          //     phyStartEvent, phyEndEvent, resetEvent, payload, clockFreqMhz, 
          //     moduleName, counterName);
          counterId++;
          numCounters++;
        }

        //@TODO SEND MESSAGE BACK TO HOST
        // std::stringstream msg;
        // msg << "Reserved " << numCounters << " counters for profiling AIE tile (" << col << "," << row << ").";
        // xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        numTileCounters[numCounters]++;
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

  void AIEProfilingPlugin::pollAIECounters(XAie_DevInst* aieDevInst, uint32_t index,
                                           xdp::built_in::OutputConfiguration* countercfg,
                                           std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>>& mPerfCounters,
                                           std::vector<AIECounter>& counterData)
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
    auto numCounters = db->getStaticInfo().getNumAIECounter(index);
    for (uint64_t c=0; c < numCounters; c++) {
      // auto aie = db->getStaticInfo().getAIECounter(index, c);
      if (!aie)
        continue;

      PSCounterInfo pscfg = xdp::built_in::PSCounterInfo;
      pscfg.col = counterData[c]->column;
      pscfg.row = counterData[c]->row;
      pscfg.startEvent = counterData[c]->startEvent;
      pscfg.endEvent = counterData[c]->endEvent;
      pscfg.resetEvent = counterData[c]->resetEvent;
      
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
        XAie_LocType tileLocation = XAie_TileLoc(aie->column, aie->row + 1);
        XAie_PerfCounterGet(aieDevInst, tileLocation, XAIE_CORE_MOD, aie->counterNumber, &counterValue);
      }
      else {
        // Runtime-defined counters
        auto perfCounter = mPerfCounters.at(c);
        perfCounter->readResult(counterValue);
      }
      pscfg.counterValue = counterValue;
      // values.push_back(counterValue);

      // Read tile timer (once per tile to minimize overhead)
      if ((aie->column != prevColumn) || (aie->row != prevRow)) {
        prevColumn = aie->column;
        prevRow = aie->row;
        XAie_LocType tileLocation = XAie_TileLoc(aie->column, aie->row + 1);
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

  xdp::built_in::InputConfiguration* params =
    reinterpret_cast<xdp::built_in::InputConfiguration*>(input);

  EventConfiguration config;
  config.initialize(params);

  if (iteration == 0) {
    // Using malloc/free instead of new/delete because the struct treats the
    // last element as a variable sized array
    // std::size_t total_size = sizeof(xdp::built_in::OutputConfiguration) + sizeof(xdp::built_in::TileData[params->numTiles - 1]);
    // xdp::built_in::OutputConfiguration* tilecfg =
    //   (xdp::built_in::OutputConfiguration*)malloc(total_size);

    // tilecfg->numTiles = params->numTiles;
    std::cout << "Setting the Metrics!" << std::endl;
    int success = setMetrics(constructs->aieDevInst, constructs->aieDev,
                            config, params, constructs->counterData);
    // uint8_t* out = reinterpret_cast<uint8_t*>(tilecfg);
    // std::memcpy(output, out, total_size);   
    std::cout << "setMetrics Function finished with a value of : " << success << std::endl;
    // free(tilecfg); 
  } else if (iteration == 1) {
    std::cout << "Polling!" << std::endl;


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


