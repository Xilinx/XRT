/**
 * Copyright (C) 2021 Xilinx, Inc
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

#ifndef AIE_CONSTRUCTS_DOT_H
#define AIE_CONSTRUCTS_DOT_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace xdp {

  /*
   * Represents AIE counter configuration for a single counter
   * Used to keep track of runtime configuration in aie profile and trace.
   */
  struct AIECounter
  {
    uint32_t id;
    uint16_t column;
    uint16_t row;
    uint8_t counterNumber;
    uint8_t resetEvent;
    uint16_t startEvent;
    uint16_t endEvent;
    double clockFreqMhz;
    std::string module;
    std::string name;

    AIECounter(uint32_t i, uint16_t col, uint16_t r, uint8_t num, 
               uint16_t start, uint16_t end, uint8_t reset,
               double freq, std::string mod, std::string aieName)
      : id(i)
      , column(col)
      , row(r)
      , counterNumber(num)
      , resetEvent(reset)
      , startEvent(start)
      , endEvent(end)
      , clockFreqMhz(freq)
      , module(mod)
      , name(aieName)
    {}
  };

  struct TraceGMIO
  {
    uint32_t id;
    uint16_t shimColumn;
    uint16_t channelNumber;
    uint16_t streamId;
    uint16_t burstLength;

    TraceGMIO(uint32_t i, uint16_t col, uint16_t num, 
              uint16_t stream, uint16_t len)
      : id(i)
      , shimColumn(col)
      , channelNumber(num)
      , streamId(stream)
      , burstLength(len)
    {}
  };

  struct NoCNode
  {
    // The index as it appears in the debug_ip_layout.  Maybe unused.
    uint64_t index ;
    std::string name ;
    uint8_t readTrafficClass ;
    uint8_t writeTrafficClass ;

    NoCNode(uint64_t i, const std::string& n, uint8_t r, uint8_t w)
      : index(i)
      , name(n)
      , readTrafficClass(r)
      , writeTrafficClass(w)
    {
    }
  } ;

  /*
   * AIE Config Writer Classes
   * Following classes act as metadata storage and are filled during aie
   * trace configuration. Since resource allocation happens at runtime,
   * trace parsers need this data in form of aie_event_trace_config json.
   */

  // Generic AIE Performance Counter
  class aie_cfg_counter
  {
    public:
      uint32_t start_event = 0;
      uint32_t stop_event = 0;
      uint32_t reset_event = 0;
      uint32_t event_value = 0;
      uint32_t counter_value = 0;
  };

  /*
   * Information common to core and memory modules within an aie tile
   * Default event and mask values are derived from AIE architecture spec.
   * 16 broadcast channels with default state being blocked.
   * Broadcast metadata isn't used for trace processing and exists for consistency.
   * 28,29 define core enable, disable events.
   */
  class aie_cfg_base
  {
    public:
      uint32_t packet_type = 0;
      uint32_t packet_id = 0;
      uint32_t start_event = 28;
      uint32_t stop_event = 29;
      uint32_t traced_events[8] = {0};
      std::map<uint32_t, uint32_t> group_event_config = {};
      uint32_t combo_event_input[4] = {0};
      uint32_t combo_event_control[3] = {0};
      uint32_t broadcast_mask_south = 65535;
      uint32_t broadcast_mask_north = 65535;
      uint32_t broadcast_mask_west = 65535;
      uint32_t broadcast_mask_east = 65535;
      uint32_t internal_events_broadcast[16] = {0};
      std::vector<aie_cfg_counter> pc;

      aie_cfg_base(uint32_t count) : pc(count) {};
  };

  /*
   * Core Module has 4 Performance counters
   * Group events 2,15,22,32,46,47,73,106,123 are defined in AIE architecture spec.
   * Core trace uses pc trace mode so we just set that as default.
   * "null" is a dummy string as port trace doesn't exist today.
   */
  class aie_cfg_core : public aie_cfg_base
  {
  public:
    uint32_t trace_mode = 1;
    std::string port_trace = "null";
    aie_cfg_core() : aie_cfg_base(4)
    {
      group_event_config = {
        {2 ,  0},
        {15,  0},
        {22,  0},
        {32,  0},
        {46,  0},
        {47,  0},
        {73,  0},
        {106, 0},
        {123, 0}
      };
    };
  };

  /*
   * Memory Module has 2 Performance counters.
   * Group events exist for memory module but don't need to be defined.
   * Memory trace uses time trace mode.
   */
  class aie_cfg_memory : public aie_cfg_base
  {
  public:
    aie_cfg_memory() : aie_cfg_base(2) {};
  };

  /*
   * Abstracted AIE tile configuration for trace
   */
  class aie_cfg_tile
  {
  public:
    uint32_t column;
    uint32_t row;
    aie_cfg_core core_trace_config;
    aie_cfg_memory memory_trace_config;
    aie_cfg_tile(uint32_t c, uint32_t r) : column(c), row(r) {}
  };

} // end namespace xdp

#endif
