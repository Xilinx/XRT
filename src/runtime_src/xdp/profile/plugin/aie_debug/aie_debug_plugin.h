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

#ifndef XDP_AIE_DEBUG_PLUGIN_DOT_H
#define XDP_AIE_DEBUG_PLUGIN_DOT_H

#include <memory>

#include "xdp/profile/plugin/aie_debug/aie_debug_impl.h"
#include "xdp/profile/plugin/aie_debug/aie_debug_metadata.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/filetypes/base_filetype_impl.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {

  class AieDebugPlugin : public XDPPlugin
  {
  public:
    AieDebugPlugin();
    ~AieDebugPlugin();
    static bool alive();
    void updateAIEDevice(void* handle);
    void endAIEDebugRead(void* handle);
    void endPollforDevice(void* handle);
    
  private:
    uint64_t getDeviceIDFromHandle(void* handle);

  private:
    static bool live;
    bool mPollRegisters = true;
    uint32_t mIndex = 0;

    // This struct and handleToAIEData map is created to provision multiple AIEs
    // on the same machine, each denoted by its own handle
    struct AIEData {
      uint64_t deviceID;
      bool valid;
      std::unique_ptr<AieDebugImpl> implementation;
      std::shared_ptr<AieDebugMetadata> metadata;
    };
    std::map<void*, AIEData> handleToAIEData;
  };

} // end namespace xdp

#endif
