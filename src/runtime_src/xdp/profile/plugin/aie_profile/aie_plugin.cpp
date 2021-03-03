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

#ifdef XRT_ENABLE_AIE
#include "core/edge/common/aie_parser.h"
#endif

extern "C" {
#include <xaiengine.h>
}

namespace xdp {

  AIEProfilingPlugin::AIEProfilingPlugin() 
      : XDPPlugin()
  {
    db->registerPlugin(this);

    // Pre-defined metric sets
    mCoreMetricSets = {"heat_map", "stalls", "execution"};
    mCorestartEvents["heat_map"]    = {AIE_EVENT_ACTIVE, AIE_EVENT_CORE_STALL,
                                    AIE_EVENT_MEMORY_STALL, AIE_EVENT_STREAM_STALL};
    mCorestartEvents["stalls"]      = {AIE_EVENT_MEMORY_STALL, AIE_EVENT_STREAM_STALL,
                                    AIE_EVENT_CASCADE_STALL, AIE_EVENT_LOCK_STALL};
    mCorestartEvents["execution"]   = {AIE_EVENT_CALL_INSTRUCTION, AIE_EVENT_VECTOR_INSTRUCTION,
                                    AIE_EVENT_LOAD_INSTRUCTION, AIE_EVENT_STORE_INSTRUCTION};
    
    mCoreendEvents["heat_map"]      = {AIE_EVENT_DISABLED, AIE_EVENT_COMBO0,
                                    AIE_EVENT_MEMORY_STALL, AIE_EVENT_STREAM_STALL};
    mCoreendEvents["stalls"]        = {AIE_EVENT_MEMORY_STALL, AIE_EVENT_STREAM_STALL,
                                    AIE_EVENT_CASCADE_STALL, AIE_EVENT_LOCK_STALL};
    mCoreendEvents["execution"]     = {AIE_EVENT_CALL_INSTRUCTION, AIE_EVENT_VECTOR_INSTRUCTION,
                                    AIE_EVENT_LOAD_INSTRUCTION, AIE_EVENT_STORE_INSTRUCTION};

    mMemoryMetricSets = {"dma_locks", "conflicts"};
    mMemorystartEvents["dma_locks"] = {AIE_EVENT_GROUP_DMA, AIE_EVENT_GROUP_LOCKS};
    mMemorystartEvents["conflicts"] = {AIE_EVENT_GROUP_CONFLICTS, AIE_EVENT_GROUP_ERRORS};
    
    mMemoryendEvents["dma_locks"]   = {AIE_EVENT_GROUP_DMA, AIE_EVENT_GROUP_LOCKS};
    mMemoryendEvents["conflicts"]   = {AIE_EVENT_GROUP_CONFLICTS, AIE_EVENT_GROUP_ERRORS};

    getPollingInterval();
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

  void AIEProfilingPlugin::getMetrics(void* handle, bool isCore)
  {
    // Get AIE tiles and metric sets
    std::string metricsStr = isCore ?
        xrt_core::config::get_aie_profile_core_metrics() :
        xrt_core::config::get_aie_profile_memory_metrics();
    if (metricsStr.empty())
      return;

    std::vector<std::string> vec;
    metricsStr.erase(metricsStr.find_last_not_of("\n\r")+1);
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

    if ((isCore && (mCoreMetricSets.find(metricSet) == mCoreMetricSets.end()))
        || (!isCore && (mMemoryMetricSets.find(metricSet) == mMemoryMetricSets.end()))) {
      std::string defaultSet = isCore ? "heat_map" : "dma_locks";
      std::stringstream msg;
      msg << "Unable to find " << moduleName << " metric set " << metricSet 
          << ". Using default of " << defaultSet << ".";
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
      metricSet = defaultSet;
    }

    // Get vector of pre-defined metrics for this set
    uint32_t startEvents = isCore ? mCorestartEvents[metricSet] : mMemorystartEvents[metricSet];
    uint32_t endEvents   = isCore ?   mCoreendEvents[metricSet] :   mMemoryendEvents[metricSet];

    if (vec.size() == 1) {
#ifdef XRT_ENABLE_AIE
      std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);
      // TODO: how do we get all tiles regardless of graph name?
      auto tiles = xrt_core::edge::aie::get_tiles(device.get(), "dummy");

      for (auto& tile : tiles) {
        for (int i=0; i < startEvents.size(); ++i) {
          AieCounter counter = std::make_tuple(tile.col, tile.row, moduleName, 
              startEvents.at(i), endEvents.at(i));
          mCounterSet.insert(counter);
        }
      }
#endif
    }
    else if (vec.size() == 2) {
      std::vector<std::string> tileVec;
      boost::split(tileVec, vec.at(0), boost::is_any_of(","));
      uint32_t col = std::stoi(tileVec.at(0));
      uint32_t row = std::stoi(tileVec.at(1));

      for (int i=0; i < startEvents.size(); ++i) {
        AieCounter counter = std::make_tuple(col, row, moduleName, 
            startEvents.at(i), endEvents.at(i));
        mCounterSet.insert(counter);
      }
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
          for (int i=0; i < startEvents.size(); ++i) {
            AieCounter counter = std::make_tuple(col, row, moduleName, 
                startEvents.at(i), endEvents.at(i));
            mCounterSet.insert(counter);
          }
        }
      }
    }
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

      // Iterate over all AIE Counters
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
        XAie_LocType tileLocation = XAie_TileLoc(aie->column, aie->row+1);
        uint32_t counterValue;
        XAie_PerfCounterGet(aieArray->getDevInst(), tileLocation, XAIE_CORE_MOD, aie->counterNumber, &counterValue);
        values.push_back(counterValue);

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

#ifdef XRT_ENABLE_AIE
    // Ensure we only read/configure once per xclbin
    if (!(db->getStaticInfo()).isAIECounterRead(deviceId)) {
      getMetrics(handle, true);
      getMetrics(handle, false);

      // Update the AIE specific portion of the device
      // When new xclbin is loaded, the xclbin specific datastructure is already recreated
      std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);
      auto counters = xrt_core::edge::aie::get_profile_counters(device.get());

      if (counters.empty() && mCounterSet.empty()) {
        std::string msg("AIE Profile Counters were not found for this design. \
            Please specify aie_profile_core_metrics and/or aie_profile_memory_metrics in your xrt.ini.");
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);			  
      }
      else if (!counters.empty()) {
        // Compiler-defined counters
        for (auto& counter : counters) {
          (db->getStaticInfo()).addAIECounter(deviceId, counter.id, counter.column,
              counter.row, counter.counterNumber, counter.startEvent, counter.endEvent,
              counter.resetEvent, counter.clockFreqMhz, counter.module, counter.name);
        }
      }
      else {
        // Runtime-defined counters
        // TODO: filter and configure based on resource availability from resource manager
        // TODO: get AIE clock frequency even when compiler counters are not specified
        double clockFreqMhz = 1000.0;
        uint32_t prevColumn = 0;
        uint32_t prevRow = 0;
        uint32_t counterNumber = 0;
        uint32_t counterId = 0;

        for (auto& counter : mCounterSet) {
          auto column     = std::get<COLUMN>(counter);
          auto row        = std::get<ROW>(counter);
          auto moduleName = std::get<MODULE>(counter);
          auto startEvent = std::get<START_EVENT>(counter);
          auto endEvent   = std::get<END_EVENT>(counter);
          uint32_t resetEvent = 0;
          std::string counterName = "AIE Counter " + std::to_string(counterId);
          // NOTE: this is temporary and assumes that counters from same tile are listed sequentially
          counterNumber   = ((column != prevColumn) || (row != prevRow)) ? 0 : (counterNumber + 1);

          (db->getStaticInfo()).addAIECounter(deviceId, counterId, column, row, counterNumber, 
              startEvent, endEvent, resetEvent, clockFreqMhz, moduleName, counterName);
          counterId++;
        }
      }

      (db->getStaticInfo()).setIsAIECounterRead(deviceId, true);
    }
#endif

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
