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

#define AIE_OFFSET_CORE_STATUS     0x32004
#define AIE_OFFSET_PROGRAM_COUNTER 0x30280

#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/plugin/aie_debug/aie_debug_plugin.h"

#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/time.h"
#include "core/common/config_reader.h"
#include "core/include/experimental/xrt-next.h"
#include "core/edge/user/shim.h"
#include "xdp/profile/database/database.h"

#include <boost/algorithm/string.hpp>

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
    XAie_DevInst* aieDevInst =
      static_cast<XAie_DevInst*>(fetchAieDevInst(devHandle)) ;
    if (!aieDevInst)
      return nullptr ;
    return new xaiefal::XAieDev(aieDevInst, false) ;
  }

  static void deallocateAieDevice(void* aieDevice)
  {
    xaiefal::XAieDev* object = static_cast<xaiefal::XAieDev*>(aieDevice) ;
    if (object != nullptr)
      delete object ;
  }

} // end anonymous namespace

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

  AIEDebugPlugin::AIEDebugPlugin() 
      : XDPPlugin()
  {
    db->registerPlugin(this);
    db->registerInfo(info::aie_debug);
    getPollingInterval();
  }

  AIEDebugPlugin::~AIEDebugPlugin()
  {
    // Stop the polling thread
    endPoll();

    // Write out final version of file and unregister plugin
    if (VPDatabase::alive()) {
      for (auto w : writers) {
        w->write(true);
      }

      db->unregisterPlugin(this);
    }
  }

  // Get polling interval (in usec; no minimum)
  void AIEDebugPlugin::getPollingInterval()
  {
    mPollingInterval = xrt_core::config::get_aie_debug_interval_us();
  }

  // Get tiles to debug
  void AIEDebugPlugin::getTilesForDebug(void* handle)
  {
    mTiles.clear();
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

    // Capture all tiles across all graphs
    // Note: in the future, we could support user-defined tile sets
    auto graphs = xrt_core::edge::aie::get_graphs(device.get());
    for (auto& graph : graphs) {
      auto coreTiles = xrt_core::edge::aie::get_event_tiles(device.get(), graph,
          xrt_core::edge::aie::module_type::core);
        
      std::sort(coreTiles.begin(), coreTiles.end(),
        [](tile_type t1, tile_type t2) {
            if (t1.row == t2.row)
              return t1.col > t2.col;
            else
              return t1.row > t2.row;
        }
      );

      std::unique_copy(coreTiles.begin(), coreTiles.end(), back_inserter(mTiles),
        [](tile_type t1, tile_type t2) {
            return ((t1.col == t2.col) && (t1.row == t2.row));
        }
      );
    }

    // Report tiles (debug only)
    {
      std::stringstream msg;
      msg << "Tiles used for AIE debug: ";
      for (auto& tile : mTiles) {
        msg << "(" << tile.col << "," << tile.row << "), ";
      }
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    }
  }

  void AIEDebugPlugin::pollAIERegisters(uint32_t index, void* handle)
  {
    auto it = mThreadCtrlMap.find(handle);
    if (it == mThreadCtrlMap.end())
      return;

    // Warning message if graph stall found
    std::string warningMessage = "All active AI Engines had same state across multiple samples. "
        "Your graph could be stalled.";

    // Pre-populate core status and PC maps
    std::map<tile_type, uint32_t> coreStatusMap;
    std::map<tile_type, uint32_t> programCounterMap;
    for (auto& tile : mTiles) {
      coreStatusMap[tile] = 0xFFFFFFFF;
      programCounterMap[tile] = 0xFFFFFFFF;
    }

    auto& should_continue = it->second;
    while (should_continue) {
      // Wait until xclbin has been loaded and device has been updated in database
      if (!(db->getStaticInfo().isDeviceReady(index)))
        continue;
      XAie_DevInst* aieDevInst =
        static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle)) ;
      if (!aieDevInst)
        continue;

      found foundStuckCores = true;

      // Iterate over all tiles
      for (auto& tile : mTiles) {
        // Read core status and PC value
        uint32_t coreStatus = 0;
        uint32_t programCounter = 0;
        auto tileOffset = _XAie_GetTileAddr(aieDevInst, tile.row, tile.col);
        XAie_Read32(aieDevInst, tileOffset + AIE_OFFSET_CORE_STATUS, &coreStatus);
        XAie_Read32(aieDevInst, tileOffset + AIE_OFFSET_PROGRAM_COUNTER, &programCounter);
        
        // Ignore if core is not enabled
        if ((coreStatus & 0x1) == 0)
          continue;

        if ((coreStatus != coreStatusMap.at(tile)) 
            || (programCounter != programCounterMap.at(tile)))
          foundStuckCores = false;

        coreStatusMap[tile] = coreStatus;
        programCounterMap[tile] = programCounter;
      }

      // Print out warning message if potential deadlock/graph stall found
      if (foundStuckCores)
        xrt_core::message::send(severity_level::warning, "XRT", warningMessage);

      // Always write out latest debug/status file
      for (auto w : writers) {
        w->write(true);
      }      

      std::this_thread::sleep_for(std::chrono::microseconds(mPollingInterval));     
    }
  }

  void AIEDebugPlugin::updateAIEDevice(void* handle)
  {
    // Don't update if no debug is requested
    if (!xrt_core::config::get_aie_debug())
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

    // Update list of tiles to debug
    getTilesForDebug(handle);

    // Open the writer for this device
    struct xclDeviceInfo2 info;
    xclGetDeviceInfo2(handle, &info);
    std::string deviceName = std::string(info.mName);
    // Create and register writer and file
    std::string outputFile = "aie_debug_" + deviceName + ".json";

    VPWriter* writer = new AIEDebugWriter(outputFile.c_str(),
                                          deviceName.c_str(), mIndex);
    writers.push_back(writer);
    db->getStaticInfo().addOpenedFile(writer->getcurrentFileName(), "AIE_DEBUG");

    // Start the AIE debug thread
    mThreadCtrlMap[handle] = true;
    auto device_thread = std::thread(&AIEDebugPlugin::pollAIERegisters, this, mIndex, handle);
    mThreadMap[handle] = std::move(device_thread);

    ++mIndex;
  }

  void AIEDebugPlugin::endPollforDevice(void* handle)
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

  void AIEDebugPlugin::endPoll()
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
