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
  typedef struct {
    uint64_t perf_address[256];
    uint32_t perf_value[256];
  } aie_profile_op_t;


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
    void configEventSelections(
      const XAie_LocType loc, const xdp::module_type type,
      const std::string metricSet, const uint8_t channel0
    );
    void configGroupEvents(
      const XAie_LocType loc, const XAie_ModuleType mod,
      const XAie_Events event, const std::string& metricSet, 
      uint8_t channel
    );
    uint32_t getCounterPayload(const tile_type& tile, 
      const xdp::module_type type, uint16_t column, 
      uint16_t row, XAie_Events startEvent, 
      const std::string metricSet, const uint8_t channel
    );
    void configStreamSwitchPorts(
      const tile_type& tile, const XAie_LocType& loc,
      const module_type& type, const std::string& metricSet,
      uint8_t channel
    );
   private:
      const std::map<xdp::module_type, uint16_t> mCounterBases = {
        {module_type::core,     static_cast<uint16_t>(0)},
        {module_type::dma,      BASE_MEMORY_COUNTER},
        {module_type::shim,     BASE_SHIM_COUNTER},
        {module_type::mem_tile, BASE_MEM_TILE_COUNTER}
      };

      const std::vector<XAie_Events> mSSEventList = {
        XAIE_EVENT_PORT_RUNNING_0_CORE,
        XAIE_EVENT_PORT_STALLED_0_CORE,
        XAIE_EVENT_PORT_RUNNING_0_PL,
        XAIE_EVENT_PORT_RUNNING_0_MEM_TILE,
        XAIE_EVENT_PORT_STALLED_0_MEM_TILE,
        XAIE_EVENT_PORT_TLAST_0_MEM_TILE
      };

      const std::map<module_type, std::vector<uint64_t>> regValues {
        {module_type::core, {0x31520,0x31524,0x31528,0x3152C}}, 
        {module_type::dma, {0x11020,0x11024}}, 
        {module_type::shim, {0x31020, 0x31024}}, 
        {module_type::mem_tile, {0x91020,0x91024,0x91028,0x9102C}}, 
      };

      std::map<std::string, std::vector<XAie_Events>> mCoreStartEvents;
      std::map<std::string, std::vector<XAie_Events>> mCoreEndEvents;
      std::map<std::string, std::vector<XAie_Events>> mMemoryStartEvents;
      std::map<std::string, std::vector<XAie_Events>> mMemoryEndEvents;
      std::map<std::string, std::vector<XAie_Events>> mShimStartEvents;
      std::map<std::string, std::vector<XAie_Events>> mShimEndEvents;
      std::map<std::string, std::vector<XAie_Events>> mMemTileStartEvents;
      std::map<std::string, std::vector<XAie_Events>> mMemTileEndEvents;

      xrt::kernel mKernel;
      xrt::bo input_bo;
      aie_profile_op_t op = {0};   
      XAie_DevInst aieDevInst = {0};
      std::vector<std::vector<uint64_t>> outputValues;

  };

}  // namespace xdp

#endif
