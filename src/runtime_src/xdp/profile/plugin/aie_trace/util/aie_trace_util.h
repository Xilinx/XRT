// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved

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
   * @param hwGen     Hardware generation
   */
  void modifyEvents(module_type type, io_type subtype, const std::string metricSet,
                    uint8_t channel, std::vector<XAie_Events>& events, const int hwGen);

  /**
   * @brief Configure group events (core modules only)
   * @param aieDevInst AIE device instance
   * @param loc        Location of tile
   * @param mod        Module type (used by driver)
   * @param type       Module/tile type
   * @param metricSet  Name of requested metric set
   */
  void configGroupEvents(XAie_DevInst* aieDevInst, const XAie_LocType loc,
                         const XAie_ModuleType mod, const module_type type, 
                         const std::string metricSet);

  /**
  * @brief Configure event selections for DMA channels
  * @param aieDevInst AIE device instance
  * @param tile       Tile metadata
  * @param loc        Location of tile
  * @param type       Module/tile type
  * @param metricSet  Name of requested metric set
  * @param channel0   First specified channel number
  * @param channel1   Second specified channel number
  * @param config     Class used to document configuration
  */
  void configEventSelections(XAie_DevInst* aieDevInst, const tile_type& tile,
                             const XAie_LocType loc, const module_type type,
                             const std::string metricSet, const uint8_t channel0,
                             const uint8_t channel1, aie_cfg_base& config);

  /**
  * @brief Configure edge detection events
  * @param aieDevInst AIE device instance
  * @param tile       Tile metadata
  * @param type       Module/tile type
  * @param metricSet  Name of requested metric set
  * @param event      Requested event ID
  * @param channel    Channel number to use for edge events
  */
  void configEdgeEvents(XAie_DevInst* aieDevInst, const tile_type& tile,
                        const module_type type, const std::string metricSet, 
                        const XAie_Events event, const uint8_t channel = 0);

  /**
   * @brief Build 2-channel broadcast network for specified tile range
   * @param aieDevInst     AIE device
   * @param metadata       Trace Metadata
   * @param broadcastId1   Broadcast channel 1
   * @param broadcastId2   Broadcast channel 2
   * @param event          Event to trigger broadcast network
   * @param startCol       Start column of the partition
   * @param numCols        Num of columns in the partition
   * @param numRows        Num of Rows
   */
  void build2ChannelBroadcastNetwork(XAie_DevInst* aieDevInst, std::shared_ptr<AieTraceMetadata> metadata,
                                    uint8_t broadcastId1, uint8_t broadcastId2, XAie_Events event,
                                    uint8_t startCol, uint8_t numCols, uint8_t numRows);

  /**
   * @brief Reset 2-channel broadcast network for specified tile range
   * @param aieDevInst     AIE device
   * @param metadata       Trace Metadata
   * @param broadcastId1   Broadcast channel 1
   * @param broadcastId2   Broadcast channel 2
   * @param startCol       Start column of the partition
   * @param numCols        Num of columns in the partition
   * @param numRows        Num of Rows
   */
  void reset2ChannelBroadcastNetwork(XAie_DevInst* aieDevInst, std::shared_ptr<AieTraceMetadata> metadata,
                                    uint8_t broadcastId1, uint8_t broadcastId2, uint8_t startCol,
                                    uint8_t numCols, uint8_t numRows);
  
}  // namespace xdp::aie::trace

#endif
