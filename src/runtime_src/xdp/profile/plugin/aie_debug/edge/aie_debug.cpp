// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

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

#include "core/common/api/bo_int.h"
#include "core/common/api/hw_context_int.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/include/experimental/xrt-next.h"

#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/vp_base/info.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  using tile_type = xdp::tile_type;
  using module_type = xdp::module_type;

  /****************************************************************************
   * Client constructor
   ***************************************************************************/
  AieDebug_EdgeImpl::AieDebug_EdgeImpl(VPDatabase* database, std::shared_ptr<AieDebugMetadata> metadata)
    : AieDebugImpl(database, metadata)
  {
    // Do nothing
  }

  /****************************************************************************
   * Poll all registers
   ***************************************************************************/
  void AieDebug_EdgeImpl::poll()
  {
    xrt_core::message::send(severity_level::debug, "XRT", "Calling AIE Poll.");

    for (auto& tileAddr : debugAddresses) {
      auto addr = tileAddr.second;
      auto tile = tileAddr.first;
      
      uint32_t value = 0;
      XAie_Read32(aieDevInst, addr, &value);

      msg << "Debug tile (" << tile.col << ", " << tile.row << ") "
          << "hex address/values: 0x" << std::hex << addr << " : 0x"
          << value << std::dec;
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
    }
  }

  /****************************************************************************
   * Finish debugging
   ***************************************************************************/
  void AieDebug_EdgeImpl::endAIEDebugRead(void* /*handle*/)
  {
    static bool finished = false;
    if (!finished) {
      finished = true;
      poll();
    }
  }

  /****************************************************************************
   * Compile list of registers to read
   ***************************************************************************/
  void AieDebug_EdgeImpl::updateAIEDevice(void* handle) 
  {
    if (!xrt_core::config::get_aie_debug())
      return;

    // Traverse all module types
    int counterId = 0;
    for (int module = 0; module < metadata->getNumModules(); ++module) {
      auto configMetrics = metadata->getConfigMetricsVec(module);
      if (configMetrics.empty())
        continue;
      
      XAie_ModuleType mod = aie::profile::getFalModuleType(module);
      auto name = moduleTypes[mod];

      std::stringstream msg;
      msg << "AIE Debug monitoring tiles of type " << name << ":\n";
      for (auto& tileMetric : configMetrics)
        msg << tileMetric.first.col << "," << tileMetric.first.row << " ";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());

      // Traverse all active tiles for this module type
      for (auto& tileMetric : configMetrics) {
        auto& metricSet  = tileMetric.second;
        auto tile        = tileMetric.first;
  
        // TODO: replace with gen-specific addresses
        uint32_t offset = 0;

        auto tileOffset = XAie_GetTileAddr(aieDevInst, tile.row, tile.col);
        debugAddresses.push_back(tileOffset + offset);
      }
    }
  }

}  // end namespace xdp
