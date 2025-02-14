/**
 * Copyright (C) 2023-2025 Advanced Micro Devices, Inc. - All rights reserved
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

#include "xdp/profile/plugin/aie_debug/edge/aie_debug.h"
#include "xdp/profile/plugin/aie_debug/aie_debug_metadata.h"
#include "xdp/profile/plugin/aie_debug/generations/aie1_attributes.h"
#include "xdp/profile/plugin/aie_debug/generations/aie1_registers.h"
#include "xdp/profile/plugin/aie_debug/generations/aie2_attributes.h"
#include "xdp/profile/plugin/aie_debug/generations/aie2_registers.h"
#include "xdp/profile/plugin/aie_debug/generations/aie2ps_attributes.h"
#include "xdp/profile/plugin/aie_debug/generations/aie2ps_registers.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <string>

#include "core/common/message.h"
#include "core/common/time.h"
#include "core/edge/user/shim.h"
#include "core/include/xrt/xrt_kernel.h"
#include "core/common/api/bo_int.h"
#include "core/common/api/hw_context_int.h"
#include "core/common/config_reader.h"
#include "core/include/experimental/xrt-next.h"

#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/vp_base/info.h"

namespace {
  static void* fetchAieDevInst(void* devHandle)
  {
    auto drv = ZYNQ::shim::handleCheck(devHandle);
    if (!drv)
      return nullptr ;
    auto aieArray = drv->getAieArray();
    if (!aieArray)
      return nullptr;
    return aieArray->get_dev();
  }

  static void* allocateAieDevice(void* devHandle)
  {
    auto aieDevInst = static_cast<XAie_DevInst*>(fetchAieDevInst(devHandle));
    if (!aieDevInst)
      return nullptr;
    return new xaiefal::XAieDev(aieDevInst, false);
  }

  static void deallocateAieDevice(void* aieDevice)
  {
    auto object = static_cast<xaiefal::XAieDev*>(aieDevice);
    if (object != nullptr)
      delete object;
  }
} // end anonymous namespace

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  using tile_type = xdp::tile_type;
  using module_type = xdp::module_type;

  /****************************************************************************
   * Edge constructor
   ***************************************************************************/
  AieDebug_EdgeImpl::AieDebug_EdgeImpl(VPDatabase* database, std::shared_ptr<AieDebugMetadata> metadata)
    : AieDebugImpl(database, metadata)
  {
    // Nothing to do
  }

  /****************************************************************************
   * Edge destructor
   ***************************************************************************/
  AieDebug_EdgeImpl::~AieDebug_EdgeImpl() {
    // Nothing to do
  }

  /****************************************************************************
   * Poll all registers
   ***************************************************************************/
  void AieDebug_EdgeImpl::poll(const uint64_t deviceID, void* handle)
  {
    xrt_core::message::send(severity_level::debug, "XRT", "Calling AIE Debug Poll.");

    if (!(db->getStaticInfo().isDeviceReady(deviceID))) {
      xrt_core::message::send(severity_level::debug, "XRT", 
        "Device is not ready, so no debug polling will occur.");
      return;
    }

    XAie_DevInst* aieDevInst =
      static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle));
    if (!aieDevInst) {
      xrt_core::message::send(severity_level::debug, "XRT", 
        "AIE device instance is not available, so no debug polling will occur.");
      return;
    }

    xrt_core::message::send(severity_level::debug, "XRT", 
      "Debugging registers for " + std::to_string(debugTileMap.size()) + " AIE tiles.");

    for (auto& tileAddr : debugTileMap) {
      tileAddr.second->readValues(aieDevInst);
      tileAddr.second->printValues(deviceID, db);
    }
  }

  /****************************************************************************
   * Update device
   ***************************************************************************/
  void AieDebug_EdgeImpl::updateDevice()
  {
    // Do nothing for now
  }

  /****************************************************************************
   * Compile list of registers to read
   ***************************************************************************/
  void AieDebug_EdgeImpl::updateAIEDevice(void* handle)
  {
    if (!xrt_core::config::get_aie_debug())
      return;
    XAie_DevInst* aieDevInst =
      static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle));

    std::unique_ptr<AieDebugPlugin> debugPlugin;
    auto regValues = metadata->getRegisterValues();

    // Traverse all module types
    int counterId = 0;
    for (int module = 0; module < metadata->getNumModules(); ++module) {
      auto configMetrics = metadata->getConfigMetricsVec(module);
      if (configMetrics.empty())
        continue;

      module_type mod = metadata->getModuleType(module);
      auto name = moduleTypes.at(mod);

      // List of registers to read for current module
      auto& Regs = regValues[mod];
      if (Regs.empty())
        continue;

      if (aie::isDebugVerbosity()) {
        std::stringstream msg;
        msg << "AIE Debug monitoring tiles of type " << name << ":\n";
        for (auto& tileMetric : configMetrics)
          msg << +tileMetric.first.col << "," << +tileMetric.first.row << " ";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
      }

      // Traverse all active and/or requested tiles
      for (auto& tileMetric : configMetrics) {
        auto tile       = tileMetric.first;
        auto tileOffset = XAie_GetTileAddr(aieDevInst, tile.row, tile.col);
        
        // Traverse all registers within tile
        for (auto& regAddr : Regs) {
          if (debugTileMap.find(tile) == debugTileMap.end())
            debugTileMap[tile] = std::make_unique<EdgeReadableTile>(tile.col, tile.row, tileOffset);
        
          auto regName = metadata->lookupRegisterName(regAddr);
          debugTileMap[tile]->addOffsetName(regAddr, regName);
        }
      }
    }
  }

}  // end namespace xdp
