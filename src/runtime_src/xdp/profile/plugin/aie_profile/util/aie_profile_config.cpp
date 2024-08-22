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

#define XDP_PLUGIN_SOURCE

#include "xdp/profile/plugin/aie_profile/util/aie_profile_config.h"
#include "xdp/profile/plugin/aie_profile/util/aie_profile_util.h"
#include "xdp/profile/database/static_info/aie_util.h"

#include <cmath>
#include <cstring>
#include <memory>
#include "core/common/message.h"


namespace xdp::aie::profile {
  using severity_level = xrt_core::message::severity_level;

  /****************************************************************************
   * Configure the individual AIE events for metric sets that use group events
   ***************************************************************************/
  void configGroupEvents(XAie_DevInst* aieDevInst, const XAie_LocType loc,
                          const XAie_ModuleType mod, const module_type type,
                          const std::string metricSet, const XAie_Events event,
                          const uint8_t channel) 
  {
    // Set masks for group events
    // NOTE: Group error enable register is blocked, so ignoring
    if (event == XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_DMA_MASK);
    else if (event == XAIE_EVENT_GROUP_LOCK_MEM)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_LOCK_MASK);
    else if (event == XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_CONFLICT_MASK);
    else if (event == XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_CORE_PROGRAM_FLOW_MASK);
    else if (event == XAIE_EVENT_GROUP_CORE_STALL_CORE)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_CORE_STALL_MASK);
    else if (event == XAIE_EVENT_GROUP_DMA_ACTIVITY_PL) {
      uint32_t bitMask = aie::isInputSet(type, metricSet) 
          ? ((channel == 0) ? GROUP_SHIM_S2MM0_STALL_MASK : GROUP_SHIM_S2MM1_STALL_MASK)
          : ((channel == 0) ? GROUP_SHIM_MM2S0_STALL_MASK : GROUP_SHIM_MM2S1_STALL_MASK);
      XAie_EventGroupControl(aieDevInst, loc, mod, event, bitMask);
    }                                    
  }

  /****************************************************************************
   * Configure the selection index to monitor channel number in memory tiles
   ***************************************************************************/
  void configEventSelections(XAie_DevInst* aieDevInst,
                        const XAie_LocType loc,
                        const module_type type,
                        const std::string metricSet,
                        const uint8_t channel)
  {
    if (type != module_type::mem_tile)
      return;

    XAie_DmaDirection dmaDir = aie::isInputSet(type, metricSet) ? DMA_S2MM : DMA_MM2S;
    XAie_EventSelectDmaChannel(aieDevInst, loc, 0, dmaDir, channel);

    std::stringstream msg;
    msg << "Configured mem tile " << (aie::isInputSet(type,metricSet) ? "S2MM" : "MM2S") 
    << "DMA  for metricset " << metricSet << ", channel " << (int)channel << ".";
    xrt_core::message::send(severity_level::debug, "XRT", msg.str());
  } 

}  // namespace xdp
