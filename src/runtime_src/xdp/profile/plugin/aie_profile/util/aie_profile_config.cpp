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
#include "xdp/profile/database/static_info/aie_constructs.h"
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

 
  /****************************************************************************
   * Configure the individual AIE events for metric sets related to Profile APIs
   ***************************************************************************/
   void configGraphIteratorAndBroadcast(xaiefal::XAieDev* aieDevice, XAie_DevInst* aieDevInst, xaiefal::XAieMod core,
                      XAie_LocType loc, const XAie_ModuleType xaieModType,
                      const module_type xdpModType, const std::string metricSet,
                      uint32_t iterCount, XAie_Events& bcEvent, std::shared_ptr<AieProfileMetadata> metadata)
  {
    if (!aie::profile::metricSupportsGraphIterator(metricSet))
      return;
   
    if (xdpModType != module_type::core) {
      auto aieCoreTilesVec = metadata->getTiles("all", module_type::core, "all");
      if (aieCoreTilesVec.empty()) {
        std::stringstream msg;
        msg << "No core tiles available, graph ieration profiling will not be available.\n";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
      }
      
      // Use the first available core tile to configure the broadcasting
      uint8_t col = aieCoreTilesVec.begin()->col;
      uint8_t row = aieCoreTilesVec.begin()->row;
      auto& xaieTile   = aieDevice->tile(col, row);
      core = xaieTile.core();
      loc = XAie_TileLoc(col, row);
    }

    XAie_Events counterEvent;
    // Step 1: Configure the graph iterator event
    aie::profile::configStartIteration(core, iterCount, counterEvent)
    
    // Step 2: Configure the brodcast of the returned counter event
    XAie_Events bcChannelEvent;
    configEventBroadcast(aieDevInst, loc, xdpModType, metricSet, xaieModType,
                          counterEvent, bcChannelEvent);

    // Store the brodcasted channel event for later use
    bcEvent = bcChannelEvent;
  }

  /****************************************************************************
   * Configure AIE Core module start on graph iteration count threshold
   ***************************************************************************/
  bool configStartIteration(xaiefal::XAieMod core, uint32_t iteration,
                            XAie_Events& retCounterEvent)
  {
    XAie_ModuleType mod = XAIE_CORE_MOD;
    // Count up by 1 for every iteration
    auto pc = core.perfCounter();
    if (pc->initialize(mod, XAIE_EVENT_INSTR_EVENT_0_CORE, 
                       mod, XAIE_EVENT_INSTR_EVENT_0_CORE) != XAIE_OK)
      return false;
    if (pc->reserve() != XAIE_OK)
      return false;

    xrt_core::message::send(severity_level::debug, "XRT", 
        "Configuring AIE profile to start on iteration " + std::to_string(iteration));

    pc->changeThreshold(iteration);

    XAie_Events counterEvent;
    pc->getCounterEvent(mod, counterEvent);

    // performance counter event to use it later for broadcasting
    retCounterEvent = counterEvent;
    return true;
  }

  /****************************************************************************
   * Configure the broadcasting of provided module and event
   * (Brodcasted from AIE Tile core module)
   ***************************************************************************/
  void configEventBroadcast(XAie_DevInst* aieDevInst,
                        const XAie_LocType loc,
                        const module_type xdpModType,
                        const std::string metricSet,
                        const XAie_ModuleType xaieModType,
                        const XAie_Events bcEvent,
                        XAie_Events& bcChannelEvent)
  {
    if ((xaieModType != XAIE_CORE_MOD) || (xdpModType != module_type::core))
      return;

    // Each module has 16 broadcast channels (0-15). It is safe to use 
    // later channel Ids considering other channel IDs being used.
    // Use by default brodcastId 11 for interface_tile_latency start trigger
    uint8_t brodcastId = 11;

    int driverStatus   = AieRC::XAIE_OK;
    driverStatus |= XAie_EventBroadcast(aieDevInst, loc, XAIE_CORE_MOD, brodcastId, bcEvent);
    if (driverStatus!= XAIE_OK) {
      std::stringstream msg;
      msg << "Configuration of graph iteration event from core tile "<< +loc.Col << ", " << +loc.Row
          <<"is unavailable, graph ieration profiling will not be available.\n";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    }

    // This is the broadcast channel event seen in interface tiles
    // TODO: To be replace with more structured way of procuring events
    bcChannelEvent = XAIE_EVENT_BROADCAST_A_11_PL;
  }


}  // namespace xdp
