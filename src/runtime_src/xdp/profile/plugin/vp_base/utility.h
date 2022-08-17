/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#ifndef UTILITY_DOT_H
#define UTILITY_DOT_H

#include <string>
#include "xdp/config.h"

// Functions that can be used in the database, the plugins, and the writers

namespace xdp {

  XDP_EXPORT std::string getCurrentDateTime() ;
  XDP_EXPORT const char* getToolVersion() ;
  XDP_EXPORT std::string getXRTVersion() ;
  XDP_EXPORT bool isEdge() ;

  enum Flow {
    SW_EMU  = 0,
    HW_EMU  = 1,
    HW      = 2,
    UNKNOWN = 3
  };

  namespace uint_constants {
    constexpr uint64_t one_thousand = 1e3;
    constexpr uint64_t one_million  = 1e6;
    constexpr uint64_t one_billion  = 1e9;
    constexpr uint64_t one_kb = 1024;
    constexpr uint64_t one_mb = 1024 * 1024;
    constexpr uint64_t one_gb = 1024 * 1024 * 1024;
  }

  XDP_EXPORT Flow getFlowMode() ;

} // end namespace xdp

#endif
