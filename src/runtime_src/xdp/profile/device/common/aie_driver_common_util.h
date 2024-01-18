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

#ifndef AIE_DRIVER_COMMON_UTIL_DOT_H
#define AIE_DRIVER_COMMON_UTIL_DOT_H

#include <cstdint>
#include "xdp/profile/database/static_info/aie_constructs.h"

namespace xdp::aie {
  /**
   * @brief   Get metric sets for core modules
   * @details Depending on hardware generation, these sets can be supplemented 
   *          with counter events as those are dependent on counter #.
   * @return  Map of metric set names with vectors of event IDs
   */
  // std::map<std::string, std::vector<XAie_Events>> getCoreEventSets();


}  // namespace xdp::aie

#endif