// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef AIE_DEBUG_H
#define AIE_DEBUG_H

#include <boost/property_tree/ptree.hpp>
#include <vector>

#include "xdp/profile/plugin/aie_debug/aie_debug_impl.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/filetypes/base_filetype_impl.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "xdp/profile/plugin/aie_debug/aie_debug_plugin.h"

#include "core/edge/common/aie_parser.h"
#include "xaiefal/xaiefal.hpp"

extern "C" {
  #include <xaiengine.h>
  #include <xaiengine/xaiegbl_params.h>
}

namespace xdp {
  class EdgeReadableTile;

  class AieDebug_EdgeImpl : public AieDebugImpl {
  public:
    AieDebug_EdgeImpl(VPDatabase* database, std::shared_ptr<AieDebugMetadata> metadata);
    ~AieDebug_EdgeImpl();
    void updateDevice();
    void updateAIEDevice(void* handle);
    void poll(const uint64_t index, void* handle);

  private:

    std::map<xdp::tile_type, std::vector<uint64_t>> debugAddresses;
    std::map<xdp::tile_type, std::unique_ptr<EdgeReadableTile>> debugTileMap;
    const std::vector<XAie_ModuleType> falModuleTypes = {
      XAIE_CORE_MOD, XAIE_MEM_MOD, XAIE_PL_MOD, XAIE_MEM_MOD};
  };

  class EdgeReadableTile: public BaseReadableTile {
  public:
    EdgeReadableTile(uint8_t c, uint8_t r) {
      col = c;
      row = r;
    }

    void readValues(XAie_DevInst* aieDevInst) {
      for (auto& offset : absoluteOffsets) {
        uint32_t val = 0;
        XAie_Read32(aieDevInst, offset, &val);
        values.push_back(val); 
      }
    }
};

} // end namespace xdp

#endif
