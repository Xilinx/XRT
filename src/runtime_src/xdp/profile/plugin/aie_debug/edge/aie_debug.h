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
    //~AieDebug_EdgeImpl() = default;
    ~AieDebug_EdgeImpl();
    void updateDevice();
    void updateAIEDevice(void* handle);
    void poll(const uint32_t index, void* handle);

  private:

    std::vector<std::string> getSettingsVector(std::string settingsString);
    std::map<module_type, std::vector<uint64_t>> parseMetrics();
    std::vector<uint64_t> stringToRegList(std::string stringEntry, module_type t);

    std::map<xdp::tile_type, std::vector<uint64_t>> debugAddresses; //TODO delete this
    UsedRegisters* usedRegisters;
    //std::vector<BaseReadableTile> debugTiles;
    //std::map<xdp::tile_type, EdgeReadableTile*> debugTileMap; // TODO delete this unique pointer will take care of the deletion of all the pointers created
    std::map<xdp::tile_type, std::unique_ptr<EdgeReadableTile>> debugTileMap;
    const std::vector<XAie_ModuleType> falModuleTypes = {
      XAIE_CORE_MOD, XAIE_MEM_MOD, XAIE_PL_MOD, XAIE_MEM_MOD};
  };

  class EdgeReadableTile: public  BaseReadableTile{
  public:
    EdgeReadableTile(int r, int c) {
      row=r;
      col=c;
    }

    void readValues(XAie_DevInst* aieDevInst){
      int i=0;
      for(auto& absoluteOffset : absoluteOffsets) {
        //XAie_Read32(aieDevInst, absoluteOffset, &values[i++]);
        uint32_t val = 0;
        XAie_Read32(aieDevInst, absoluteOffset, &val);
        values.push_back(val); }
    }
};

} // end namespace xdp

#endif
