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
#include "xdp/profile/database/static_info/aie_constructs.h"

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

}  // namespace xdp::aie::profile

#endif
