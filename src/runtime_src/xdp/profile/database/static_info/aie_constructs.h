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
      : id(i),
        column(col),
        row(r),
        counterNumber(num),
        resetEvent(reset),
        startEvent(start),
        endEvent(end),
        clockFreqMhz(freq),
        module(mod),
        name(aieName)
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
      : id(i),
        shimColumn(col),
        channelNumber(num),
        streamId(stream),
        burstLength(len)
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
   */
  class aie_cfg_counter
  {
    public:
      uint32_t start_event = 0;
      uint32_t stop_event = 0;
      uint32_t reset_event = 0;
      uint32_t event_value = 0;
      uint32_t counter_value = 0;
  };

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

  class aie_cfg_memory : public aie_cfg_base
  {
  public:
    aie_cfg_memory() : aie_cfg_base(2) {};
  };

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
