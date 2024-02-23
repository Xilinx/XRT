// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef XDP_AIE_DEBUG_PLUGIN_DOT_H
#define XDP_AIE_DEBUG_PLUGIN_DOT_H

#include <boost/property_tree/ptree.hpp>
#include <memory>

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

  class AieDebugPlugin : public XDPPlugin
  {
  public:
    AieDebugPlugin();
    ~AieDebugPlugin();
    void updateAIEDevice(void* handle);
    void endAIEDebugRead(void* handle);
    static bool alive();

  private:
    void endPoll();
    void poll();
    std::vector<std::string> getSettingsVector(std::string settingsString);
    std::map<module_type, std::vector<uint64_t>> parseMetrics();
    aie::driver_config getAIEConfigMetadata();
    uint64_t getDeviceIDFromHandle(void* handle);

    const std::map<module_type, const char*> moduleTypes = {
      {module_type::core, "AIE"},
      {module_type::dma, "DMA"},
      {module_type::shim, "Interface"},
      {module_type::mem_tile, "Memory Tile"}
    };
    
    std::unique_ptr<aie::ClientTransaction> transactionHandler;
    uint8_t* txn_ptr;
    XAie_DevInst aieDevInst = {0};
    const aie::BaseFiletypeImpl* metadataReader = nullptr;
    aie_profile_op_t* op;
    std::size_t op_size;

    static bool live;
    struct AIEData {
      uint64_t deviceID;
      std::atomic<bool> threadCtrlBool;
      std::thread thread;
    };
    std::map<void*, AIEData>  handleToAIEData;

  };

} // end namespace xdp

#endif
