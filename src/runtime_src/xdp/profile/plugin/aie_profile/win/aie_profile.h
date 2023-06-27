/**
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef AIE_PROFILE_H
#define AIE_PROFILE_H

#include <cstdint>

#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_impl.h"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

namespace xdp {

  class AieProfile_WinImpl : public AieProfileImpl {
   public:
    AieProfile_WinImpl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata);
    ~AieProfile_WinImpl() = default;

    void updateDevice();
    void poll(uint32_t index, void* handle);
    void freeResources();
    bool setMetricsSettings(uint64_t deviceId, void* handle);
    xdp::module_type getModuleType(uint16_t absRow, XAie_ModuleType mod);
    bool isValidType(module_type type, XAie_ModuleType mod);
    bool isStreamSwitchPortEvent(const XAie_Events event);
    void configEventSelections(const XAie_LocType loc,
                                const xdp::module_type type,
                                const std::string metricSet,
                                const uint8_t channel0);
    void configGroupEvents(const XAie_LocType loc,
                           const XAie_ModuleType mod,
                           const XAie_Events event, 
                           std::string metricSet, 
                           uint8_t channel);
    uint32_t getCounterPayload( const tile_type& tile, 
                                         const xdp::module_type type, 
                                         uint16_t column, 
                                         uint16_t row, 
                                         XAie_Events startEvent, 
                                         const std::string metricSet,
                                         const uint8_t channel);

    void configStreamSwitchPorts(const tile_type& tile,
                                  const XAie_LocType loc,
                                  const module_type type,
                                  const std::string metricSet,
                                  const uint8_t channel);
   private:
      XAie_DevInst aieDevInst = { 0 };
      std::map<xdp::module_type, uint16_t> mCounterBases;
      std::map<std::string, std::vector<XAie_Events>> mCoreStartEvents;
      std::map<std::string, std::vector<XAie_Events>> mCoreEndEvents;
      std::map<std::string, std::vector<XAie_Events>> mMemoryStartEvents;
      std::map<std::string, std::vector<XAie_Events>> mMemoryEndEvents;
      std::map<std::string, std::vector<XAie_Events>> mShimStartEvents;
      std::map<std::string, std::vector<XAie_Events>> mShimEndEvents;
      std::map<std::string, std::vector<XAie_Events>> mMemTileStartEvents;
      std::map<std::string, std::vector<XAie_Events>> mMemTileEndEvents; 
     
  };

}  // namespace xdp

#endif
