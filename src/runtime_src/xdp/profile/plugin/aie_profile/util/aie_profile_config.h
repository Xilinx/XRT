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

#ifndef AIE_PROFILE_CONFIG_DOT_H
#define AIE_PROFILE_CONFIG_DOT_H

#include <cstdint>
#include <memory>
#include "xaiefal/xaiefal.hpp"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_metadata.h"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

namespace xdp::aie::profile {  

   /**
   * @brief Configure the individual AIE events for metric sets that use group events
   * @param aieDevInst AIE device instance
   * @param loc Tile location
   * @param mod AIE driver module type
   * @param type xdp module type
   * @param metricSet metric set to be configured
   * @param event metric set group event
   * @param channel channel to be configured
   */
   void configGroupEvents(XAie_DevInst* aieDevInst, const XAie_LocType loc,
                          const XAie_ModuleType mod, const module_type type,
                          const std::string metricSet, const XAie_Events event,
                          const uint8_t channel);


  /**
   * @brief Configure the selection index to monitor channel number in memory tiles
   * @param aieDevInst AIE device instance
   * @param loc Tile location
   * @param type xdp module type
   * @param metricSet metric set to be configured
   * @param channel channel to be configured
   * 
   */
  void configEventSelections(XAie_DevInst* aieDevInst,
                              const XAie_LocType loc,
                              const module_type type,
                              const std::string metricSet,
                              const uint8_t channel);

  /**
   * @brief Configure AIE Core module start on graph iteration count threshold
   * @param aieDevInst       AIE device instance
   * @param iteration        Graph iteration count to configure counters
   * @param retCounterEvent  Allocated Profile counter resource event
   * 
   */
  bool configStartIteration(xaiefal::XAieMod core, uint32_t iteration,
                            XAie_Events& retCounterEvent);

  /**
   * @brief Configure the AIE Core module to brodcast event to provided
   *        module type and sets the associated brodcast channel event
   * @param aieDevInst     AIE device instance
   * @param loc            Tile location
   * @param xdpModType     Xdp module type
   * @param metricSet      Metric set to be configured
   * @param xaieModType    Xaie module type
   * @param bcEvent        Event to configure broadcast on
   * @param bcChannelEvent Brodcasted channel event
   * 
   */
  void configEventBroadcast(XAie_DevInst* aieDevInst,
                            const XAie_LocType loc,
                            const module_type xdpModType,
                            const std::string metricSet,
                            const XAie_ModuleType xaieModType,
                            const XAie_Events bcEvent,
                            XAie_Events& bcChannelEvent);

  /**
   * @brief Configure the AIE Core module to monitor total graph iteration count
   *        and brodcasts the state to provided module type
   * @param aieDevInst     AIE device instance
   * @param loc            Tile location
   * @param xaieModType    XAIE module type
   * @param xdpModType        Xdp module type
   * @param metricSet      Metric set to be configured
   * 
   */
   void configGraphIteratorAndBroadcast(xaiefal::XAieDev* aieDevice, XAie_DevInst* aieDevInst, xaiefal::XAieMod core,
                      XAie_LocType loc, const XAie_ModuleType xaieModType,
                      const module_type xdpModType, const std::string metricSet,
                      uint32_t iterCount, XAie_Events& bcEvent, std::shared_ptr<AieProfileMetadata> metadata);

/*
void configInterfaceTilesRunningOrStalledCount(xaiefal::XAieMod& mod, 
                             XAie_ModuleType& startMod, XAie_Events& startEvent,
                             XAie_ModuleType& stopMod, XAie_Events& stopEvent,
                             XAie_ModuleType& resetMod, XAie_Events& resetEvent,
                             uint32_t iteration);

void configInterfaceTilesLatencyTrigger(XAie_DevInst* aieDevInst,
                                        const XAie_LocType tileloc,
                                        const module_type modType,
                                        const std::string metricSet,
                                        const XAie_Events& userEventTrigger);
 

std::vector<XAie_Events>
configComboEvents(XAie_DevInst* aieDevInst, xaiefal::XAieTile& xaieTile, 
                  const XAie_LocType loc, const XAie_ModuleType mod,
                  const module_type type, const std::string metricSet,
                  aie_cfg_base& config)

*/

}  // namespace xdp::aie::profile

#endif
