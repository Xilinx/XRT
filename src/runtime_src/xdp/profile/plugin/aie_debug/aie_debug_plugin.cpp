/**
 * Copyright (C) 2021 Xilinx, Inc
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

#include "xdp/profile/plugin/aie_debug/aie_debug_plugin.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/writer/aie_debug/aie_debug_writer.h"

#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/time.h"
#include "core/common/config_reader.h"
#include "core/include/experimental/xrt-next.h"
#include "core/edge/user/shim.h"

namespace {
  static void* fetchAieDevInst(void* devHandle)
  {
    auto drv = ZYNQ::shim::handleCheck(devHandle);
    if (!drv)
      return nullptr ;
    auto aieArray = drv->getAieArray();
    if (!aieArray)
      return nullptr ;
    return aieArray->getDevInst();
  }

  static void* allocateAieDevice(void* devHandle)
  {
    XAie_DevInst* aieDevInst = static_cast<XAie_DevInst*>(fetchAieDevInst(devHandle));
    if (!aieDevInst)
      return nullptr;
    return new xaiefal::XAieDev(aieDevInst, false);
  }

  static void deallocateAieDevice(void* aieDevice)
  {
    xaiefal::XAieDev* object = static_cast<xaiefal::XAieDev*>(aieDevice);
    if (object != nullptr)
      delete object;
  }

} // end anonymous namespace

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

  bool AIEDebugPlugin::live = false;

  AIEDebugPlugin::AIEDebugPlugin() 
      : XDPPlugin()
  {
    AIEDebugPlugin::live = true;

    db->registerPlugin(this);
    db->registerInfo(info::aie_status);

    mPollingInterval = xrt_core::config::get_aie_status_interval_us();
  }

  AIEDebugPlugin::~AIEDebugPlugin()
  {
    // Stop the polling thread
    endPoll();

    // Do not call writers here. Once shim is destroyed, writers do not have access to data
    if (VPDatabase::alive())
      db->unregisterPlugin(this);

    AIEDebugPlugin::live = false;
  }

  bool AIEDebugPlugin::alive()
  {
    return AIEDebugPlugin::live;
  }

  // Get tiles to debug
  void AIEDebugPlugin::getTilesForDebug(void* handle)
  {
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

    // Capture all tiles across all graphs
    // Note: in the future, we could support user-defined tile sets
    auto graphs = xrt_core::edge::aie::get_graphs(device.get());
    for (auto& graph : graphs) {
      mGraphCoreTilesMap[graph] = xrt_core::edge::aie::get_event_tiles(device.get(), graph,
          xrt_core::edge::aie::module_type::core);
    }

    // Report tiles (debug only)
    {
      std::stringstream msg;
      msg << "Tiles used for AIE debug:\n";
      for (const auto& kv : mGraphCoreTilesMap) {
        msg << kv.first << " : ";
        for (const auto& tile : kv.second)
          msg << "(" << tile.col << "," << tile.row << "), ";
        msg << "\n";
      }
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    }
  }

  std::string AIEDebugPlugin::getCoreStatusString(uint32_t status) {
    std::string statusStr;

    if (status & 0x1)
      statusStr += "Enable,";
    if (status & 0x2)
      statusStr += "Reset,";
    if (status & 0x4)
      statusStr += "Memory_Stall_S,";
    if (status & 0x8)
      statusStr += "Memory_Stall_W,";
    if (status & 0x10)
      statusStr += "Memory_Stall_N,";
    if (status & 0x20)
      statusStr += "Memory_Stall_E,";
    if (status & 0x40)
      statusStr += "Lock_Stall_S,";
    if (status & 0x80)
      statusStr += "Lock_Stall_W,";
    if (status & 0x100)
      statusStr += "Lock_Stall_N,";
    if (status & 0x200)
      statusStr += "Lock_Stall_E,";
    if (status & 0x400)
      statusStr += "Stream_Stall_SS0,";
    if (status & 0x800)
      statusStr += "Stream_Stall_SS1,";
    if (status & 0x1000)
      statusStr += "Stream_Stall_MS0,";
    if (status & 0x2000)
      statusStr += "Stream_Stall_MS1,";
    if (status & 0x4000)
      statusStr += "Cascade_Stall_SCD,";
    if (status & 0x8000)
      statusStr += "Cascade_Stall_MCD,";
    if (status & 0x10000)
      statusStr += "Debug_Halt,";
    if (status & 0x20000)
      statusStr += "ECC_Error_Stall,";
    if (status & 0x40000)
      statusStr += "ECC_Scrubbing_Stall,";

    // remove trailing comma
    if (!statusStr.empty())
      statusStr.pop_back();

    return statusStr;
  }

  void AIEDebugPlugin::pollDeadlock(uint64_t index, void* handle)
  {
    auto it = mThreadCtrlMap.find(handle);
    if (it == mThreadCtrlMap.end())
      return;

    // AIE core register offsets
    constexpr uint64_t AIE_OFFSET_CORE_STATUS = 0x32004;

    // This mask check for following states
    // ECC_Scrubbing_Stall
    // ECC_Error_Stall
    // Debug_Halt
    // Cascade_Stall_MCD
    // Cascade_Stall_SCD
    // Stream_Stall_MS1
    // Stream_Stall_MS0
    // Stream_Stall_SS1
    // Stream_Stall_SS0
    // Lock_Stall_E
    // Lock_Stall_N
    // Lock_Stall_W
    // Lock_Stall_S
    // Memory_Stall_E
    // Memory_Stall_N
    // Memory_Stall_W
    // Memory_Stall_S
    constexpr uint32_t CORE_STALL_MASK = 0xFFFC;
    // This mask check for following states
    // Reset
    // Done
    constexpr uint32_t CORE_INACTIVE_MASK = 0x100002;
    // Count of samples before we say it's a hang
    constexpr unsigned int CORE_HANG_COUNT_THRESHOLD = 100;
    constexpr unsigned int GRAPH_HANG_COUNT_THRESHOLD = 50;
    // Reset values
    constexpr uint32_t CORE_RESET_STATUS  = 0x2;
    constexpr uint32_t CORE_ENABLE_MASK  = 0x1;
    // Graph -> total stuck core cycles
    std::map<std::string, uint64_t> graphStallTotalMap;
    // Core -> total stall cycles
    std::map<tile_type, uint32_t> coreStuckCountMap;
    // Core -> last checked status
    std::map<tile_type, uint32_t> coreStatusMap;
    // Pre-populate core status and PC maps
    for (const auto& kv : mGraphCoreTilesMap) {
      for (const auto& tile : kv.second) {
        coreStuckCountMap[tile] = 0;
        coreStatusMap[tile] = CORE_RESET_STATUS;
      }
    }

    auto& shouldContinue = it->second;
    while (shouldContinue) {
      // Wait until xclbin has been loaded and device has been updated in database
      if (!(db->getStaticInfo().isDeviceReady(index)))
        continue;
      XAie_DevInst* aieDevInst =
        static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle)) ;
      if (!aieDevInst)
        continue;

      bool foundStuckCores = false;
      tile_type stuckTile;
      uint32_t stuckCoreStatus = 0;

      // Iterate over all tiles
      for (const auto& kv : mGraphCoreTilesMap) {
        auto& graphName = kv.first;
        auto& graphTilesVec = kv.second;
        auto& graphStallCounter = graphStallTotalMap[graphName];
        for (const auto& tile : graphTilesVec) {
          // Read core status and PC value
          bool coreUnstalled = false;
          uint32_t coreStatus = 0;
          auto tileOffset = _XAie_GetTileAddr(aieDevInst, tile.row + 1, tile.col);
          XAie_Read32(aieDevInst, tileOffset + AIE_OFFSET_CORE_STATUS, &coreStatus);

          auto& coreStallCounter = coreStuckCountMap[tile];

          // Condition : Core is in reset/done state or not enabled
          if (coreStatus & CORE_INACTIVE_MASK || !(coreStatus & CORE_ENABLE_MASK)) {
            coreUnstalled = (coreStallCounter >= GRAPH_HANG_COUNT_THRESHOLD);
            coreStallCounter = 0;
          }
          // Condition : If core is enabled + stalled and has same kind of stall as previous check
          else if ((coreStatus & CORE_STALL_MASK) && (coreStatus == coreStatusMap[tile]) ) {
            coreStallCounter++;
          }
          // Core is running normally or has changed state
          else {
            coreUnstalled = (coreStallCounter >= GRAPH_HANG_COUNT_THRESHOLD);
            coreStallCounter = 0;
          }

          // Is this core contributing to entire graph hang?
          if (coreUnstalled && graphStallCounter) {
            graphStallCounter--;
          } else if (coreStallCounter == GRAPH_HANG_COUNT_THRESHOLD) {
            graphStallCounter++;
          }

          // Is this core stuck for long time?
          if (coreStallCounter == CORE_HANG_COUNT_THRESHOLD) {
            foundStuckCores = true;
            stuckTile = tile;
            stuckCoreStatus = coreStatus;
          }

          coreStatusMap[tile] = coreStatus;
        } // For tiles in graph

        std::stringstream warningMessage;
        if (graphStallCounter == graphTilesVec.size()) {
          // We have a stuck graph
          warningMessage
          << "Potential deadlock/hang found in AI Engines. Graph : " << graphName;
          xrt_core::message::send(severity_level::warning, "XRT", warningMessage.str());
          // Send next warning if all tiles come out of hang & reach threshold again
          graphStallCounter = 0;
        } else if (foundStuckCores) {
          // We have a stuck core within this graph
          warningMessage
          << "Potential stuck cores found in AI Engines. Graph : " << graphName << " "
          << "Tile : " << "(" << stuckTile.col << "," << stuckTile.row + 1 << ") "
          << "Status 0x" << std::hex << stuckCoreStatus << std::dec
          << " : " << getCoreStatusString(stuckCoreStatus);

          xrt_core::message::send(severity_level::warning, "XRT", warningMessage.str());
          foundStuckCores = false;
        }

        // Print status for debug
        if (xrt_core::config::get_verbosity() >= static_cast<unsigned int>(severity_level::debug)) {
          std::stringstream msg;
          for (const auto& tile : graphTilesVec) {
            if (coreStuckCountMap[tile]) {
              msg
                << "T(" << tile.col <<"," << tile.row + 1 << "):" << "<" << coreStuckCountMap[tile]
                << ":0x" << std::hex << coreStatusMap[tile] << std::dec << "> ";
            }
          }
          if (!msg.str().empty()) {
            msg << std::endl << "Graph " << graphName << " #Cur : " << graphStallCounter << " #Thr : " << graphTilesVec.size();
            xrt_core::message::send(severity_level::debug, "XRT", msg.str());
          }
        }
      } // For graphs

      std::this_thread::sleep_for(std::chrono::microseconds(mPollingInterval));
    }
  }

  void AIEDebugPlugin::writeDebug(uint64_t index, void* handle, VPWriter* aieWriter, VPWriter* aieshimWriter)
  {
    auto it = mThreadCtrlMap.find(handle);
    if (it == mThreadCtrlMap.end())
      return;
    auto& shouldContinue = it->second;

    while (shouldContinue) {
      if (!(db->getStaticInfo().isDeviceReady(index)))
        continue;

      aieWriter->write(false);
      aieshimWriter->write(false);
      std::this_thread::sleep_for(std::chrono::microseconds(mPollingInterval));
    }
  }

  void AIEDebugPlugin::updateAIEDevice(void* handle)
  {
    // Don't update if no debug/status is requested
    if (!xrt_core::config::get_aie_status())
      return;

    const unsigned int PATH_LENGTH = 512;
    char pathBuf[PATH_LENGTH];
    memset(pathBuf, 0, PATH_LENGTH);
    xclGetDebugIPlayoutPath(handle, pathBuf, PATH_LENGTH);

    std::string sysfspath(pathBuf);
    uint64_t deviceID = db->addDevice(sysfspath); // Get the unique device Id

    if (!(db->getStaticInfo()).isDeviceReady(deviceID)) {
      // Update the static database with information from xclbin
      (db->getStaticInfo()).updateDevice(deviceID, handle);
      {
        struct xclDeviceInfo2 info;
        if(xclGetDeviceInfo2(handle, &info) == 0) {
          (db->getStaticInfo()).setDeviceName(deviceID, std::string(info.mName));
        }
      }
    }

    // Update list of tiles to debug
    getTilesForDebug(handle);

    // Open the writer for this device
    struct xclDeviceInfo2 info;
    xclGetDeviceInfo2(handle, &info);
    std::string devicename { info.mName };

    // Create and register aie status writer
    std::string filename = "aie_status_" + devicename + ".json";
    VPWriter* aieWriter = new AIEDebugWriter(filename.c_str(), devicename.c_str(), deviceID);
    writers.push_back(aieWriter);
    db->getStaticInfo().addOpenedFile(aieWriter->getcurrentFileName(), "AIE_RUNTIME_STATUS");

    // Create and register aie shim status writer
    filename = "aieshim_status_" + devicename + ".json";
    VPWriter* aieshimWriter = new AIEShimDebugWriter(filename.c_str(), devicename.c_str(), deviceID);
    writers.push_back(aieshimWriter);
    db->getStaticInfo().addOpenedFile(aieshimWriter->getcurrentFileName(), "AIE_RUNTIME_STATUS");

    // Start the AIE debug thread
    mThreadCtrlMap[handle] = true;
    // NOTE: This does not start the threads immediately.
    mDeadlockThreadMap[handle] = std::thread { [=] { pollDeadlock(deviceID, handle); } };
    mDebugThreadMap[handle] = std::thread { [=] { writeDebug(deviceID, handle, aieWriter, aieshimWriter); } };
  }

  void AIEDebugPlugin::endPollforDevice(void* handle)
  {
    // Ask threads to stop
    mThreadCtrlMap[handle] = false;

    {
      auto it = mDeadlockThreadMap.find(handle);
      if (it != mDeadlockThreadMap.end()) {
        it->second.join();
        mDeadlockThreadMap.erase(it);
      }
    }

    {
      auto it = mDebugThreadMap.find(handle);
      if (it != mDebugThreadMap.end()) {
        it->second.join();
        mDebugThreadMap.erase(it);
      }
    }

    mThreadCtrlMap.erase(handle);
  }

  void AIEDebugPlugin::endPoll()
  {
    // Ask all threads to end
    for (auto& p : mThreadCtrlMap)
      p.second = false;

    for (auto& t : mDeadlockThreadMap)
      t.second.join();

    for (auto& t : mDebugThreadMap)
      t.second.join();

    mThreadCtrlMap.clear();
    mDeadlockThreadMap.clear();
    mDebugThreadMap.clear();
  }

} // end namespace xdp
