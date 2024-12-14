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

#ifndef AIE_PROFILE_CONFIG_DOT_H
#define AIE_PROFILE_CONFIG_DOT_H

#include <vector>

namespace xdp {
  namespace built_in {

    enum class CoreMetrics : uint8_t {
      HEAT_MAP = 0,
      STALLS = 1,
      EXECUTION = 2,
      FLOATING_POINT = 3,
      STREAM_PUT_GET = 4,
      WRITE_BANDWIDTHS = 5,
      READ_BANDWIDTHS = 6,
      AIE_TRACE = 7,
      EVENTS = 8
    };

    enum class MemoryMetrics : uint8_t {
      CONFLICTS = 0,
      DMA_LOCKS = 1,
      DMA_STALLS_S2MM = 2,
      DMA_STALLS_MM2S = 3,
      WRITE_BANDWIDTHS = 4,
      READ_BANDWIDTHS = 5
    };

    enum class InterfaceMetrics : uint8_t { INPUT_BANDWIDTHS = 0, OUTPUT_BANDWIDTHS = 1, PACKETS = 2 };

    enum class MemTileMetrics : uint8_t {
      INPUT_CHANNELS = 0,
      INPUT_CHANNELS_DETAILS = 1,
      OUTPUT_CHANNELS = 2,
      OUTPUT_CHANNELS_DETAILS = 3,
      MEMORY_STATS = 4,
      MEM_TRACE = 5
    };

    // This struct is used for input for the PS kernel.  It contains all of
    // the information gathered from the user controls in the xrt.ini file
    // and the information we can infer from the debug ip layout file.
    // The struct should be constructed and then transferred via a buffer object.
    //
    // Since this is transferred from host to device, it should have
    // a C-Style interface.

    struct ProfileTileType {
      uint16_t row;
      uint16_t col;
      std::vector<uint8_t> stream_ids;
      std::vector<uint8_t> is_master_vec;
      uint64_t itr_mem_addr;
      bool is_trigger;
      uint8_t metricSet;
      uint8_t tile_mod;
      int8_t channel0 = -1;
      int8_t channel1 = -1;
    };

    struct ProfileInputConfiguration {
      static constexpr auto NUM_CORE_COUNTERS = 4;
      static constexpr auto NUM_MEMORY_COUNTERS = 2;
      static constexpr auto NUM_SHIM_COUNTERS = 2;
      static constexpr auto NUM_MEM_TILE_COUNTERS = 4;

      uint16_t numTiles;
      uint16_t offset;

      // uint16_t numTiles[NUM_MODULES]; // Make unique variab
      ProfileTileType tiles[1];  // flexible array member
    };

    struct PSCounterInfo {
      uint8_t moduleName;
      uint16_t col;
      uint16_t row;
      uint16_t startEvent;
      uint16_t endEvent;
      uint32_t counterValue;
      uint32_t payload;
      uint8_t counterNum;  //.Counter number in the tile
      uint32_t counterId;  // Counter ID in list of all possible counters
      uint8_t resetEvent;
      uint64_t timerValue;
    };

    struct ProfileOutputConfiguration {
      uint32_t numCounters;
      PSCounterInfo counters[1];
    };

  }  // end namespace built_in
}  // end namespace xdp

#endif
