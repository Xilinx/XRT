// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef XDP_AIE_DEBUG_PLUGIN_DOT_H
#define XDP_AIE_DEBUG_PLUGIN_DOT_H

#include <boost/property_tree/ptree.hpp>
#include <vector>

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

  class AieDebug_EdgeImpl : public AieDebugImpl {
  public:
    AieDebug_EdgeImpl(VPDatabase* database, std::shared_ptr<AieDebugMetadata> metadata);
    ~AieDebug_EdgeImpl() = default;
    void updateAIEDevice(void* handle);
    void endAIEDebugRead(void* handle);
  
  private:
    void poll();

    std::map<xdp::tile_type, uint64_t> debugAddresses;
  };

} // end namespace xdp

#endif
