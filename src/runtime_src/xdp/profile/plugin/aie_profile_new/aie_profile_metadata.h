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

#ifndef AIE_PROFILE_METADATA_H
#define AIE_PROFILE_METADATA_H

#include "core/edge/common/aie_parser.h"

// #include "xaiefal/xaiefal.hpp"

// extern "C" {
// #include <xaiengine.h>
// #include <xaiengine/xaiegbl_params.h>
// }
namespace xdp {
  using tile_type = xrt_core::edge::aie::tile_type;
  using module_type = xrt_core::edge::aie::module_type;

  // enum class module_type {
  //     core = 0,
  //     dma,
  //     shim
  //   };

  //   struct tile_type
  //   { 
  //     uint16_t row;
  //     uint16_t col;
  //     uint16_t itr_mem_row;
  //     uint16_t itr_mem_col;
  //     uint64_t itr_mem_addr;
  //     bool     is_trigger;
      
  //     bool operator==(const tile_type &tile) const {
  //       return (col == tile.col) && (row == tile.row);
  //     }
  //     bool operator<(const tile_type &tile) const {
  //       return (col < tile.col) || ((col == tile.col) && (row < tile.row));
  //     }
  //   };

class AieProfileMetadata{

  public:
    AieProfileMetadata(uint64_t deviceID, void* handle);
    void getPollingInterval();
    
    uint64_t getDeviceID() {return deviceID;}
    void* getHandle() {return handle;}
    int16_t getChannelId(){return mChannelId;}
    uint32_t getPollingIntervalVal(){return mPollingInterval;}

    void setChannelId(uint16_t newChannelId){mChannelId = newChannelId;}

  private:
    int16_t mChannelId = -1;
    uint32_t mIndex = 0;
    uint32_t mPollingInterval;
    uint64_t deviceID;
    void* handle;
  };
}

#endif