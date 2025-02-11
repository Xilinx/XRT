/**
 * Copyright (C) 2021 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <set>

#include "xdp/profile/plugin/aie_status/aie_status_plugin.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/device/utility.h"
#include "xdp/profile/device/xdp_base_device.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/writer/aie_status/aie_status_writer.h"

#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/time.h"
#include "core/common/config_reader.h"
#include "core/include/experimental/xrt-next.h"

#ifdef XDP_VE2_BUILD
#include "shim/shim.h"
#else
#include "core/edge/user/shim.h"
#endif

namespace {
  static void* fetchAieDevInst(void* devHandle)
  {
#ifdef XDP_VE2_BUILD
    auto drv = aiarm::shim::handleCheck(devHandle);
    if (!drv)
      return nullptr;
    auto aieArray = drv->get_aie_array();
#else
    auto drv = ZYNQ::shim::handleCheck(devHandle);
    if (!drv)
      return nullptr;
    auto aieArray = drv->getAieArray();
#endif

    if (!aieArray)
      return nullptr;
    return aieArray->get_dev();
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

  bool AIEStatusPlugin::live = false;

  AIEStatusPlugin::AIEStatusPlugin() 
      : XDPPlugin()
  {
    AIEStatusPlugin::live = true;

    db->registerPlugin(this);
    db->registerInfo(info::aie_status);
    db->getStaticInfo().setAieApplication();

    mPollingInterval = xrt_core::config::get_aie_status_interval_us();
  }

  AIEStatusPlugin::~AIEStatusPlugin()
  {
    // Stop the polling thread
    endPoll();

    // Do not call writers here. Once shim is destroyed, writers do not have access to data
    if (VPDatabase::alive())
      db->unregisterPlugin(this);

    AIEStatusPlugin::live = false;
  }

  bool AIEStatusPlugin::alive()
  {
    return AIEStatusPlugin::live;
  }

  /****************************************************************************
   * Gather list of tiles to check status
   ***************************************************************************/
  void AIEStatusPlugin::getTilesForStatus()
  {
    // Capture all tiles across all graphs
    // Note: in the future, we could support user-defined tile sets
    auto graphs = metadataReader->getValidGraphs();
    for (auto& graph : graphs) {
      mGraphCoreTilesMap[graph] = metadataReader->getEventTiles(graph, module_type::core);
    }

   // NOTE: AIE Status is not released product on client. Whenever client support is needed,
   // required dynamic column start shift should come from XRT and not compiler metadata
   uint8_t startColShift = metadataReader->getPartitionOverlayStartCols().front();
   aie::displayColShiftInfo(startColShift);

   if (startColShift > 0) {
    for(auto& [graph, tileVec] : mGraphCoreTilesMap) {
      for(auto& tile : tileVec)
        tile.col += startColShift;
    }
   }

    // Report tiles (debug only)
    if (aie::isDebugVerbosity()) {
      auto offset = metadataReader->getAIETileRowOffset();

      std::stringstream msg;
      msg << "Tiles used for AIE status:\n";
      for (const auto& kv : mGraphCoreTilesMap) {
        msg << kv.first << " : ";
        for (const auto& tile : kv.second)
          msg << "(" << +tile.col << "," << +(tile.row - offset) << "), ";
        msg << "\n";
      }
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    }
  }

  /****************************************************************************
   * Convert core status register value to readable string
   ***************************************************************************/
  std::string AIEStatusPlugin::getCoreStatusString(uint32_t status) {
    std::string statusStr;

    if (status & 0x000001)
      statusStr += "Enable,";
    if (status & 0x000002)
      statusStr += "Reset,";
    if (status & 0x000004)
      statusStr += "Memory_Stall_S,";
    if (status & 0x000008)
      statusStr += "Memory_Stall_W,";
    if (status & 0x000010)
      statusStr += "Memory_Stall_N,";
    if (status & 0x000020)
      statusStr += "Memory_Stall_E,";
    if (status & 0x000040)
      statusStr += "Lock_Stall_S,";
    if (status & 0x000080)
      statusStr += "Lock_Stall_W,";
    if (status & 0x000100)
      statusStr += "Lock_Stall_N,";
    if (status & 0x000200)
      statusStr += "Lock_Stall_E,";
    if (status & 0x000400)
      statusStr += "Stream_Stall_SS0,";
    if (status & 0x000800)
      statusStr += "Stream_Stall_SS1,";
    if (status & 0x001000)
      statusStr += "Stream_Stall_MS0,";
    if (status & 0x002000)
      statusStr += "Stream_Stall_MS1,";
    if (status & 0x004000)
      statusStr += "Cascade_Stall_SCD,";
    if (status & 0x008000)
      statusStr += "Cascade_Stall_MCD,";
    if (status & 0x010000)
      statusStr += "Debug_Halt,";
    if (status & 0x020000)
      statusStr += "ECC_Error_Stall,";
    if (status & 0x040000)
      statusStr += "ECC_Scrubbing_Stall,";
    if (status & 0x080000)
      statusStr += "Error_Halt,";
    if (status & 0x100000)
      statusStr += "Core_Done,";
    if (status & 0x200000)
      statusStr += "Core_Processor_Bus_Stall,";

    // remove trailing comma
    if (!statusStr.empty())
      statusStr.pop_back();

    return statusStr;
  }

  /****************************************************************************
   * Poll core status values to detect deadlock
   ***************************************************************************/
  void AIEStatusPlugin::pollDeadlock(uint64_t index, void* handle)
  {
    auto it = mThreadCtrlMap.find(handle);
    if (it == mThreadCtrlMap.end())
      return;

    // AIE core register offsets
    constexpr uint64_t AIE_OFFSET_CORE_STATUS = 0x32004;
    auto offset = metadataReader->getAIETileRowOffset();
    auto hwGen = metadataReader->getHardwareGeneration();

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

    // Tiles already reported with error(s)
    std::set<tile_type> errorTileSet;
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
          auto tileOffset = XAie_GetTileAddr(aieDevInst, tile.row, tile.col);
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

          // Check for errors in tile
          // NOTE: warning is only issued once per tile
          if (errorTileSet.find(tile) == errorTileSet.end()) {
            auto loc = XAie_TileLoc(tile.col, tile.row);

            // Memory module
            uint8_t memErrors = 0;
            XAie_EventReadStatus(aieDevInst, loc, XAIE_MEM_MOD, 
                XAIE_EVENT_GROUP_ERRORS_MEM, &memErrors);
            
            // Core module
            // NOTE: Per CR-1167717, ignore group errors on AIE1 devices 
            //       since instruction event 2 is used as DONE bit.
            uint8_t coreErrors0 = 0;
            uint8_t coreErrors1 = 0;
            if (hwGen > 1) {
              XAie_EventReadStatus(aieDevInst, loc, XAIE_CORE_MOD, 
                  XAIE_EVENT_GROUP_ERRORS_0_CORE, &coreErrors0);
              XAie_EventReadStatus(aieDevInst, loc, XAIE_CORE_MOD, 
                  XAIE_EVENT_GROUP_ERRORS_1_CORE, &coreErrors1);
            }

            if (memErrors || coreErrors0 || coreErrors1) {
              std::stringstream errorMessage;
              errorMessage << "Error(s) found in tile (" << +tile.col << "," << +(tile.row - offset)
                           << "). Please view status in Vitis Analyzer for specifics.";
              xrt_core::message::send(severity_level::warning, "XRT", errorMessage.str());
              errorTileSet.insert(tile);
            }
          }
        } // For tiles in graph

        std::stringstream warningMessage;
        if (graphStallCounter == graphTilesVec.size()) {
          if (xdp::HW_EMU != xdp::getFlowMode()) {
            // We have a stuck graph
            warningMessage
            << "Potential deadlock/hang found in AI Engines. Graph : " << graphName;
            xrt_core::message::send(severity_level::warning, "XRT", warningMessage.str());
          }
          // Send next warning if all tiles come out of hang & reach threshold again
          graphStallCounter = 0;
        } else if (foundStuckCores) {
          if (xdp::HW_EMU != xdp::getFlowMode()) {
            // We have a stuck core within this graph
            warningMessage
            << "Potential stuck cores found in AI Engines. Graph : " << graphName << " "
            << "Tile : " << "(" << +stuckTile.col << "," << +(stuckTile.row - offset) << ") "
            << "Status 0x" << std::hex << stuckCoreStatus << std::dec
            << " : " << getCoreStatusString(stuckCoreStatus);

            xrt_core::message::send(severity_level::warning, "XRT", warningMessage.str());
          }
          foundStuckCores = false;
        }

        // Print status for debug
        if (aie::isDebugVerbosity()) {
          std::stringstream msg;
          for (const auto& tile : graphTilesVec) {
            if (coreStuckCountMap[tile]) {
              msg
                << "T(" << +tile.col <<"," << +(tile.row - offset) << "):" << "<" << coreStuckCountMap[tile]
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

  /****************************************************************************
   * Periodically write status of active tiles
   ***************************************************************************/
  void AIEStatusPlugin::writeStatus(uint64_t index, void* handle, VPWriter* aieWriter)
  {
    auto it = mThreadCtrlMap.find(handle);
    if (it == mThreadCtrlMap.end())
      return;
    auto& shouldContinue = it->second;

    while (shouldContinue) {
      if (!(db->getStaticInfo().isDeviceReady(index)))
        continue;

      aieWriter->write(false, handle);
      std::this_thread::sleep_for(std::chrono::microseconds(mPollingInterval));
    }
  }

  /****************************************************************************
   * Update AIE device
   ***************************************************************************/
  void AIEStatusPlugin::updateAIEDevice(void* handle)
  {
    // Don't update if no debug/status is requested
    if (!xrt_core::config::get_aie_status())
      return;

    mXrtCoreDevice = xrt_core::get_userpf_device(handle);

    uint64_t deviceID = db->addDevice(util::getDebugIpLayoutPath(handle)); // Get the unique device Id

    if (!(db->getStaticInfo()).isDeviceReady(deviceID)) {
      // Update the static database with information from xclbin
      (db->getStaticInfo()).updateDevice(deviceID, nullptr, handle);
    }

    // Grab AIE metadata
    metadataReader = (db->getStaticInfo()).getAIEmetadataReader();
    if (!metadataReader)
      return;
    auto hwGen =  metadataReader->getHardwareGeneration();

    // Update list of tiles to debug
    getTilesForStatus();

    // Open the writer for this device
    std::string devicename = util::getDeviceName(handle);

    std::string currentTime = "0000_00_00_0000";
    auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm* p_tstruct = std::localtime(&time);
    if (p_tstruct) {
      char buf[80] = {0};
      strftime(buf, sizeof(buf), "%Y_%m_%d_%H%M%S", p_tstruct);
      currentTime = std::string(buf);
    }

    // Create and register AIE status writer
    std::string filename = "aie_status_" + devicename + "_" + currentTime + ".json";
    VPWriter* aieWriter = new AIEStatusWriter(filename.c_str(), devicename.c_str(), deviceID, hwGen, mXrtCoreDevice);
    writers.push_back(aieWriter);
    db->getStaticInfo().addOpenedFile(aieWriter->getcurrentFileName(), "AIE_RUNTIME_STATUS");

    // Start the AIE status thread
    mThreadCtrlMap[handle] = true;
    // NOTE: This does not start the threads immediately.
    mDeadlockThreadMap[handle] = std::thread { [=] { pollDeadlock(deviceID, handle); } };
    mStatusThreadMap[handle] = std::thread { [=] { writeStatus(deviceID, handle, aieWriter); } };
  }

  /****************************************************************************
   * End polling for device
   ***************************************************************************/
  void AIEStatusPlugin::endPollforDevice(void* handle)
  {
    // Last chance at writing status reports
    for (auto w : writers)
      w->write(false, handle);

    // When ending polling for a device, if we are on edge we must instead
    // shut down all of the threads and not just a single one in order
    // to avoid race conditions between the zynq driver destructor and our own.
    //
    // Currently, Edge is the only supported type of platform so we can
    // safely end all threads here, but this must be revisited if we extend
    // AIE status functionality to other types of platforms.
    endPoll();
  }

  /****************************************************************************
   * End all polling threads
   ***************************************************************************/
  void AIEStatusPlugin::endPoll()
  {
    // Ask all threads to end
    for (auto& p : mThreadCtrlMap)
      p.second = false;

    for (auto& t : mDeadlockThreadMap)
      t.second.join();

    for (auto& t : mStatusThreadMap)
      t.second.join();

    mThreadCtrlMap.clear();
    mDeadlockThreadMap.clear();
    mStatusThreadMap.clear();
  }

} // end namespace xdp
