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
#include <memory>

#include "xdp/profile/plugin/aie_debug/aie_debug_impl.h"
#include "xdp/profile/plugin/aie_debug/aie_debug_metadata.h"
#include "xdp/profile/device/common/client_transaction.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/filetypes/base_filetype_impl.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

#include "core/include/xrt/xrt_hw_context.h"

extern "C" {
  #include <xaiengine.h>
  #include <xaiengine/xaiegbl_params.h>
}

namespace xdp {
  class ClientReadableTile;

  class AieDebug_WinImpl : public AieDebugImpl {
  public:
    AieDebug_WinImpl(VPDatabase* database, std::shared_ptr<AieDebugMetadata> metadata);
    ~AieDebug_WinImpl() = default;
    void updateDevice();
    void updateAIEDevice(void* handle);
    void poll(const uint64_t index, void* handle);

  private:
    xrt::hw_context hwContext;
    std::unique_ptr<aie::ClientTransaction> transactionHandler;
    uint8_t* txn_ptr;
    XAie_DevInst aieDevInst = {0};
    read_register_op_t* op;
    std::size_t op_size;

    std::map<xdp::tile_type, std::unique_ptr<ClientReadableTile>> debugTileMap;
  };

  class ClientReadableTile : public BaseReadableTile {
    public:
      ClientReadableTile(uint8_t c, uint8_t r, uint64_t to) {
        col = c;
        row = r;
        tileOffset = to;
      }
      void addValue(uint32_t val) {
        values.push_back(val);
      }
      void readValues(XAie_DevInst* /*aieDevInst*/) {}
  };
} // end namespace xdp

#endif
