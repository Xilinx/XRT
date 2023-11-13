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

#include "core/edge/common/aie_parser.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_impl.h"
#include "xaiefal/xaiefal.hpp"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

namespace xdp {
  using tile_type = xdp::tile_type;
  
  class AieProfile_EdgeImpl : public AieProfileImpl{
    public:
      // AieProfile_EdgeImpl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata)
      //   : AieProfileImpl(database, metadata){}
      AieProfile_EdgeImpl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata);

      ~AieProfile_EdgeImpl() = default;

      void updateDevice();
      void poll(uint32_t index, void* handle);
      void freeResources();
      bool checkAieDevice(uint64_t deviceId, void* handle);

      bool setMetricsSettings(uint64_t deviceId, void* handle);
      uint16_t getRelativeRow(uint16_t absRow);
      module_type getModuleType(uint16_t absRow, XAie_ModuleType mod);
      bool isInputSet(const module_type type, const std::string metricSet);
      bool isValidType(module_type type, XAie_ModuleType mod);
      bool isStreamSwitchPortEvent(const XAie_Events event);
      bool isPortRunningEvent(const XAie_Events event);
      bool isPortTlastEvent(const XAie_Events event);
      uint8_t getPortNumberFromEvent(XAie_Events event);
      void printTileModStats(xaiefal::XAieDev* aieDevice, 
                             const tile_type& tile, 
                             const XAie_ModuleType mod);
      void modifyEvents(module_type type, uint16_t subtype, 
                        uint8_t channel, std::vector<XAie_Events>& events);
      void configGroupEvents(XAie_DevInst* aieDevInst,
                             const XAie_LocType loc,
                             const XAie_ModuleType mod,
                             const module_type type,
                             const std::string metricSet,
                             const XAie_Events event,
                             const uint8_t channel = 0);
      void configStreamSwitchPorts(XAie_DevInst* aieDevInst,
                                   const tile_type& tile,
                                   xaiefal::XAieTile& xaieTile,
                                   const XAie_LocType loc,
                                   const module_type type,
                                   const uint32_t numCounters,
                                   const std::string metricSet,
                                   const uint8_t channel0,
                                   const uint8_t channel1,
                                   std::vector<XAie_Events>& startEvents,
                                   std::vector<XAie_Events>& endEvents);
      void configEventSelections(XAie_DevInst* aieDevInst,
                                 const XAie_LocType loc,
                                 const XAie_ModuleType mod,
                                 const module_type type,
                                 const std::string metricSet,
                                 const uint8_t channel0,
                                 const uint8_t channel1);
      uint32_t getCounterPayload(XAie_DevInst* aieDevInst,
                                 const tile_type& tile,
                                 const module_type type,
                                 uint16_t column, 
                                 uint16_t row, 
                                 uint16_t startEvent,
                                 const std::string metricSet,
                                 const uint8_t channel);

    private:
      XAie_DevInst*     aieDevInst = nullptr;
      xaiefal::XAieDev* aieDevice  = nullptr;    

      std::map<module_type, uint32_t> mCounterBases;
      std::map<std::string, std::vector<XAie_Events>> mCoreStartEvents;
      std::map<std::string, std::vector<XAie_Events>> mCoreEndEvents;
      std::map<std::string, std::vector<XAie_Events>> mMemoryStartEvents;
      std::map<std::string, std::vector<XAie_Events>> mMemoryEndEvents;
      std::map<std::string, std::vector<XAie_Events>> mShimStartEvents;
      std::map<std::string, std::vector<XAie_Events>> mShimEndEvents;
      std::map<std::string, std::vector<XAie_Events>> mMemTileStartEvents;
      std::map<std::string, std::vector<XAie_Events>> mMemTileEndEvents; 
      std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> mPerfCounters;
      std::vector<std::shared_ptr<xaiefal::XAieStreamPortSelect>> mStreamPorts;

  };

}   

#endif
