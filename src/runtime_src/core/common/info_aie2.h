// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef COMMON_INFO_AIE2_H_
#define COMMON_INFO_AIE2_H_

#include "core/common/device.h"

#include <boost/property_tree/ptree.hpp>
#include <cstdint>
#include <string>
#include <vector>

// asd stands for aie status dump
// This file has structures defined to parse aie status dump of all the tiles
namespace aie2 {

enum class aie_tile_type { core, shim, mem };

// struct aie_tiles_info - Device specific AIE tiles information
struct aie_tiles_info
{
  uint32_t     col_size;
  uint16_t     major;
  uint16_t     minor;
   
  uint16_t     cols;
  uint16_t     rows;
    
  uint16_t     core_rows;
  uint16_t     mem_rows;
  uint16_t     shim_rows;
    
  uint16_t     core_row_start;
  uint16_t     mem_row_start;
  uint16_t     shim_row_start;
    
  uint16_t     core_dma_channels;
  uint16_t     mem_dma_channels;
  uint16_t     shim_dma_channels;
   
  uint16_t     core_locks;
  uint16_t     mem_locks;
  uint16_t     shim_locks;
   
  uint16_t     core_events;
  uint16_t     mem_events;
  uint16_t     shim_events;

  uint16_t     padding;

  uint16_t get_tile_count(const aie_tile_type tile_type) const
  {
    switch (tile_type) {
      case aie_tile_type::core:
        return core_rows;
      case aie_tile_type::shim:
        return shim_rows;
      case aie_tile_type::mem:
        return mem_rows;
    }
    throw std::runtime_error("Unknown tile type in formatting AIE tiles status info");
  }

  uint16_t get_tile_start(const aie_tile_type tile_type) const
  {
    switch (tile_type) {
      case aie_tile_type::core:
        return core_row_start;
      case aie_tile_type::shim:
        return shim_row_start;
      case aie_tile_type::mem:
        return mem_row_start;
    }
    throw std::runtime_error("Unknown tile type in formatting AIE tiles status info");
  }
};
static_assert(sizeof(struct aie_tiles_info) == 44, "aie_tiles_info structure no longer is 44 bytes in size");

XRT_CORE_COMMON_EXPORT
boost::property_tree::ptree
get_formated_tiles_info(const xrt_core::device* device, aie_tile_type tile_type);

} // aie2

#endif
