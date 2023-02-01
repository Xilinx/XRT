/*
 * Copyright (C) 2023 Xilinx, Inc
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef _XCL_ASD_PARSER_H_
#define _XCL_ASD_PARSER_H_

#include "xrt.h"

#include <boost/property_tree/ptree.hpp>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

// asd stands for aie status dump
namespace asd_parser {
  /*
   * aie_tiles_info struct is maintained in both firmware and userspace code
   * as there is no common code base b/w both, below versions are used for
   * handshaking mechanism. Update these whenever aie_tiles_info changes.
   */
  inline constexpr uint16_t AIE_TILES_INFO_VERSION_MAJOR = 1;
  inline constexpr uint16_t AIE_TILES_INFO_VERSION_MINOR = 1;

  /* struct aie_tiles_info - Device specific Aie tiles information */
  struct aie_tiles_info {
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

  /* 
   * AIE status structures are maintained by AIE team and they use preprocessor
   * macros for different aie architectures, but we get all tiles information on
   * runtime and we are using binary parser for parsing the data.
   * Below versions are used for handshaking with aie driver
   * Update this whenever we change any of the below structures
   */
  inline constexpr uint16_t AIE_STATUS_VERSION_MAJOR = 1;
  inline constexpr uint16_t AIE_STATUS_VERSION_MINOR = 1;

  enum class aie_tile_type { core, shim, mem };

  /* Data structure to capture the dma status */
  struct aie_dma_status {
    uint32_t s2mm_status;
    uint32_t mm2s_status;
  };

  /* Data structure for dma status internals*/
  struct aie_dma_int {
    std::vector<std::string> channel_status;
    std::string queue_status;
    uint32_t queue_size;
    uint32_t current_bd;
  };

  /* Data structure to capture column status */
  class aie_col_status;

  /* Data structure to capture the core tile status */
  struct aie_core_tile_status {
    std::vector<aie_dma_status> Dma;
    std::vector<uint32_t> Events;
    uint32_t CoreStatus;
    uint32_t ProgramCounter;
    uint32_t StackPtr;
    uint32_t LinkReg;
    std::vector<uint8_t> LockValue;

    uint64_t size();
    static uint64_t size(aie_tiles_info &info);
    static inline aie_tile_type type() { return aie_tile_type::core; }
    static void parse_buf(char *buf, aie_tiles_info &info, std::vector<std::unique_ptr<aie_col_status>> &aie_cols);
    static boost::property_tree::ptree
    format_status(std::vector<std::unique_ptr<aie_col_status>> &aie_cols,
                  uint32_t start_col,
                  uint32_t cols,
                  aie_tiles_info &tiles_info);
  };
  
  /* Data structure to capture the mem tile status */
  struct aie_mem_tile_status {
    std::vector<aie_dma_status> Dma;
    std::vector<uint32_t> Events;
    std::vector<uint8_t> LockValue;

    uint64_t size();
    static uint64_t size(aie_tiles_info &info);
    static inline aie_tile_type type() { return aie_tile_type::mem; }
    static void parse_buf(char *buf, aie_tiles_info &info, std::vector<std::unique_ptr<aie_col_status>> &aie_cols);
    static boost::property_tree::ptree
    format_status(std::vector<std::unique_ptr<aie_col_status>> &aie_cols,
                  uint32_t start_col,
                  uint32_t cols,
                  aie_tiles_info &tiles_info);
  };
  
  /* Data structure to capture the shim tile status */
  struct aie_shim_tile_status {
    std::vector<aie_dma_status> Dma;
    std::vector<uint32_t> Events;
    std::vector<uint8_t> LockValue;

    uint64_t size();
    static uint64_t size(aie_tiles_info &info);
    static inline aie_tile_type type() { return aie_tile_type::shim; }
    static void parse_buf(char *buf, aie_tiles_info &info, std::vector<std::unique_ptr<aie_col_status>> &aie_cols);
    static boost::property_tree::ptree
    format_status(std::vector<std::unique_ptr<aie_col_status>> &aie_cols,
                  uint32_t start_col,
                  uint32_t cols,
                  aie_tiles_info &tiles_info);
  };

  class aie_col_status {
    public:
    std::vector<aie_core_tile_status> CoreTile;
    std::vector<aie_mem_tile_status> MemTile;
    std::vector<aie_shim_tile_status> ShimTile;
    
    aie_col_status(aie_tiles_info &stats)
    {
      CoreTile.resize(stats.core_rows);
      MemTile.resize(stats.mem_rows);
      ShimTile.resize(stats.shim_rows);
      
      for (auto &core : CoreTile) {
        core.Dma.resize(stats.core_dma_channels);
        core.Events.resize(stats.core_events);
        core.LockValue.resize(stats.core_locks);
      }
      
      for (auto &shim : ShimTile) {
        shim.Dma.resize(stats.shim_dma_channels);
        shim.Events.resize(stats.shim_events);
        shim.LockValue.resize(stats.shim_locks);
      }
      
      for (auto &mem : MemTile) {
        mem.Dma.resize(stats.mem_dma_channels);
        mem.Events.resize(stats.mem_events);
        mem.LockValue.resize(stats.mem_locks);
      }
    }
  };
  
  enum class core_status : uint32_t
  {
    XAIE_CORE_STATUS_ENABLE_BIT = 0U,
    XAIE_CORE_STATUS_RESET_BIT,
    XAIE_CORE_STATUS_MEM_STALL_S_BIT,
    XAIE_CORE_STATUS_MEM_STALL_W_BIT,
    XAIE_CORE_STATUS_MEM_STALL_N_BIT,
    XAIE_CORE_STATUS_MEM_STALL_E_BIT,
    XAIE_CORE_STATUS_LOCK_STALL_S_BIT,
    XAIE_CORE_STATUS_LOCK_STALL_W_BIT,
    XAIE_CORE_STATUS_LOCK_STALL_N_BIT,
    XAIE_CORE_STATUS_LOCK_STALL_E_BIT,
    XAIE_CORE_STATUS_STREAM_STALL_SS0_BIT,
    XAIE_CORE_STATUS_STREAM_STALL_MS0_BIT = 12U,
    XAIE_CORE_STATUS_CASCADE_STALL_SCD_BIT = 14U,
    XAIE_CORE_STATUS_CASCADE_STALL_MCD_BIT,
    XAIE_CORE_STATUS_DEBUG_HALT_BIT,
    XAIE_CORE_STATUS_ECC_ERROR_STALL_BIT,
    XAIE_CORE_STATUS_ECC_SCRUBBING_STALL_BIT,
    XAIE_CORE_STATUS_ERROR_HALT_BIT,
    XAIE_CORE_STATUS_DONE_BIT,
    XAIE_CORE_STATUS_PROCESSOR_BUS_STALL_BIT,
    XAIE_CORE_STATUS_MAX_BIT
  };
  
  enum class dma_s2mm_status : uint32_t
  {
    XAIE_DMA_STATUS_S2MM_STATUS = 0U,
    XAIE_DMA_STATUS_S2MM_STALLED_LOCK_ACK = 2U,
    XAIE_DMA_STATUS_S2MM_STALLED_LOCK_REL,
    XAIE_DMA_STATUS_S2MM_STALLED_STREAM_STARVATION,
    XAIE_DMA_STATUS_S2MM_STALLED_TCT_OR_COUNT_FIFO_FULL,
    XAIE_DMA_STATUS_S2MM_ERROR_LOCK_ACCESS_TO_UNAVAIL = 8U, // Specific only to MEM Tile
    XAIE_DMA_STATUS_S2MM_ERROR_DM_ACCESS_TO_UNAVAIL,       // Specific only to MEM Tile
    XAIE_DMA_STATUS_S2MM_ERROR_BD_UNAVAIL = 10U,
    XAIE_DMA_STATUS_S2MM_ERROR_BD_INVALID,
    XAIE_DMA_STATUS_S2MM_ERROR_FOT_LENGTH,
    XAIE_DMA_STATUS_S2MM_ERROR_FOT_BDS_PER_TASK,
    XAIE_DMA_STATUS_S2MM_AXI_MM_DECODE_ERROR = 16U,
    XAIE_DMA_STATUS_S2MM_AXI_MM_SLAVE_ERROR  = 17U,
    XAIE_DMA_STATUS_S2MM_TASK_QUEUE_OVERFLOW = 18U,
    XAIE_DMA_STATUS_S2MM_CHANNEL_RUNNING,
    XAIE_DMA_STATUS_S2MM_TASK_QUEUE_SIZE,
    XAIE_DMA_STATUS_S2MM_CURRENT_BD = 24U,
    XAIE_DMA_STATUS_S2MM_MAX
  };
  
  enum class dma_mm2s_status : uint32_t
  {
    XAIE_DMA_STATUS_MM2S_STATUS = 0U,
    XAIE_DMA_STATUS_MM2S_STALLED_LOCK_ACK = 2U,
    XAIE_DMA_STATUS_MM2S_STALLED_LOCK_REL,
    XAIE_DMA_STATUS_MM2S_STALLED_STREAM_BACKPRESSURE,
    XAIE_DMA_STATUS_MM2S_STALLED_TCT,
    XAIE_DMA_STATUS_MM2S_ERROR_LOCK_ACCESS_TO_UNAVAIL = 8U, // Specific only to MEM Tile
    XAIE_DMA_STATUS_MM2S_ERROR_DM_ACCESS_TO_UNAVAIL,        // Specific only to MEM Tile
    XAIE_DMA_STATUS_MM2S_ERROR_BD_UNAVAIL,
    XAIE_DMA_STATUS_MM2S_ERROR_BD_INVALID = 11U,
    XAIE_DMA_STATUS_MM2S_AXI_MM_DECODE_ERROR = 16U,
    XAIE_DMA_STATUS_MM2S_AXI_MM_SLAVE_ERROR  = 17U,
    XAIE_DMA_STATUS_MM2S_TASK_QUEUE_OVERFLOW = 18U,
    XAIE_DMA_STATUS_MM2S_CHANNEL_RUNNING,
    XAIE_DMA_STATUS_MM2S_TASK_QUEUE_SIZE,
    XAIE_DMA_STATUS_MM2S_CURRENT_BD = 24U,
    XAIE_DMA_STATUS_MM2S_MAX
  };

  void
  aie_status_version_check(uint16_t &major_ver, uint16_t &minor_ver);

  void
  aie_info_sanity_check(uint32_t start_col, uint32_t num_cols, aie_tiles_info &info);

  template <typename tile_type>
  std::vector<std::unique_ptr<aie_col_status>>
  parse_data_from_buf(char *buf, aie_tiles_info &info)
  {
    std::vector<std::unique_ptr<aie_col_status>> aie_cols;

    for (uint32_t i = 0; i < info.cols; i++) {
      aie_cols.emplace_back(std::make_unique<aie_col_status>(info));
    }

    tile_type::parse_buf(buf, info, aie_cols);
    return aie_cols;
  }

  template <typename tile_type>
  boost::property_tree::ptree
  format_aie_info(std::vector<std::unique_ptr<aie_col_status>> &aie_cols,
                  uint32_t start_col,
                  uint32_t cols,
                  aie_tiles_info &tiles_info)
  {
    return tile_type::format_status(aie_cols, start_col, cols, tiles_info);
  }

  void
  get_aie_status_version_info(xclDeviceHandle hdl, uint16_t &aie_ver_major, uint16_t &aie_ver_minor);
  
  aie_tiles_info
  get_aie_tiles_info(xclDeviceHandle hdl);
  
  void
  get_aie_col_info(xclDeviceHandle hdl, char *buf, uint32_t size, uint32_t start_col, uint32_t cols);
} // asd_parser
#endif