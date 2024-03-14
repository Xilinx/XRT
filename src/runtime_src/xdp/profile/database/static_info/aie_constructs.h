/**
 * Copyright (C) 2021 Xilinx, Inc
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

#ifndef AIE_CONSTRUCTS_DOT_H
#define AIE_CONSTRUCTS_DOT_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "xdp/profile/device/tracedefs.h"

namespace xdp::aie {
  struct aiecompiler_options
  {
    bool broadcast_enable_core;
    bool graph_iterator_event;
    std::string event_trace;
  };

  struct driver_config
  {
    uint8_t hw_gen;
    uint64_t base_address;
    uint8_t column_shift;
    uint8_t row_shift;
    uint8_t num_rows;
    uint8_t num_columns;
    uint8_t shim_row;
    uint8_t mem_row_start;
    uint8_t mem_num_rows;
    uint8_t aie_tile_row_start;
    uint8_t aie_tile_num_rows;
  };
}

namespace xdp {


enum class module_type {
    core = 0,
    dma,
    shim,
    mem_tile,
    num_types
  };

  struct tile_type
  { 
    uint8_t  row;
    uint8_t  col;
    uint8_t  subtype;
    uint8_t  stream_id;
    uint8_t  is_master;
    uint64_t itr_mem_addr;
    bool     active_core;
    bool     active_memory;
    bool     is_trigger;
    
    bool operator==(const tile_type &tile) const {
      return (col == tile.col) && (row == tile.row);
    }
    bool operator<(const tile_type &tile) const {
      return (col < tile.col) || ((col == tile.col) && (row < tile.row));
    }
  };

  struct io_config
  { 
    // Object id
    int id;
    // Variable name
    std::string name;
    // Loginal name
    std::string logicalName;
    // Column where I/O is mapped
    uint8_t shimColumn;
    // slave or master - 0:slave, 1:master
    uint8_t slaveOrMaster;
    // Shim stream switch port id
    uint8_t streamId;
    // Channel number
    uint8_t channelNum;
    // Burst length
    uint8_t burstLength;
    // I/O type - 0:PLIO, 1:GMIO
    uint8_t type;
  };  

  /*
   * Represents AIE counter configuration for a single counter
   * Used to keep track of runtime configuration in aie profile and trace.
   */
  struct AIECounter
  {
    uint32_t id;
    uint8_t column;
    uint8_t row;
    uint8_t counterNumber;
    uint8_t resetEvent;
    uint16_t startEvent;
    uint16_t endEvent;
    uint32_t payload;
    double clockFreqMhz;
    std::string module;
    std::string name;

    AIECounter(uint32_t i, uint8_t col, uint8_t r, uint8_t num, 
               uint16_t start, uint16_t end, uint8_t reset,
               uint32_t load, double freq, const std::string& mod, 
               const std::string& aieName)
      : id(i)
      , column(col)
      , row(r)
      , counterNumber(num)
      , resetEvent(reset)
      , startEvent(start)
      , endEvent(end)
      , payload(load)
      , clockFreqMhz(freq)
      , module(mod)
      , name(aieName)
    {}
  };

  struct TraceGMIO
  {
    uint32_t id;
    uint8_t shimColumn;
    uint8_t channelNumber;
    uint8_t streamId;
    uint8_t burstLength;

    TraceGMIO(uint32_t i, uint8_t col, uint8_t num, 
              uint8_t stream, uint8_t len)
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
      uint32_t start_event = EVENT_CORE_ACTIVE;
      uint32_t stop_event = EVENT_CORE_DISABLED;
      uint32_t traced_events[NUM_TRACE_EVENTS] = {};
      std::map<uint32_t, uint32_t> group_event_config = {};
      uint32_t combo_event_input[NUM_COMBO_EVENT_INPUT] = {};
      uint32_t combo_event_control[NUM_COMBO_EVENT_CONTROL] = {};

      uint32_t broadcast_mask_south = BROADCAST_MASK_DEFAULT;
      uint32_t broadcast_mask_north = BROADCAST_MASK_DEFAULT;
      uint32_t broadcast_mask_west = BROADCAST_MASK_DEFAULT;
      uint32_t broadcast_mask_east = BROADCAST_MASK_DEFAULT;
      uint32_t internal_events_broadcast[NUM_BROADCAST_EVENTS] = {};
      
      bool port_trace_is_master[NUM_SWITCH_MONITOR_PORTS];
      int8_t port_trace_ids[NUM_SWITCH_MONITOR_PORTS];
      int8_t s2mm_channels[NUM_CHANNEL_SELECTS] = {-1, -1};
      int8_t mm2s_channels[NUM_CHANNEL_SELECTS] = {-1, -1};
      std::vector<aie_cfg_counter> pc;

      aie_cfg_base(uint32_t count) : pc(count) {
        for (uint32_t i=0; i < NUM_SWITCH_MONITOR_PORTS; ++i) {
          port_trace_is_master[i] = false;
          port_trace_ids[i] = -1;
        }
      };
  };

  /*
   * Core Module has 4 Performance counters
   * Group events 2,15,22,32,46,47,73,106,123 are defined in AIE architecture spec.
   * Core trace uses PC packets so we set that as default.
   */
  class aie_cfg_core : public aie_cfg_base
  {
  public:
    uint32_t trace_mode = 1;
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
   * Group events exist but don't need to be defined.
   * Memory trace uses time packets.
   */
  class aie_cfg_memory : public aie_cfg_base
  {
  public:
    aie_cfg_memory() : aie_cfg_base(2) {};
  };

  /*
   * Memory Tiles have 4 Performance counters.
   * Group events exist but don't need to be defined.
   * Memory tile trace uses time packets.
   */
  class aie_cfg_memory_tile : public aie_cfg_base
  {
  public:
    aie_cfg_memory_tile() : aie_cfg_base(4) {};
  };

  /*
   * Interface Tiles have 2 Performance counters.
   * Group events exist but don't need to be defined.
   * Interface tile trace uses time packets.
   */
  class aie_cfg_interface_tile : public aie_cfg_base
  {
  public:
    aie_cfg_interface_tile() : aie_cfg_base(2) {};
  };

  /*
   * Abstracted AIE tile configuration for trace
   */
  class aie_cfg_tile
  {
  public:
    bool active_core = true;
    bool active_memory = true;
    uint32_t column;
    uint32_t row;
    module_type type;
    std::string trace_metric_set;
    aie_cfg_core core_trace_config;
    aie_cfg_memory memory_trace_config;
    aie_cfg_memory_tile memory_tile_trace_config;
    aie_cfg_interface_tile interface_tile_trace_config;
    aie_cfg_tile(uint32_t c, uint32_t r, module_type t) : column(c), row(r), type(t) {}
  };

  // Used by client profiling/debug
  typedef struct {
    uint64_t perf_address;
  } profile_data_t;

  typedef struct {
    uint32_t count;
    profile_data_t profile_data[1];
  } aie_profile_op_t;

} // end namespace xdp

#endif
