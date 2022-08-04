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

#ifndef AIE_TRACE_CONFIG_DOT_H
#define AIE_TRACE_CONFIG_DOT_H

namespace xdp {
namespace built_in {

  enum class MetricSet : uint8_t 
  {
    FUNCTIONS = 0,
    PARTIAL_STALLS = 1,
    ALL_STALLS = 2,
    ALL = 3
  };

  enum class CounterScheme : uint8_t 
  {
    ES1 = 0,
    ES2 = 1
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

  struct PCData
  {
    public:
      uint32_t start_event = 0;
      uint32_t stop_event = 0;
      uint32_t reset_event = 0;
      uint32_t event_value = 0;
      uint32_t counter_value = 0;

  };

  struct TileTraceData
  {   
    public:
      uint32_t packet_type = 0;
      uint32_t start_event = 28; 
      uint32_t stop_event = 29; 
      uint32_t traced_events[8] = {0};
      uint32_t internal_events_broadcast[16] = {0};
      uint32_t broadcast_mask_west = 65535;
      uint32_t broadcast_mask_east = 65535;
      PCData pc[4];
  };  


  struct TileData
  {
    public:
      uint32_t column;
      uint32_t row;
      TileTraceData  core_trace_config;
      TileTraceData  memory_trace_config;
   
      TileData(uint32_t c, uint32_t r) : column(c), row(r) {}
  };

  struct OutputConfiguration
  {
    public:
      uint16_t numTiles;
      uint32_t numTileCoreTraceEvents[9] = {0};
      uint32_t numTileMemoryTraceEvents[9] = {0};
      TileData tiles[1]; 
  };


  struct GMIOBuffer
  {
    uint32_t shimColumn;      // From TraceGMIo
    uint32_t channelNumber;
    uint32_t burstLength;
	uint64_t physAddr;
  };

   
  struct GMIOConfiguration
  {
    uint64_t bufAllocSz;
    uint8_t numStreams;
    struct GMIOBuffer gmioData[1];
  };  


} // end namespace built_in
} // end namespace xdp

#endif
