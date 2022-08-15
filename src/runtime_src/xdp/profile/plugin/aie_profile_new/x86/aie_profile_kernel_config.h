/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

namespace xdp {
namespace built_in {

  enum class CoreMetrics : uint8_t 
  {
    EMPTY = 0,
    HEAT_MAP = 1,
    STALLS = 2,
    EXECUTION = 3,
    FLOATING_POINT = 4,
    WRITE_BANDWIDTHS = 5,
    READ_BANDWIDTHS = 6,
    AIE_TRACE = 7
    
  };

  enum class MemoryMetrics : uint8_t 
  {
    EMPTY = 0,
    CONFLICTS = 1,
    DMA_LOCKS = 2,
    DMA_STALLS_S2MM = 3,
    DMA_STALS_MM2S = 4,
    WRITE_BANDWIDTHS = 5,
    READ_BANDWIDTHS = 6
  };

  enum class InterfaceMetrics : uint8_t 
  {
    EMPTY = 0,
    INPUT_BANDWIDTHS = 1,
    OUTPUT_BANDWIDTHS = 2,
    INPUT_STALLS_IDLE = 3,
    OUTPUT_STALLS_IDLE = 4,
  };

  // This struct is used for input for the PS kernel.  It contains all of
  // the information gathered from the user controls in the xrt.ini file
  // and the information we can infer from the debug ip layout file.
  // The struct should be constructed and then transferred via a buffer object.
  //
  // Since this is transferred from host to device, it should have
  // a C-Style interface.
  struct InputConfiguration
  {
    static constexpr auto NUM_CORE_COUNTERS = 4;
    static constexpr auto NUM_MEMORY_COUNTERS = 2;
    static constexpr auto NUM_SHIM_COUNTERS = 2;
    constexpr int NUM_MODULES = 3;

    uint8_t metricSettings[NUM_MODULES];
   

    uint16_t tiles[1]; //flexible array member
  };

} // end namespace built_in
} // end namespace xdp

#endif
