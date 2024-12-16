// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

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
  };

#if 0
  class ClientReadableTile : public BaseReadableTile {
    public:
      ClientReadableTile(int r, int c) {
        row = r;
        col = c;
      }
      void readValues(XAie_DevInst* /*aieDevInst*/) {
        // Different Implementation. everything else same
      }
  };
#endif
} // end namespace xdp

#endif
