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
#include "xaiefal/xaiefal.hpp"
#include "xdp/profile/database/static_info/aie_constructs.h"

namespace xdp::aie::trace {
  std::map<std::string, std::vector<XAie_Events>> getCoreEventSets();
  std::map<std::string, std::vector<XAie_Events>> getMemoryEventSets();
  std::map<std::string, std::vector<XAie_Events>> getMemoryTileEventSets();
  std::map<std::string, std::vector<XAie_Events>> getInterfaceTileEventSets(int hwGen);

  std::vector<XAie_Events> getCoreCounterStartEvents(int hwGen, std::string scheme);
  std::vector<XAie_Events> getCoreCounterEndEvents(int hwGen, std::string scheme);
  std::vector<uint32_t> getCoreCounterEventValues(int hwGen, std::string scheme);
  std::vector<XAie_Events> getMemoryCounterStartEvents(int hwGen, std::string scheme);
  std::vector<XAie_Events> getMemoryCounterEndEvents(int hwGen, std::string scheme);
  std::vector<uint32_t> getMemoryCounterEventValues(int hwGen, std::string scheme);

  bool isStreamSwitchPortEvent(const XAie_Events event);
  bool isPortRunningEvent(const XAie_Events event);
  uint8_t getPortNumberFromEvent(XAie_Events event);
  
  void printTileStats(xaiefal::XAieDev* aieDevice, const tile_type& tile);
  void printTraceEventStats(int m, int numTiles[]);
  void modifyEvents(module_type type, uint16_t subtype, const std::string metricSet,
                    uint8_t channel, std::vector<XAie_Events>& events);
}  // namespace xdp::aie::trace

#endif
