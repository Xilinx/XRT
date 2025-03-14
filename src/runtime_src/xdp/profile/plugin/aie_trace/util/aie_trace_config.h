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

#ifndef AIE_TRACE_CONFIG_DOT_H
#define AIE_TRACE_CONFIG_DOT_H

#include <cstdint>
#include "xaiefal/xaiefal.hpp"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_metadata.h"

namespace xdp::aie::trace {
  /**
   * @brief Print out resource usage statistics for a given tile
   * @param aieDevice  AIE device
   * @param tile       Tile metadata
   */
  void printTileStats(xaiefal::XAieDev* aieDevice, const tile_type& tile);

  /**
   * @brief Configure stream switch monitor ports
   * @param aieDevInst AIE device instance
   * @param tile       Tile metadata
   * @param xaieTile   Tile instance in FAL/resource manager
   * @param loc        Location of tile
   * @param type       Module/tile type
   * @param metricSet  Name of requested metric set
   * @param channel0   First specified channel number
   * @param channel1   Second specified channel number
   * @param events     Vector of original events in metric set
   * @param config     Class used to document configuration
   * @return Vector of stream switch monitor ports used
   */
  std::vector<std::shared_ptr<xaiefal::XAieStreamPortSelect>>
  configStreamSwitchPorts(XAie_DevInst* aieDevInst, const tile_type& tile,
                          xaiefal::XAieTile& xaieTile, const XAie_LocType loc,
                          const module_type type, const std::string metricSet, 
                          const uint8_t channel0, const uint8_t channel1,
                          std::vector<XAie_Events>& events, aie_cfg_base& config);

  /**
   * @brief Configure combo events (AIE tiles only)
   * @param aieDevInst AIE device instance
   * @param xaieTile   Tile instance in FAL/resource manager
   * @param loc        Location of tile
   * @param mod        Module type (used by driver)
   * @param type       Module/tile type
   * @param metricSet  Name of requested metric set
   * @param config     Class used to document configuration
   * @return Vector of events to use for trace start and end
   */
  std::vector<XAie_Events>
  configComboEvents(XAie_DevInst* aieDevInst, xaiefal::XAieTile& xaieTile, 
                    const XAie_LocType loc, const XAie_ModuleType mod,
                    const module_type type, const std::string metricSet,
                    aie_cfg_base& config);

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
   * @param loc        Location of tile
   * @param type       Module/tile type
   * @param metricSet  Name of requested metric set
   * @param channel0   First specified channel number
   * @param channel1   Second specified channel number
   * @param config     Class used to document configuration
   */
  void configEventSelections(XAie_DevInst* aieDevInst, const XAie_LocType loc,
                             const module_type type, const std::string metricSet, 
                             const uint8_t channel0, const uint8_t channel1,
                             aie_cfg_base& config);

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
   * @brief Configure start of event trace using time delay
   * @param core       Core module in FAL/resource manager
   * @param delay      Requested delay (in AIE clock cycles)
   * @param startEvent Event ID to start trace
   * @return True if able to reserve and configure counter(s)
   */
  bool configStartDelay(xaiefal::XAieMod& core, uint64_t delay,
                        XAie_Events& startEvent);

  /**
   * @brief Configure start of event trace using graph iteration
   * @param core       Core module in FAL/resource manager
   * @param iteration  Requested graph iteration to start on
   * @param startEvent Event ID to start trace
   * @return True if able to reserve and configure counter
   */
  bool configStartIteration(xaiefal::XAieMod& core, uint32_t iteration,
                            XAie_Events& startEvent);
  
  /**
   * @brief Reset timers for specified tile range
   * @param aieDevInst     AIE device Instance
   * @param aieDevice      AIE device
   * @param metadata       Trace Metadata
   * @param startCol       Start column of the partition
   * @param numCols        Num of columns in the partition
   * @param numRows        Num of Rows
   */
  void timerSyncronization(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice,
                           std::shared_ptr<AieTraceMetadata> metadata, uint8_t startCol, 
                           uint8_t numCols, uint8_t numRows);
}  // namespace xdp::aie::trace

#endif
