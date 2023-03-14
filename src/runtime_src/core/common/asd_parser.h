// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_ASD_PARSER_H_
#define _XCL_ASD_PARSER_H_

#include "core/common/device.h"

#include <boost/property_tree/ptree.hpp>
#include <cstdint>
#include <string>
#include <vector>

// asd stands for aie status dump
// This file has structures defined to parse aie status dump of all the tiles
namespace asd_parser {

// aie_tiles_info struct is maintained in both firmware and userspace code
// as there is no common code base b/w both, below versions are used for
// handshaking mechanism. Update these whenever aie_tiles_info changes.

inline constexpr uint16_t aie_tiles_info_version_major = 1;
inline constexpr uint16_t aie_tiles_info_version_minor = 1;

// struct aie_tiles_info - Device specific Aie tiles information
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
};
static_assert(sizeof(struct aie_tiles_info) == 44, "aie_tiles_info structure no longer is 44 bytes in size");
 
// AIE status structures are maintained by AIE team and they use preprocessor
// macros for different aie architectures, but we get all tiles information on
// runtime and we are using binary parser for parsing the data.
// Below versions are used for handshaking with aie driver
// Update this whenever we change any of the below structures

inline constexpr uint16_t aie_status_version_major = 1;
inline constexpr uint16_t aie_status_version_minor = 1;

enum class aie_tile_type { core, shim, mem };

// Data structure to capture the dma status
struct aie_dma_status
{
  uint32_t s2mm_status;
  uint32_t mm2s_status;
};

// Data structure for dma status internals
struct aie_dma_int
{
  std::vector<std::string> channel_status;
  std::string queue_status;
  uint32_t queue_size;
  uint32_t current_bd;
};

// Data structure to capture aie tiles status
class aie_tiles_status;

// Data structure to capture the core tile status
struct aie_core_tile_status
{
  std::vector<aie_dma_status> dma;
  std::vector<uint32_t> core_mode_events;
  std::vector<uint32_t> mem_mode_events;
  uint32_t core_status;
  uint32_t program_counter;
  uint32_t stack_ptr;
  uint32_t link_reg;
  std::vector<uint8_t> lock_value;

  // Get the size of this structure
  uint64_t
  size();

  // Get the size of this structure using aie tiles metadata
  static uint64_t
  size(const aie_tiles_info& info);

  // Retrieve corresponding tile type enum value
  static inline aie_tile_type
  type()
  {
    return aie_tile_type::core;
  }

  // Convert Raw buffer data received from driver to core tile status
  // size of buffer and offsets info for various data fields is obtained
  // from tiles metadata(aie_tiles_info)
  static void
  parse_buf(const std::vector<char>& buf, const aie_tiles_info& info, std::vector<aie_tiles_status>& aie_status);

  // Format the parsed buffer into ptree to be used by tools for reporting
  static boost::property_tree::ptree
  format_status(const std::vector<aie_tiles_status>& aie_status,
                uint32_t cols,
                const aie_tiles_info& tiles_info,
                uint32_t cols_filled);
};
  
// Data structure to capture the mem tile status
struct aie_mem_tile_status
{
  std::vector<aie_dma_status> dma;
  std::vector<uint32_t> events;
  std::vector<uint8_t> lock_value;

  // Get the size of this structure
  uint64_t
  size();

  // Get the size of this structure using aie tiles metadata
  static uint64_t
  size(const aie_tiles_info& info);

  // Retrieve corresponding tile type enum value
  static inline aie_tile_type
  type() 
  {
    return aie_tile_type::mem;
  }

  // Convert Raw buffer data received from driver to core tile status
  // size of buffer and offsets info for various data fields is obtained
  // from tiles metadata(aie_tiles_info)
  static void
  parse_buf(const std::vector<char>& buf, const aie_tiles_info& info, std::vector<aie_tiles_status>& aie_status);

  // Format the parsed buffer into ptree to be used by tools for reporting
  static boost::property_tree::ptree
  format_status(const std::vector<aie_tiles_status>& aie_status,
                uint32_t cols,
                const aie_tiles_info& tiles_info,
                uint32_t cols_filled);
};
  
// Data structure to capture the shim tile status
struct aie_shim_tile_status
{
  std::vector<aie_dma_status> dma;
  std::vector<uint32_t> events;
  std::vector<uint8_t> lock_value;

  // Get the size of this structure
  uint64_t
  size();

  // Get the size of this structure using aie tiles metadata
  static uint64_t
  size(const aie_tiles_info& info);

   // Retrieve corresponding tile type enum value
  static inline aie_tile_type
  type()
  {
    return aie_tile_type::shim;
  }

  // Convert Raw buffer data received from driver to core tile status
  // size of buffer and offsets info for various data fields is obtained
  // from tiles metadata(aie_tiles_info)
  static void
  parse_buf(const std::vector<char>& buf, const aie_tiles_info& info, std::vector<aie_tiles_status>& aie_status);

  // Format the parsed buffer into ptree to be used by tools for reporting
  static boost::property_tree::ptree
  format_status(const std::vector<aie_tiles_status>& aie_status,
                uint32_t cols,
                const aie_tiles_info& tiles_info,
                uint32_t cols_filled);
};

class aie_tiles_status
{
  public:
  std::vector<aie_core_tile_status> core_tiles;
  std::vector<aie_mem_tile_status> mem_tiles;
  std::vector<aie_shim_tile_status> shim_tiles;
  
  aie_tiles_status(const aie_tiles_info& stats)
  {
    core_tiles.resize(stats.core_rows);
    mem_tiles.resize(stats.mem_rows);
    shim_tiles.resize(stats.shim_rows);
     
    for (auto& core : core_tiles) {
      core.dma.resize(stats.core_dma_channels);
      core.core_mode_events.resize(stats.core_events);
      core.mem_mode_events.resize(stats.core_events);
      core.lock_value.resize(stats.core_locks);
    }
      
    for (auto& shim : shim_tiles) {
      shim.dma.resize(stats.shim_dma_channels);
      shim.events.resize(stats.shim_events);
      shim.lock_value.resize(stats.shim_locks);
    }
      
    for (auto& mem : mem_tiles) {
      mem.dma.resize(stats.mem_dma_channels);
      mem.events.resize(stats.mem_events);
      mem.lock_value.resize(stats.mem_locks);
    }
  }
};

// The following enum represents bits in aie core tile status  
enum class core_status : uint32_t
{
  xaie_core_status_enable_bit = 0U,
  xaie_core_status_reset_bit,
  xaie_core_status_mem_stall_s_bit,
  xaie_core_status_mem_stall_w_bit,
  xaie_core_status_mem_stall_n_bit,
  xaie_core_status_mem_stall_e_bit,
  xaie_core_status_lock_stall_s_bit,
  xaie_core_status_lock_stall_w_bit,
  xaie_core_status_lock_stall_n_bit,
  xaie_core_status_lock_stall_e_bit,
  xaie_core_status_stream_stall_ss0_bit,
  xaie_core_status_stream_stall_ms0_bit = 12U,
  xaie_core_status_cascade_stall_scd_bit = 14U,
  xaie_core_status_cascade_stall_mcd_bit,
  xaie_core_status_debug_halt_bit,
  xaie_core_status_ecc_error_stall_bit,
  xaie_core_status_ecc_scrubbing_stall_bit,
  xaie_core_status_error_halt_bit,
  xaie_core_status_done_bit,
  xaie_core_status_processor_bus_stall_bit,
  xaie_core_status_max_bit
};

// The following enum represents bits in aie tiles dma s2mm status  
enum class dma_s2mm_status : uint32_t
{
  xaie_dma_status_s2mm_status = 0U,
  xaie_dma_status_s2mm_stalled_lock_ack = 2U,
  xaie_dma_status_s2mm_stalled_lock_rel,
  xaie_dma_status_s2mm_stalled_stream_starvation,
  xaie_dma_status_s2mm_stalled_tct_or_count_fifo_full,
  xaie_dma_status_s2mm_error_lock_access_to_unavail = 8U, // Specific only to MEM Tile
  xaie_dma_status_s2mm_error_dm_access_to_unavail,       // specific only to MEM tile
  xaie_dma_status_s2mm_error_bd_unavail = 10U,
  xaie_dma_status_s2mm_error_bd_invalid,
  xaie_dma_status_s2mm_error_fot_length,
  xaie_dma_status_s2mm_error_fot_bds_per_task,
  xaie_dma_status_s2mm_axi_mm_decode_error = 16U,
  xaie_dma_status_s2mm_axi_mm_slave_error  = 17U,
  xaie_dma_status_s2mm_task_queue_overflow = 18U,
  xaie_dma_status_s2mm_channel_running,
  xaie_dma_status_s2mm_task_queue_size,
  xaie_dma_status_s2mm_current_bd = 24U,
  xaie_dma_status_s2mm_max
};

// The following enum represents bits in aie tiles dma mm2s status  
enum class dma_mm2s_status : uint32_t
{
  xaie_dma_status_mm2s_status = 0U,
  xaie_dma_status_mm2s_stalled_lock_ack = 2U,
  xaie_dma_status_mm2s_stalled_lock_rel,
  xaie_dma_status_mm2s_stalled_stream_backpressure,
  xaie_dma_status_mm2s_stalled_tct,
  xaie_dma_status_mm2s_error_lock_access_to_unavail = 8U, // Specific only to MEM Tile
  xaie_dma_status_mm2s_error_dm_access_to_unavail,        // specific only to MEM tile
  xaie_dma_status_mm2s_error_bd_unavail,
  xaie_dma_status_mm2s_error_bd_invalid = 11U,
  xaie_dma_status_mm2s_axi_mm_decode_error = 16U,
  xaie_dma_status_mm2s_axi_mm_slave_error  = 17U,
  xaie_dma_status_mm2s_task_queue_overflow = 18U,
  xaie_dma_status_mm2s_channel_running,
  xaie_dma_status_mm2s_task_queue_size,
  xaie_dma_status_mm2s_current_bd = 24U,
  xaie_dma_status_mm2s_max
};

template <typename tile_type>
std::vector<aie_tiles_status>
parse_data_from_buf(const std::vector<char>& buf, const aie_tiles_info& info, uint32_t cols_filled)
{
  std::vector<aie_tiles_status> aie_status;
  uint16_t cols_count = 0;

  while (cols_filled) {
    if (cols_filled & 0x1)
      ++cols_count;

    cols_filled = cols_filled >> 1;
  }

  aie_status.reserve(cols_count);

  for (uint32_t i = 0; i < cols_count; i++) {
    aie_status.emplace_back(info);
  }

  tile_type::parse_buf(buf, info, aie_status);
  return aie_status;
}

template <typename tile_type>
boost::property_tree::ptree
format_aie_info(const std::vector<aie_tiles_status>& aie_status,
                uint32_t cols,
                const aie_tiles_info& tiles_info,
                uint32_t cols_filled)
{
  return tile_type::format_status(aie_status, cols, tiles_info, cols_filled);
}

boost::property_tree::ptree
get_formated_tiles_info(const xrt_core::device* device, aie_tile_type tile_type, aie_tiles_info& info,
                        uint32_t& cols_filled);

boost::property_tree::ptree
get_formated_tiles_info(const xrt_core::device* device, aie_tile_type tile_type);
} // asd_parser

#endif

