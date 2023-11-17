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

namespace xdp::aie::trace {
  // Configuration helper functions
  std::vector<std::shared_ptr<xaiefal::XAieStreamPortSelect>>
  configStreamSwitchPorts(XAie_DevInst* aieDevInst, const tile_type& tile,
                          xaiefal::XAieTile& xaieTile, const XAie_LocType loc,
                          const module_type type, const std::string metricSet, 
                          const uint8_t channel0, const uint8_t channel1,
                          std::vector<XAie_Events>& events);
  void configEventSelections(XAie_DevInst* aieDevInst, const XAie_LocType loc, 
                             const XAie_ModuleType mod, const module_type type, 
                             const std::string metricSet, const uint8_t channel0,
                             const uint8_t channel);
  void configEdgeEvents(XAie_DevInst* aieDevInst, const tile_type& tile,
                        const module_type type, const std::string metricSet, 
                        const XAie_Events event);
  bool configStartIteration(xaiefal::XAieMod& core, uint32_t iteration,
                            XAie_Events& startEvent);
  bool configStartDelay(xaiefal::XAieMod& core, uint64_t delay,
                        XAie_Events& startEvent);
}  // namespace xdp::aie::trace

#endif
