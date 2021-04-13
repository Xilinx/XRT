/**
 * Copyright (C) 2020 Xilinx, Inc
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

#include "xdp/profile/plugin/aie_profile/aie_plugin.h"
#include "xdp/profile/writer/aie_profile/aie_writer.h"

#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/time.h"
#include "core/common/config_reader.h"
#include "core/include/experimental/xrt-next.h"
#include "core/edge/user/shim.h"
#include "core/edge/common/aie_parser.h"
#include "xdp/profile/database/database.h"

#include <boost/algorithm/string.hpp>

#include "core/edge/common/aie_parser.h"
#include "core/edge/user/aie/AIEResources.h"

#define NUM_CORE_COUNTERS   4
#define NUM_MEMORY_COUNTERS 2
#define BASE_MEMORY_COUNTER 128

namespace xdp {

  AIEProfilingPlugin::AIEProfilingPlugin() 
      : XDPPlugin()
  {
    db->registerPlugin(this);
    getPollingInterval();

    //
    // Pre-defined metric sets
    //
    // **** Core Module Counters ****
    mCoreMetricSets = {"heat_map", "stalls", "execution"};
    mCoreStartEvents = {
      {"heat_map",  {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_GROUP_CORE_STALL_CORE,
                     XAIE_EVENT_MEMORY_STALL_CORE,         XAIE_EVENT_STREAM_STALL_CORE}},
      {"stalls",    {XAIE_EVENT_MEMORY_STALL_CORE,         XAIE_EVENT_STREAM_STALL_CORE,
                     XAIE_EVENT_CASCADE_STALL_CORE,        XAIE_EVENT_LOCK_STALL_CORE}},
      {"execution", {XAIE_EVENT_INSTR_CALL_CORE,           XAIE_EVENT_INSTR_VECTOR_CORE,
                     XAIE_EVENT_INSTR_LOAD_CORE,           XAIE_EVENT_INSTR_STORE_CORE}}
    };
    mCoreEndEvents = {
      {"heat_map",  {XAIE_EVENT_DISABLED_CORE,             XAIE_EVENT_GROUP_CORE_STALL_CORE,
                     XAIE_EVENT_MEMORY_STALL_CORE,         XAIE_EVENT_STREAM_STALL_CORE}},
      {"stalls",    {XAIE_EVENT_MEMORY_STALL_CORE,         XAIE_EVENT_STREAM_STALL_CORE,
                     XAIE_EVENT_CASCADE_STALL_CORE,        XAIE_EVENT_LOCK_STALL_CORE}},
      {"execution", {XAIE_EVENT_INSTR_CALL_CORE,           XAIE_EVENT_INSTR_VECTOR_CORE,
                     XAIE_EVENT_INSTR_LOAD_CORE,           XAIE_EVENT_INSTR_STORE_CORE}}
    };

    // **** Memory Module Counters ****
    mMemoryMetricSets = {"dma_locks", "conflicts"};
    mMemoryStartEvents = {
      {"dma_locks", {XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM,    XAIE_EVENT_GROUP_LOCK_MEM}},
      {"conflicts", {XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM, XAIE_EVENT_GROUP_ERRORS_MEM}}
    };
    mMemoryEndEvents = {
      {"dma_locks", {XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM,    XAIE_EVENT_GROUP_LOCK_MEM}}, 
      {"conflicts", {XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM, XAIE_EVENT_GROUP_ERRORS_MEM}}
    };
  }

  AIEProfilingPlugin::~AIEProfilingPlugin()
  {
    // Stop the polling thread
    endPoll();

    if (VPDatabase::alive()) {
      for (auto w : writers) {
        w->write(false);
      }

      db->unregisterPlugin(this);
    }
  }

  void AIEProfilingPlugin::getPollingInterval()
  {
    // Get polling interval (in usec; minimum is 100)
    mPollingInterval = xrt_core::config::get_aie_profile_interval_us();
    if (mPollingInterval < 100) {
      mPollingInterval = 100;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", 
          "Minimum supported AIE profile interval is 100 usec.");
    }
  }

  bool AIEProfilingPlugin::setMetrics(uint64_t deviceId, void* handle)
  {
    bool runtimeCounters = false;

    auto drv = ZYNQ::shim::handleCheck(handle);
    if (!drv)
      return runtimeCounters;
    auto aieArray = drv->getAieArray();
    if (!aieArray)
      return runtimeCounters;

    auto Aie = aieArray->getDevInst();
    AieRscDevice = std::make_shared<xaiefal::XAieDev>(Aie, false);
	  std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

    // Get AIE clock frequency
    auto clockFreqMhz = xrt_core::edge::aie::get_clock_freq_mhz(device.get());

    // Configure both core and memory module counters
    for (int module=0; module < 2; ++module) {
      bool isCore = (module == 0);

      std::string metricsStr = isCore ?
          xrt_core::config::get_aie_profile_core_metrics() :
          xrt_core::config::get_aie_profile_memory_metrics();
      if (metricsStr.empty())
        continue;

      std::vector<std::string> vec;
      boost::split(vec, metricsStr, boost::is_any_of(":"));

      for (int i=0; i < vec.size(); ++i) {
        boost::replace_all(vec.at(i), "{", "");
        boost::replace_all(vec.at(i), "}", "");
      }
      
      // Determine specification type based on vector size:
      //   * Size = 1: All tiles
      //     * aie_profile_core_metrics = <heat_map|stalls|execution>
      //     * aie_profile_memory_metrics = <dma_locks|conflicts>
      //   * Size = 2: Single tile
      //     * aie_profile_core_metrics = {<column>,<row>}:<heat_map|stalls|execution>
      //     * aie_profile_memory_metrics = {<column>,<row>}:<dma_locks|conflicts>
      //   * Size = 3: Range of tiles
      //     * aie_profile_core_metrics = {<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<heat_map|stalls|execution>
      //     * aie_profile_memory_metrics = {<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<dma_locks|conflicts>
      std::string metricSet  = vec.at( vec.size()-1 );
      std::string moduleName = isCore ? "core" : "memory";

      // Ensure requested metric set is supported (if not, use default)
      if ((isCore && (mCoreMetricSets.find(metricSet) == mCoreMetricSets.end()))
          || (!isCore && (mMemoryMetricSets.find(metricSet) == mMemoryMetricSets.end()))) {
        std::string defaultSet = isCore ? "heat_map" : "dma_locks";
        std::stringstream msg;
        msg << "Unable to find " << moduleName << " metric set " << metricSet 
            << ". Using default of " << defaultSet << ".";
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
        metricSet = defaultSet;
      }

      // Compile list of tiles based on how its specified in setting
      std::vector<xrt_core::edge::aie::tile_type> tiles;

      if (vec.size() == 1) {
        // Capture all tiles across all graphs
        auto graphs = xrt_core::edge::aie::get_graphs(device.get());
        
        for (auto& graph : graphs) {
          auto currTiles = xrt_core::edge::aie::get_tiles(device.get(), graph);
          std::copy(currTiles.begin(), currTiles.end(), back_inserter(tiles));
        }
      }
      else if (vec.size() == 2) {
        std::vector<std::string> tileVec;
        boost::split(tileVec, vec.at(0), boost::is_any_of(","));

        xrt_core::edge::aie::tile_type tile;
        tile.col = std::stoi(tileVec.at(0));
        tile.row = std::stoi(tileVec.at(1));
        tiles.push_back(tile);
      }
      else if (vec.size() == 3) {
        std::vector<std::string> minTileVec;
        boost::split(minTileVec, vec.at(0), boost::is_any_of(","));
        uint32_t minCol = std::stoi(minTileVec.at(0));
        uint32_t minRow = std::stoi(minTileVec.at(1));
        
        std::vector<std::string> maxTileVec;
        boost::split(maxTileVec, vec.at(1), boost::is_any_of(","));
        uint32_t maxCol = std::stoi(maxTileVec.at(0));
        uint32_t maxRow = std::stoi(maxTileVec.at(1));

        for (uint32_t col = minCol; col <= maxCol; ++col) {
          for (uint32_t row = minRow; row <= maxRow; ++row) {
            xrt_core::edge::aie::tile_type tile;
            tile.col = col;
            tile.row = row;
            tiles.push_back(tile);
          }
        }
      }

      // Report tiles (debug only)
      {
        std::stringstream msg;
        msg << "Tiles used for AIE " << moduleName << " profile counters: ";
        for (auto& tile : tiles) {
          msg << "(" << tile.col << "," << tile.row << "), ";
        }
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
      }

      // Get vector of pre-defined metrics for this set
      auto startEvents = isCore ? mCoreStartEvents[metricSet] : mMemoryStartEvents[metricSet];
      auto endEvents   = isCore ?   mCoreEndEvents[metricSet] :   mMemoryEndEvents[metricSet];

      uint8_t resetEvent = 0;
      int counterId = 0;
      auto moduleType = isCore ? XAIE_CORE_MOD : XAIE_MEM_MOD;
      
      int NUM_COUNTERS = isCore ? NUM_CORE_COUNTERS : NUM_MEMORY_COUNTERS;
      int numTileCounters[NUM_COUNTERS+1] = {0};

      // Iterate over tiles and metrics to configure all desired counters
      for (auto& tile : tiles) {
        int numCounters = 0;
        auto col = tile.col;
        auto row = tile.row;
        // NOTE: resource manager requires absolute row number
        auto& core   = AieRscDevice->tile(col, row + 1).core();
        auto& memory = AieRscDevice->tile(col, row + 1).mem();

        for (int i=0; i < startEvents.size(); ++i) {
          // Request counter from resource manager
          // NOTE: Resource manager does not currently support configuring group stalls,
          //       so for that case, we need to use the extended class XAieStallCycles.
          if (startEvents.at(i) == XAIE_EVENT_GROUP_CORE_STALL_CORE) {
            auto perfCounter = core.stallCycles();
            auto ret = perfCounter->reserve();
            if (ret != XAIE_OK) break;
            ret = perfCounter->start();
            if (ret != XAIE_OK) break;
            mPerfCounters.push_back(perfCounter);
          }
          else {
            auto perfCounter = isCore ? core.perfCounter() : memory.perfCounter();
            auto ret1 = perfCounter->initialize(moduleType, startEvents.at(i),
                                                moduleType, endEvents.at(i));
            if (ret1 != XAIE_OK) break;
            auto ret = perfCounter->reserve();
            if (ret != XAIE_OK) break;
            ret = perfCounter->start();
            if (ret != XAIE_OK) break;
            mPerfCounters.push_back(perfCounter);
          }
	        
          // Convert enums to physical event IDs for reporting purposes
          int counterNum = i;
          auto mod = isCore ? XAIE_CORE_MOD : XAIE_MEM_MOD;
          auto loc = XAie_TileLoc(col, row + 1);
          uint8_t phyStartEvent = 0;
          uint8_t phyEndEvent = 0;
          XAie_EventLogicalToPhysicalConv(Aie, loc, mod, startEvents.at(i), &phyStartEvent);
          XAie_EventLogicalToPhysicalConv(Aie, loc, mod, endEvents.at(i), &phyEndEvent);
          if (!isCore) {
            phyStartEvent += BASE_MEMORY_COUNTER;
            phyEndEvent += BASE_MEMORY_COUNTER;
          }

          // Store counter info in database
          std::string counterName = "AIE Counter " + std::to_string(counterId);
          (db->getStaticInfo()).addAIECounter(deviceId, counterId, col, row, counterNum, 
              phyStartEvent, phyEndEvent, resetEvent, clockFreqMhz, moduleName, counterName);
          counterId++;
          numCounters++;
        }

        std::stringstream msg;
        msg << "Reserved " << numCounters << " counters for profiling AIE tile (" << col << "," << row << ").";
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
        numTileCounters[numCounters]++;
      }

      // Report counters reserved per tile
      {
        std::stringstream msg;
        msg << "AIE profile counters reserved in " << moduleName << " modules - ";
        for (int n=0; n <= NUM_COUNTERS; ++n) {
          if (numTileCounters[n] == 0) continue;
          msg << n << ": " << numTileCounters[n] << " tiles";
          if (n != NUM_COUNTERS) msg << ", ";

          (db->getStaticInfo()).addAIECounterResources(deviceId, n, numTileCounters[n]);
        }
        xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg.str());
      }

      runtimeCounters = true;
    } // for module

    return runtimeCounters;
  }
  
  void AIEProfilingPlugin::pollAIECounters(uint32_t index, void* handle)
  {
    auto drv = ZYNQ::shim::handleCheck(handle);
    if (!drv)
      return;
    auto it = mThreadCtrlMap.find(handle);
    if (it == mThreadCtrlMap.end())
      return;

    auto& should_continue = it->second;
    while (should_continue) {
      // Wait until xclbin has been loaded and device has been updated in database
      if (!(db->getStaticInfo().isDeviceReady(index)))
        continue;
      auto aieArray = drv->getAieArray();
      if (!aieArray)
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
          // TODO: How do we safely read these?
          XAie_LocType tileLocation = XAie_TileLoc(aie->column, aie->row + 1);
          XAie_PerfCounterGet(aieArray->getDevInst(), tileLocation, XAIE_CORE_MOD, aie->counterNumber, &counterValue);
        }
        else {
          // Runtime-defined counters
          auto perfCounter = mPerfCounters.at(c);
          perfCounter->readResult(counterValue);
        }
        values.push_back(counterValue);

        // Read tile timer (once per tile to minimize overhead)
        if ((aie->column != prevColumn) && (aie->row != prevRow)) {
          prevColumn = aie->column;
          prevRow = aie->row;
          XAie_LocType tileLocation = XAie_TileLoc(aie->column, aie->row + 1);
          XAie_ReadTimer(aieArray->getDevInst(), tileLocation, XAIE_CORE_MOD, &timerValue);
        }
        values.push_back(timerValue);

        // Get timestamp in milliseconds
        double timestamp = xrt_core::time_ns() / 1.0e6;
        db->getDynamicInfo().addAIESample(index, timestamp, values);
      }

      std::this_thread::sleep_for(std::chrono::microseconds(mPollingInterval));     
    }
  }

  void AIEProfilingPlugin::updateAIEDevice(void* handle)
  {
    // Don't update if no profiling is requested
    if (!xrt_core::config::get_aie_profile())
      return;

    char pathBuf[512];
    memset(pathBuf, 0, 512);
    xclGetDebugIPlayoutPath(handle, pathBuf, 512);

    std::string sysfspath(pathBuf);
    uint64_t deviceId = db->addDevice(sysfspath); // Get the unique device Id

    if (!(db->getStaticInfo()).isDeviceReady(deviceId)) {
      // Update the static database with information from xclbin
      (db->getStaticInfo()).updateDevice(deviceId, handle);
      {
        struct xclDeviceInfo2 info;
        if(xclGetDeviceInfo2(handle, &info) == 0) {
          (db->getStaticInfo()).setDeviceName(deviceId, std::string(info.mName));
        }
      }
    }

    // Ensure we only read/configure once per xclbin
    if (!(db->getStaticInfo()).isAIECounterRead(deviceId)) {
      // Update the AIE specific portion of the device
      // When new xclbin is loaded, the xclbin specific datastructure is already recreated

      // 1. Runtime-defined counters
      // NOTE: these take precedence
      bool runtimeCounters = setMetrics(deviceId, handle);

      // 2. Compiler-defined counters
      if (!runtimeCounters) {
        std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);
        auto counters = xrt_core::edge::aie::get_profile_counters(device.get());

        if (counters.empty()) {
          std::string msg = "AIE Profile Counters were not found for this design. "
                            "Please specify aie_profile_core_metrics and/or aie_profile_memory_metrics in your xrt.ini.";
          xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
        }
        else {
          for (auto& counter : counters) {
            (db->getStaticInfo()).addAIECounter(deviceId, counter.id, counter.column,
                counter.row + 1, counter.counterNumber, counter.startEvent, counter.endEvent,
                counter.resetEvent, counter.clockFreqMhz, counter.module, counter.name);
          }
        }
      }

      (db->getStaticInfo()).setIsAIECounterRead(deviceId, true);
    }

    // Open the writer for this device
    struct xclDeviceInfo2 info;
    xclGetDeviceInfo2(handle, &info);
    std::string deviceName = std::string(info.mName);
    // Create and register writer and file
    std::string outputFile = "aie_profile_" + deviceName + ".csv";
    writers.push_back(new AIEProfilingWriter(outputFile.c_str(),
        deviceName.c_str(), mIndex));
    db->getStaticInfo().addOpenedFile(outputFile.c_str(), "AIE_PROFILE");

    // Start the AIE profiling thread
    mThreadCtrlMap[handle] = true;
    auto device_thread = std::thread(&AIEProfilingPlugin::pollAIECounters, this, mIndex, handle);
    mThreadMap[handle] = std::move(device_thread);

    ++mIndex;
  }

  void AIEProfilingPlugin::endPollforDevice(void* handle)
  {
    // Ask thread to stop
    mThreadCtrlMap[handle] = false;

    auto it = mThreadMap.find(handle);
    if (it != mThreadMap.end()) {
      it->second.join();
      mThreadMap.erase(it);
      mThreadCtrlMap.erase(handle);
    }
  }

  void AIEProfilingPlugin::endPoll()
  {
    // Ask all threads to end
    for (auto& p : mThreadCtrlMap)
      p.second = false;

    for (auto& t : mThreadMap)
      t.second.join();

    mThreadCtrlMap.clear();
    mThreadMap.clear();
  }

} // end namespace xdp
