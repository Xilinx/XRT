/**
 * Copyright (C) 2020-2022 Xilinx, Inc
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

namespace xdp {
namespace built_in {

  enum MetricSet : uint8_t { FUNCTIONS = 0,
                   PARTIAL_STALLS = 1,
                   ALL_STALLS = 2,
                   ALL = 3
                 };

  enum CounterScheme : uint8_t { ES1 = 0, ES2 = 1};

  // This struct is used for input for the PS kernel.  It contains all of
  // the information gathered from the user controls in the xrt.ini file
  // and the information we can infer from the debug ip layout file.
  // The struct should be constructed and then transferred via a buffer object.
  //
  // Since this is transferred from host to device, it should have
  // a C-Style interface.
  struct InputConfiguration
  {
    static constexpr auto NUM_CORE_TRACE_EVENTS = 8;
    static constexpr auto NUM_MEMORY_TRACE_EVENTS = 8;

    uint32_t delayCycles;
    uint16_t numTiles;
    uint8_t counterScheme;
    uint8_t metricSet; // functions, partial_stalls, all_stalls, etc. (enum above)
   
    bool useDelay;
    bool userControl;
    uint16_t tiles[1]; //flexible array member
  };

  // This struct is used as output from the PS kernel.  It should be zeroed out
  // and passed as a buffer object to and from the PS kernel.  The PS kernel
  // will fill in the different values.
  //
  // Since this is transferred from host to device, it should have
  // a C-Style interface.
  struct OutputValues
  {    
    bool success;
  };

} // end namespace built_in
} // end namespace xdp

#endif
