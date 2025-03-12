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
#include "xaiefal/xaiefal.hpp"
#include "xdp/profile/plugin/aie_profile/aie_profile_metadata.h"
#include "xdp/profile/plugin/aie_profile/util/aie_profile_util.h"
#include "xdp/profile/database/static_info/aie_constructs.h"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

namespace xdp::aie::profile {  

   /**
   * @brief Configure stream switch ports for monitoring purposes
   * @param tile tile type
   * @param xaieTile tile instance in FAL/resource manager
   * @param loc tile location
   * @param type xdp module type
   * @param numCounters number of counters
   * @param metricSet metric set to be configured
   * @param channel0 first channel to be configured
   * @param channel1 second channel to be configured
   * @param startEvents vector of start events from metric set
   * @param endEvents vector of end events from metric set
   * @param streamPorts vector of stream ports used
   */
   void configStreamSwitchPorts(const tile_type& tile, xaiefal::XAieTile& xaieTile, 
                                const XAie_LocType loc, const module_type type, 
                                const uint32_t numCounters, const std::string metricSet, 
                                const uint8_t channel0, const uint8_t channel1, 
                                std::vector<XAie_Events>& startEvents,
                                std::vector<XAie_Events>& endEvents,
                                std::vector<std::shared_ptr<xaiefal::XAieStreamPortSelect>>& streamPorts);

  /**
   * @brief Configure performance counter for profile API
   * @param aieDevInst AIE device instance
   * @param aieDevice AIE device
   * @param metadata profile metadata
   * @param xaieModule module type used by FAL
   * @param xaieModType AIE driver module type
   * @param xdpModType xdp module type
   * @param metricSet metric set to be configured
   * @param startEvent start event for counter
   * @param endEvent end event for counter
   * @param resetEvent reset event for counter
   * @param pcIndex index of performance counter
   * @param counterIndex overall index of counter
   * @param threshold threshold value used by counter
   * @param retCounterEvent counter event
   * @param tile tile type
   * @param bcResourcesLatency vector of broadcast channels for latency calculations
   * @param adfAPIResourceInfoMap resource information map
   * @return shared pointer to performance counter used by FAL
   */
  std::shared_ptr<xaiefal::XAiePerfCounter>
  configProfileAPICounters(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice,
                           std::shared_ptr<AieProfileMetadata> metadata,
                           xaiefal::XAieMod& xaieModule, XAie_ModuleType& xaieModType, 
                           const module_type xdpModType, const std::string& metricSet, 
                           XAie_Events startEvent, XAie_Events endEvent, 
                           XAie_Events resetEvent, int pcIndex, size_t counterIndex, 
                           size_t threshold, XAie_Events& retCounterEvent, const tile_type& tile,
                           std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesLatency,
                           std::map<adfAPI, std::map<std::string, adfAPIResourceInfo>>& adfAPIResourceInfoMap,
                           std::map<std::string, std::pair<int, XAie_Events>>& adfAPIBroadcastEventsMap);

   /**
   * @brief Start a performance counter
   * @param pc performance counter
   * @param counterEvent counter event
   * @param retCounterEvent returned counter event
   * @return shared pointer to performance counter used by FAL
   */
   std::shared_ptr<xaiefal::XAiePerfCounter>
   startCounter(std::shared_ptr<xaiefal::XAiePerfCounter>& pc,
                XAie_Events counterEvent, XAie_Events& retCounterEvent);

   /**
   * @brief Configure performance counter using combo event 3 FSM
   * @param xaieModule module type used by FAL
   * @param xaieModType AIE driver module type
   * @param xdpModType xdp module type
   * @param metricSet metric set to be configured
   * @param startEvent start event for counter
   * @param endEvent end event for counter
   * @param resetEvent reset event for counter
   * @param pcIndex index of performance counter
   * @param threshold threshold value used by counter
   * @param retCounterEvent counter event
   * @return shared pointer to performance counter used by FAL
   */
   std::shared_ptr<xaiefal::XAiePerfCounter>
   configPCUsingComboEvents(xaiefal::XAieMod& xaieModule, XAie_ModuleType& xaieModType, 
                            const module_type xdpModType, const std::string& metricSet, 
                            XAie_Events startEvent, XAie_Events endEvent, XAie_Events resetEvent,
                            int pcIndex, size_t threshold, XAie_Events& retCounterEvent);

  /**
   * @brief Get broadcast channel in interface tile
   * @param aieDevice AIE device
   * @param srcTile source tile location
   * @param metadata profile metadata
   * @param bcResourcesLatency vector of broadcast channels for latency calculations
   * @return channel and event
   */
  std::pair<int, XAie_Events>
  getShimBroadcastChannel(xaiefal::XAieDev* aieDevice, const tile_type& srcTile, const tile_type& destTile,
                          std::shared_ptr<AieProfileMetadata> metadata,
                          std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesLatency,
                          std::map<std::string, std::pair<int, XAie_Events>>& adfAPIBroadcastEventsMap);

  /**
   * @brief Setup broadcast channel
   * @param aieDevice AIE device
   * @param currTileLoc current tile location
   * @param metadata profile metadata
   * @param bcResourcesLatency vector of broadcast channels for latency calculations
   * @return channel and event
   */
  std::pair<int, XAie_Events>
  setupBroadcastChannel(xaiefal::XAieDev* aieDevice, const tile_type& currTileLoc, 
                        std::shared_ptr<AieProfileMetadata> metadata,
                        std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesLatency,
                        std::map<std::string, std::pair<int, XAie_Events>>& adfAPIBroadcastEventsMap);
  
   std::pair<int, XAie_Events>
   getSetBroadcastChannel(xaiefal::XAieDev* aieDevice, const tile_type& currTileLoc, 
                          std::shared_ptr<AieProfileMetadata> metadata,
                          std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesLatency,
                          std::map<std::string, std::pair<int, XAie_Events>>& adfAPIBroadcastEventsMap);

  /**
   * @brief Configure interface tile counter for latency
   * @param aieDevInst AIE device instance
   * @param aieDevice AIE device
   * @param metadata profile metadata
   * @param xaieModule module type used by FAL
   * @param xaieModType AIE driver module type
   * @param xdpModType xdp module type
   * @param metricSet metric set to be configured
   * @param startEvent start event for counter
   * @param endEvent end event for counter
   * @param resetEvent reset event for counter
   * @param pcIndex index of performance counter
   * @param threshold threshold value used by counter
   * @param retCounterEvent counter event
   * @param tile tile type
   * @param isSource source or destination?
   * @param bcResourcesLatency vector of broadcast channels for latency calculations
   * @return shared pointer to performance counter used by FAL
   */
  std::shared_ptr<xaiefal::XAiePerfCounter>
  configInterfaceLatency(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice,
                         std::shared_ptr<AieProfileMetadata> metadata,
                         xaiefal::XAieMod& xaieModule, XAie_ModuleType& xaieModType, 
                         const module_type xdpModType, const std::string& metricSet, 
                         XAie_Events startEvent, XAie_Events endEvent, XAie_Events resetEvent, 
                         int pcIndex, size_t threshold, XAie_Events& retCounterEvent,
                         const tile_type& tile, bool& isSource,
                         std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesLatency,
                         std::map<std::string, std::pair<int, XAie_Events>>& adfAPIBroadcastEventsMap);

   /**
   * @brief Configure individual AIE events for metric sets related to Profile APIs
   * @param aieDevInst AIE device instance
   * @param aieDevice AIE device
   * @param metadata profile metadata
   * @param core module type used by FAL
   * @param loc tile location
   * @param xaieModType AIE driver module type
   * @param xdpModType xdp module type
   * @param metricSet metric set to be configured
   * @param bcEvent broadcast event
   * @return success of configuration
   */
   bool 
   configGraphIteratorAndBroadcast(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice,
                                   std::shared_ptr<AieProfileMetadata> metadata,
                                   xaiefal::XAieMod core, XAie_LocType loc, 
                                   const XAie_ModuleType xaieModType, const module_type xdpModType, 
                                   const std::string metricSet, XAie_Events& bcEvent,
                                   std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesBytesTx);

   /**
   * @brief Configure AIE Core module start on graph iteration count threshold
   * @param core module type used by FAL
   * @param iteration iteration count
   * @param retCounterEvent counter event
   * @return success of configuration
   */
   bool configStartIteration(xaiefal::XAieMod core, uint32_t iteration,
                             XAie_Events& retCounterEvent);

   /**
   * @brief Configure the broadcasting of provided module and event
   *        (Brodcasted from AIE Tile core module)
   * @param aieDevInst AIE device instance
   * @param aieDevice AIE device
   * @param metadata profile metadata
   * @param loc tile location
   * @param xdpModType xdp module type
   * @param metricSet metric set to be configured
   * @param xaieModType AIE driver module type
   * @param bcEvent broadcast event
   * @param bcChannelEvent broadcast channel event
   * @param bcResourcesBytesTx vector of broadcast channels
   */
   void configEventBroadcast(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice,
                             std::shared_ptr<AieProfileMetadata> metadata,
                             const XAie_LocType loc, const module_type xdpModType, 
                             const std::string metricSet, const XAie_ModuleType xaieModType, 
                             const XAie_Events bcEvent, XAie_Events& bcChannelEvent,
                             std::vector<std::shared_ptr<xaiefal::XAieBroadcast>>& bcResourcesBytesTx);

}  // namespace xdp::aie::profile

#endif
