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

#ifndef AIE_TRACE_UTIL_DOT_H
#define AIE_TRACE_UTIL_DOT_H

#include <cstdint>
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_metadata.h"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

namespace xdp::aie::trace {
  /**
   * @brief   Get metric sets for core modules
   * @details Depending on hardware generation, these sets can be supplemented 
   *          with counter events as those are dependent on counter #.
   * @param   hwGen Hardware generation
   * @return  Map of metric set names with vectors of event IDs
   */
  std::map<std::string, std::vector<XAie_Events>> getCoreEventSets(int hwGen);

  /**
   * @brief   Get metric sets for memory modules
   * @details Core events listed here are broadcast by the resource manager.
   *          These can be supplemented with counter events as those are 
   *          dependent on counter # (AIE1 only).
   * @param   hwGen Hardware generation
   * @return  Map of metric set names with vectors of event IDs
   */
  std::map<std::string, std::vector<XAie_Events>> getMemoryEventSets(int hwGen);

  /**
   * @brief   Get metric sets for memory tiles
   * @details This function is only applicable to hardware generations that
   *          contain memory tiles.
   * @param   hwGen Hardware generation
   * @return  Map of metric set names with vectors of event IDs 
   */
  std::map<std::string, std::vector<XAie_Events>> getMemoryTileEventSets(int hwGen);

  /**
   * @brief  Get metric sets for interface tiles
   * @param  hwGen Hardware generation
   * @return Map of metric set names with vectors of event IDs
   */
  std::map<std::string, std::vector<XAie_Events>> getInterfaceTileEventSets(int hwGen);

  /**
   * @brief  Get start events for core module counters
   * @param  hwGen  Hardware generation
   * @param  scheme Counter scheme
   * @return Vector of core module counter start events to use with event trace
   *         (empty if not applicable to hardware generation)
   */
  std::vector<XAie_Events> getCoreCounterStartEvents(int hwGen, std::string scheme);

  /**
   * @brief  Get end events for core module counters
   * @param  hwGen  Hardware generation
   * @param  scheme Counter scheme
   * @return Vector of core module counter end events to use with event trace
   *         (empty if not applicable to hardware generation)
   */
  std::vector<XAie_Events> getCoreCounterEndEvents(int hwGen, std::string scheme);

  /**
   * @brief  Get counter event values for core module counters
   * @param  hwGen  Hardware generation
   * @param  scheme Counter scheme
   * @return Vector of core module counter event values to use with event trace
   *         (empty if not applicable to hardware generation)
   */
  std::vector<uint32_t> getCoreCounterEventValues(int hwGen, std::string scheme);

  /**
   * @brief  Get start events for memory module counters
   * @param  hwGen  Hardware generation
   * @param  scheme Counter scheme
   * @return Vector of memory module counter start events to use with event trace
   *         (empty if not applicable to hardware generation)
   */
  std::vector<XAie_Events> getMemoryCounterStartEvents(int hwGen, std::string scheme);

  /**
   * @brief  Get end events for memory module counters
   * @param  hwGen  Hardware generation
   * @param  scheme Counter scheme
   * @return Vector of memory module counter end events to use with event trace
   *         (empty if not applicable to hardware generation)
   */
  std::vector<XAie_Events> getMemoryCounterEndEvents(int hwGen, std::string scheme);

  /**
   * @brief  Get counter event values for memory module counters
   * @param  hwGen  Hardware generation
   * @param  scheme Counter scheme
   * @return Vector of memory module counter event values to use with event trace
   *         (empty if not applicable to hardware generation)
   */
  std::vector<uint32_t> getMemoryCounterEventValues(int hwGen, std::string scheme);

  /**
   * @brief  Check if metric set contains DMA events
   * @param  metricSet Name of requested metric set
   * @return True if given metric set contains DMA event(s)
   */
  bool isDmaSet(const std::string metricSet);

  /**
   * @brief  Check if event is core module event
   * @param  event Event ID to check
   * @return True if given event is from a core module
   */
  bool isCoreModuleEvent(const XAie_Events event);

  /**
   * @brief  Check if event is generated by a stream switch monitor port
   * @param  event Event ID to check
   * @return True if given event is from a stream switch port
   */
  bool isStreamSwitchPortEvent(const XAie_Events event);

  /**
   * @brief  Check if event is a port running event
   * @param  event Event ID to check
   * @return True if given event is a port running event
   */
  bool isPortRunningEvent(const XAie_Events event);

  /**
   * @brief  Get port number from event
   * @param  event Event ID to check
   * @return Port number associated with given event (default: 0)
   */
  uint8_t getPortNumberFromEvent(XAie_Events event);

  /**
   * @brief  Get channel number from event
   * @param  event Event ID to check
   * @return Channel number associated with given event (default: -1)
   */
  int8_t getChannelNumberFromEvent(XAie_Events event);

  /**
   * @brief Print out trace event statistics across multiple tiles
   * @param m        Module index (used to get name)
   * @param numTiles Array of tile usage stats (length: NUM_TRACE_EVENTS)
   */
  void printTraceEventStats(int m, int numTiles[]);

  /**
   * @brief Modify events in metric set based on tile type and channel number
   * @param type      Module/tile type
   * @param subtype   Subtype of module/tile (0: PLIO, 1: GMIO)
   * @param metricSet Name of requested metric set
   * @param channel   Channel number
   * @param events    Vector of events in metric set (modified if needed)
   */
  void modifyEvents(module_type type, io_type subtype, const std::string metricSet,
                    uint8_t channel, std::vector<XAie_Events>& events);

  /**
   * @brief Build 2-channel broadcast network for specified tile range
   * @param aieDevInst     AIE device
   * @param metadata       Trace Metadata
   * @param broadcastId1   Broadcast channel 1
   * @param broadcastId2   Broadcast channel 2
   * @param event          Event to trigger broadcast network
   * @param startCol       Start column of the partition
   * @param numCols        Num of columns in the partition
   */
  void build2ChannelBroadcastNetwork(XAie_DevInst* aieDevInst, std::shared_ptr<AieTraceMetadata> metadata, uint8_t broadcastId1, uint8_t broadcastId2, XAie_Events event, uint8_t startCol, uint8_t numCols);

  /**
   * @brief Reset 2-channel broadcast network for specified tile range
   * @param aieDevInst     AIE device
   * @param metadata       Trace Metadata
   * @param broadcastId1   Broadcast channel 1
   * @param broadcastId2   Broadcast channel 2
   * @param startCol       Start column of the partition
   * @param numCols        Num of columns in the partition
   */
  void reset2ChannelBroadcastNetwork(XAie_DevInst* aieDevInst, std::shared_ptr<AieTraceMetadata> metadata, uint8_t broadcastId1, uint8_t broadcastId2, uint8_t startCol, uint8_t numCols);
}  // namespace xdp::aie::trace

#endif
