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

#include <stdint.h>

#include "xdp/profile/device/tracedefs.h"

namespace xdp {
  namespace built_in {

    enum class MetricSet : uint8_t { FUNCTIONS = 0, PARTIAL_STALLS = 1, ALL_STALLS = 2, ALL = 3 };

    enum class MemTileMetricSet : uint8_t {
      INPUT_CHANNELS = 0,
      INPUT_CHANNELS_STALLS = 1,
      OUTPUT_CHANNELS = 2,
      OUTPUT_CHANNELS_STALLS = 3
    };

    enum class CounterScheme : uint8_t { ES1 = 0, ES2 = 1, AIE2 = 2 };

    enum class Messages : uint8_t {
      NO_CORE_MODULE_PCS = 0,
      NO_CORE_MODULE_TRACE_SLOTS = 1,
      NO_CORE_MODULE_BROADCAST_CHANNELS = 2,
      NO_MEM_MODULE_PCS = 3,
      NO_MEM_MODULE_TRACE_SLOTS = 4,
      NO_RESOURCES = 5,
      COUNTERS_NOT_RESERVED = 6,
      CORE_MODULE_TRACE_NOT_RESERVED = 7,
      CORE_TRACE_EVENTS_RESERVED = 8,
      MEMORY_MODULE_TRACE_NOT_RESERVED = 9,
      MEMORY_TRACE_EVENTS_RESERVED = 10,
      ALL_TRACE_EVENTS_RESERVED = 11,
      ENABLE_TRACE_FLUSH = 12,
    };

    struct MessagePacket {
      uint8_t messageCode;
      uint32_t params[4] = {};  // Tile information to display to user based on message type
    };

    struct MessageConfiguration {
      static constexpr auto MAX_NUM_MESSAGES = 800;
      uint32_t numMessages;
      MessagePacket packets[MAX_NUM_MESSAGES];
    };

    struct TraceTileType {
      uint16_t col;
      uint16_t row;
      uint8_t metricSet;
      uint8_t channel0 = -1;  // Only relevant for MemTiles
      uint8_t channel1 = -1;  // Only relevant for MemTiles
    };

    // This struct is used for input for the PS kernel.  It contains all of
    // the information gathered from the user controls in the xrt.ini file
    // and the information we can infer from the debug ip layout file.
    // The struct should be constructed and then transferred via a buffer object.
    //
    // Since this is transferred from host to device, it should have
    // a C-Style interface.
    struct TraceInputConfiguration {
      static constexpr auto NUM_CORE_TRACE_EVENTS = 8;
      static constexpr auto NUM_MEMORY_TRACE_EVENTS = 8;
      static constexpr auto NUM_MEM_TILE_TRACE_EVENTS = 8;

      uint32_t delayCycles;
      uint32_t iterationCount;
      uint16_t numTiles;
      uint8_t counterScheme;
      uint8_t hwGen;
      uint8_t offset;

      bool useGraphIterator;
      bool useDelay;
      bool useUserControl;
      bool useOneDelayCounter;
      TraceTileType tiles[1];  // Flexible array member
    };

    struct PCData {
     public:
      uint32_t start_event = 0;
      uint32_t stop_event = 0;
      uint32_t reset_event = 0;
      uint32_t event_value = 0;
      uint32_t counter_value = 0;
    };

    struct TileTraceData {
     public:
      uint32_t packet_type = 0;
      uint32_t start_event = EVENT_CORE_ACTIVE;
      uint32_t stop_event = EVENT_CORE_DISABLED;
      uint32_t traced_events[NUM_TRACE_EVENTS] = {};
      uint32_t internal_events_broadcast[NUM_BROADCAST_EVENTS] = {};
      uint32_t broadcast_mask_west = BROADCAST_MASK_DEFAULT;
      uint32_t broadcast_mask_east = BROADCAST_MASK_DEFAULT;
      PCData pc[NUM_TRACE_PCS];
    };

    struct MemTileTraceData {
      uint8_t port_trace_ids[NUM_SWITCH_MONITOR_PORTS] = {};
      bool port_trace_is_master[NUM_SWITCH_MONITOR_PORTS];
      uint8_t s2mm_channels[NUM_CHANNEL_SELECTS] = {};
      uint8_t mm2s_channels[NUM_CHANNEL_SELECTS] = {};

      uint32_t packet_type = 0;
      uint32_t start_event = EVENT_CORE_ACTIVE;
      uint32_t stop_event = EVENT_CORE_DISABLED;
      uint32_t traced_events[NUM_TRACE_EVENTS] = {};
      uint32_t internal_events_broadcast[NUM_BROADCAST_EVENTS] = {};
      uint32_t broadcast_mask_west = BROADCAST_MASK_DEFAULT;
      uint32_t broadcast_mask_east = BROADCAST_MASK_DEFAULT;
      PCData pc[NUM_TRACE_PCS];
    };

    struct TileData {
     public:
      uint8_t type;
      uint8_t trace_metric_set;
      uint32_t column;
      uint32_t row;
      TileTraceData core_trace_config;
      TileTraceData memory_trace_config;
      MemTileTraceData memory_tile_trace_config;
      TileData(uint32_t c, uint32_t r) : column(c), row(r)
      {}
    };

    struct TraceOutputConfiguration {
     public:
      uint16_t numTiles;
      uint32_t numTileCoreTraceEvents[NUM_OUTPUT_TRACE_EVENTS] = {};
      uint32_t numTileMemoryTraceEvents[NUM_OUTPUT_TRACE_EVENTS] = {};
      uint32_t numTileMemTileTraceEvents[NUM_OUTPUT_TRACE_EVENTS] = {};
      TileData tiles[1];
    };

    struct GMIOBuffer {
      uint32_t shimColumn;  // From TraceGMIo
      uint32_t channelNumber;
      uint32_t burstLength;
      uint64_t physAddr;
    };

    struct GMIOConfiguration {
      uint64_t bufAllocSz;
      uint8_t numStreams;
      struct GMIOBuffer gmioData[1];  // Flexible Array Member
    };

  }  // end namespace built_in
}  // end namespace xdp

#endif
