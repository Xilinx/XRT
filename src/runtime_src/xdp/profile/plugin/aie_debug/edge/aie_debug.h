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

#ifndef AIE_DEBUG_H
#define AIE_DEBUG_H

#include <boost/property_tree/ptree.hpp>
#include <vector>

#include "xdp/profile/plugin/aie_debug/aie_debug_impl.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/filetypes/base_filetype_impl.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "xdp/profile/plugin/aie_debug/aie_debug_plugin.h"

#include "core/common/message.h"
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
    EdgeReadableTile(uint8_t c, uint8_t r, uint64_t to) {
      col = c;
      row = r;
      tileOffset = to;
    }

    void readValues(XAie_DevInst* aieDevInst) {
      std::stringstream msg;
      msg << "Debugging " << relativeOffsets.size() << " registers for tile " 
          << +col << "," << +row;
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());

      for (auto& offset : relativeOffsets) {
        uint32_t val = 0;
        XAie_Read32(aieDevInst, offset + tileOffset, &val);
        values.push_back(val); 
      }
    }
};

} // end namespace xdp

#endif
