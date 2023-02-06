// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include "asd_parser.h"

namespace asd_parser {
namespace bpt = boost::property_tree;

static std::vector<std::string> status_map;
static std::vector<std::string> dma_s2mm_map;
static std::vector<std::string> dma_mm2s_map;

static void
initialize_mapping()
{
  // core status
  status_map.resize(uint32_t(core_status::xaie_core_status_max_bit));
  status_map[uint32_t(core_status::xaie_core_status_enable_bit)] = "Enable";
  status_map[uint32_t(core_status::xaie_core_status_reset_bit)] = "Reset";
  status_map[uint32_t(core_status::xaie_core_status_mem_stall_s_bit)] = "Memory_Stall_S";
  status_map[uint32_t(core_status::xaie_core_status_mem_stall_w_bit)] = "Memory_Stall_W";
  status_map[uint32_t(core_status::xaie_core_status_mem_stall_n_bit)] = "Memory_Stall_N";
  status_map[uint32_t(core_status::xaie_core_status_mem_stall_e_bit)] = "Memory_Stall_E";
  status_map[uint32_t(core_status::xaie_core_status_lock_stall_s_bit)] = "Lock_Stall_S";
  status_map[uint32_t(core_status::xaie_core_status_lock_stall_w_bit)] = "Lock_Stall_W";
  status_map[uint32_t(core_status::xaie_core_status_lock_stall_n_bit)] = "Lock_Stall_N";
  status_map[uint32_t(core_status::xaie_core_status_lock_stall_e_bit)] = "Lock_Stall_E";
  status_map[uint32_t(core_status::xaie_core_status_stream_stall_ss0_bit)] = "Stream_Stall_SS0";
  status_map[uint32_t(core_status::xaie_core_status_stream_stall_ms0_bit)] = "Stream_Stall_MS0";
  status_map[uint32_t(core_status::xaie_core_status_cascade_stall_scd_bit)] = "Cascade_Stall_SCD";
  status_map[uint32_t(core_status::xaie_core_status_cascade_stall_mcd_bit)] = "Cascade_Stall_MCD";
  status_map[uint32_t(core_status::xaie_core_status_debug_halt_bit)] = "Debug_Halt";
  status_map[uint32_t(core_status::xaie_core_status_ecc_error_stall_bit)] = "ECC_Error_Stall";
  status_map[uint32_t(core_status::xaie_core_status_ecc_scrubbing_stall_bit)] = "ECC_Scrubbing_Stall";
  status_map[uint32_t(core_status::xaie_core_status_error_halt_bit)] = "Error_Halt";
  status_map[uint32_t(core_status::xaie_core_status_done_bit)] = "Core_Done";
  status_map[uint32_t(core_status::xaie_core_status_processor_bus_stall_bit)] = "Core_Proc_Bus_Stall";

  // DMA s2mm status
  dma_s2mm_map.resize(uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_max));
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_status)] = "Status";
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_stalled_lock_ack)] = "Stalled_Lock_Acq";
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_stalled_lock_rel)] = "Stalled_Lock_Rel";
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_stalled_stream_starvation)] = "Stalled_Stream_Starvation";
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_stalled_tct_or_count_fifo_full)] = "Stalled_TCT_Or_Count_FIFO_Full";
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_error_lock_access_to_unavail)] = "Error_Lock_Access_Unavail",
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_error_dm_access_to_unavail)] = "Error_DM_Access_Unavail",
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_error_bd_unavail)] = "Error_BD_Unavail";
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_error_bd_invalid)] = "Error_BD_Invalid";
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_error_fot_length)] = "Error_FoT_Length";
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_error_fot_bds_per_task)] = "Error_Fot_BDs";
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_axi_mm_decode_error)] = "AXI-MM_decode_error";
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_axi_mm_slave_error)] = "AXI-MM_slave_error";
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_task_queue_overflow)] = "Task_Queue_Overflow";
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_channel_running)] = "Channel_Running";
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_task_queue_size)] = "Task_Queue_Size";
  dma_s2mm_map[uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_current_bd)] = "Cur_BD";

  // DMA mm2s status
  dma_mm2s_map.resize(uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_max));
  dma_mm2s_map[uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_status)] = "Status";
  dma_mm2s_map[uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_stalled_lock_ack)] = "Stalled_Lock_Acq";
  dma_mm2s_map[uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_stalled_lock_rel)] = "Stalled_Lock_Rel";
  dma_mm2s_map[uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_stalled_stream_backpressure)] = "Stalled_Stream_Back_Pressure";
  dma_mm2s_map[uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_stalled_tct)] = "Stalled_TCT";
  dma_mm2s_map[uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_error_lock_access_to_unavail)] = "Error_Lock_Access_Unavail";
  dma_mm2s_map[uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_error_dm_access_to_unavail)] = "Error_DM_Access_Unavail";
  dma_mm2s_map[uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_error_bd_unavail)] = "Error_BD_Unavail";
  dma_mm2s_map[uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_error_bd_invalid)] = "Error_BD_Invalid";
  dma_mm2s_map[uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_axi_mm_decode_error)] = "AXI-MM_decode_error";
  dma_mm2s_map[uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_axi_mm_slave_error)] = "AXI-MM_slave_error";
  dma_mm2s_map[uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_task_queue_overflow)] = "Task_Queue_Overflow";
  dma_mm2s_map[uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_channel_running)] = "Channel_Running";
  dma_mm2s_map[uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_task_queue_size)] = "Task_Queue_Size";
  dma_mm2s_map[uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_current_bd)] = "Cur_BD";
}

struct initialize { 
  initialize() { initialize_mapping(); }
};
static initialize obj;

static constexpr uint8_t dma_channel_status = 0x3;
static constexpr uint8_t dma_queue_overflow = 0x1;
static constexpr uint8_t dma_queue_size = 0x7;
static constexpr uint8_t dma_current_bd = 0x3f;
static constexpr uint8_t dma_default = 0x1;

/* Internal Functions */
static aie_dma_int 
get_dma_mm2s_status(uint32_t status, aie_tile_type tile_type)
{
  aie_dma_int dma_status_int;

  for (auto flag = uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_status); 
       flag <= uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_max); flag++) {
    // Below is for bits  8, 9, 10 in DMA_MM2S mem tile
    if ((tile_type != aie_tile_type::mem) && 
        ((flag == uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_error_lock_access_to_unavail)) || 
        (flag == uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_error_dm_access_to_unavail)) || 
        (flag == uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_error_bd_unavail))))
      continue;

    // Below is for bits 16, 17 in DMA_MM2S shim tile
    if ((tile_type != aie_tile_type::shim) && 
        ((flag == uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_axi_mm_decode_error)) || 
        (flag == uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_axi_mm_slave_error))))
      continue;

    if (!dma_mm2s_map[flag].empty()) {
      uint32_t val = (status >> flag);

      switch (flag) {
        case uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_status) :
          val &= dma_channel_status;
          if (val == 0)
            dma_status_int.channel_status.emplace_back("Idle");
          else if (val == 1)
            dma_status_int.channel_status.emplace_back("Starting");
          else if (val == 2)
            dma_status_int.channel_status.emplace_back("Running");
          else
            dma_status_int.channel_status.emplace_back("Invalid State");
            
          break;
        case uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_task_queue_overflow) :
          val &= dma_queue_overflow;
          if (val == 0)
            dma_status_int.queue_status = "okay";
          else
            dma_status_int.queue_status = "channel_overflow";

          break;
        case uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_task_queue_size) :
          dma_status_int.queue_size = (val & dma_queue_size);
          break;
        case uint32_t(dma_mm2s_status::xaie_dma_status_mm2s_current_bd) :
          dma_status_int.current_bd = (val & dma_current_bd);
          break;
        default :
          val &= dma_default;
          if (val)
            dma_status_int.channel_status.emplace_back(dma_mm2s_map[flag]); 
      }
    }
  }

  return dma_status_int;
}

static aie_dma_int 
get_dma_s2mm_status(uint32_t status, aie_tile_type tile_type)
{
  aie_dma_int dma_status_int;

  for (auto flag = uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_status); 
       flag <= uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_max); flag++) {
    // Below is for bits  8, 9 in DMA_S2MM mem tile
    if ((tile_type != aie_tile_type::mem) && 
        ((flag == uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_error_lock_access_to_unavail)) || 
        (flag == uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_error_dm_access_to_unavail))))
      continue;

    // Below is for bits 16, 17 in DMA_S2MM shim tile
    if ((tile_type != aie_tile_type::shim) && 
        ((flag == uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_axi_mm_decode_error)) || 
        (flag == uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_axi_mm_slave_error))))
      continue;

    if (!dma_s2mm_map[flag].empty()) {
      uint32_t val = (status >> flag);

      switch (flag) {
        case uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_status) :
          val &= dma_channel_status;
          if (val == 0)
            dma_status_int.channel_status.emplace_back("Idle");
          else if (val == 1)
            dma_status_int.channel_status.emplace_back("Starting");
          else if (val == 2)
            dma_status_int.channel_status.emplace_back("Running");
          else
            dma_status_int.channel_status.emplace_back("Invalid State");
            
          break;
        case uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_task_queue_overflow) :
          val &= dma_queue_overflow;
          if (val == 0)
            dma_status_int.queue_status = "okay";
          else
            dma_status_int.queue_status = "channel_overflow";

          break;
        case uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_channel_running) :
          dma_status_int.queue_size = (val & dma_queue_size);
          break;
        case uint32_t(dma_s2mm_status::xaie_dma_status_s2mm_current_bd) :
          dma_status_int.current_bd = (val & dma_current_bd);
          break;
        default :
          val &= dma_default;
          if (val)
            dma_status_int.channel_status.emplace_back(dma_s2mm_map[flag]); 
      }
    }
  }

  return dma_status_int;
}

static void
populate_dma(std::vector<aie_dma_status> dma, bpt::ptree& dma_pt, aie_tile_type tile_type)
{
  bpt::ptree channel_status_mm2s_array;
  bpt::ptree channel_status_s2mm_array;
  bpt::ptree queue_size_mm2s_array;
  bpt::ptree queue_size_s2mm_array;
  bpt::ptree queue_status_mm2s_array;
  bpt::ptree queue_status_s2mm_array;
  bpt::ptree current_bd_mm2s_array;
  bpt::ptree current_bd_s2mm_array;

  for (uint32_t i = 0; i < dma.size(); i++)
  {
    // channel status
    bpt::ptree channel_status_mm2s;
    bpt::ptree channel_status_s2mm;

    auto dma_mm2s_status = get_dma_mm2s_status(dma[i].mm2s_status, tile_type);
    auto dma_s2mm_status = get_dma_s2mm_status(dma[i].s2mm_status, tile_type);

    std::string mm2s_channel_status = "";
    for (auto &status : dma_mm2s_status.channel_status)
      mm2s_channel_status.append(status.append(","));

    mm2s_channel_status.erase(mm2s_channel_status.length() - 1); //remove last comma
    channel_status_mm2s.put("", mm2s_channel_status);

    std::string s2mm_channel_status = "";
    for (auto &status : dma_s2mm_status.channel_status)
      s2mm_channel_status.append(status.append(","));

    s2mm_channel_status.erase(s2mm_channel_status.length() - 1); //remove last comma
    channel_status_s2mm.put("", s2mm_channel_status);

    channel_status_mm2s_array.push_back(std::make_pair("", channel_status_mm2s));
    channel_status_s2mm_array.push_back(std::make_pair("", channel_status_s2mm)); 

    // queue size
    bpt::ptree queue_size_mm2s;
    bpt::ptree queue_size_s2mm;

    queue_size_mm2s.put("", std::to_string(dma_mm2s_status.queue_size));
    queue_size_mm2s_array.push_back(std::make_pair("", queue_size_mm2s));
    queue_size_s2mm.put("", std::to_string(dma_s2mm_status.queue_size)); 
    queue_size_s2mm_array.push_back(std::make_pair("", queue_size_s2mm));
      
    // queue status
    bpt::ptree queue_status_mm2s;
    bpt::ptree queue_status_s2mm;

    queue_status_mm2s.put("", dma_mm2s_status.queue_status);
    queue_status_mm2s_array.push_back(std::make_pair("", queue_status_mm2s));
    queue_status_s2mm.put("", dma_s2mm_status.queue_status);
    queue_status_s2mm_array.push_back(std::make_pair("", queue_status_s2mm));

    // current bd
    bpt::ptree bd_mm2s;
    bpt::ptree bd_s2mm;

    bd_mm2s.put("", std::to_string(dma_mm2s_status.current_bd));
    current_bd_mm2s_array.push_back(std::make_pair("", bd_mm2s));
    bd_s2mm.put("", std::to_string(dma_s2mm_status.current_bd));
    current_bd_s2mm_array.push_back(std::make_pair("", bd_mm2s));
  }

  dma_pt.add_child("channel_status.mm2s", channel_status_mm2s_array);
  dma_pt.add_child("channel_status.s2mm", channel_status_s2mm_array);

  dma_pt.add_child("queue_size.mm2s", queue_size_mm2s_array);
  dma_pt.add_child("queue_size.s2mm", queue_size_s2mm_array);

  dma_pt.add_child("queue_status.mm2s", queue_status_mm2s_array);
  dma_pt.add_child("queue_status.s2mm", queue_status_s2mm_array);

  dma_pt.add_child("current_bd.mm2s", current_bd_mm2s_array);
  dma_pt.add_child("current_bd.s2mm", current_bd_s2mm_array);
}

static void
populate_locks(std::vector<uint8_t> locks, bpt::ptree& lock_pt)
{
  for (uint32_t i = 0; i < locks.size(); i++)
  {
    bpt::ptree lock;
    bpt::ptree lock_array;
      
    lock.put("", locks[i]);
    lock_array.push_back(std::make_pair("", lock));

    lock_pt.add_child(std::to_string(i), lock_array);
  }
}

static void 
core_status_to_string_array(uint32_t status, std::vector<std::string>& status_vec)
{
  uint32_t count = 0;

  // if neither bit 0 is set nor bit 1 is set, then core status is 'Disable'
  if (!((status & 0x1) || (status & 0x2))){
    status_vec.push_back("Disable");
    status >>= 2;
  }

  while (status) {
    if (status & 0x1)
      status_vec.push_back(status_map[count]);
  
    status >>= 1;
    count++;
  }
}

/* Functions related to Core Tile */
uint64_t
aie_core_tile_status::size() {
  return sizeof(aie_dma_status) * dma.size() + sizeof(uint32_t) * events.size() +
      sizeof(uint8_t) * lock_value.size() + sizeof(uint32_t) * 4 /*(cs,pc,sp,lr)*/;
}

uint64_t
aie_core_tile_status::size(aie_tiles_info& info) {
  return sizeof(aie_dma_status) * info.core_dma_channels + sizeof(uint32_t) * info.core_events +
      sizeof(uint8_t) * info.core_locks + sizeof(uint32_t) * 4 /*(cs,pc,sp,lr)*/;
}

void
aie_core_tile_status::parse_buf(char* buf, aie_tiles_info& info, std::vector<aie_col_status>& aie_cols) {
  for (auto &aie_col : aie_cols) {
    for (auto &core : aie_col.core_tile) {
      // DMA status
      uint32_t size_cal = sizeof(aie_dma_status) * info.core_dma_channels;
      std::memcpy(core.dma.data(), buf, size_cal);
      buf += size_cal;

      // Events
      size_cal = sizeof(uint32_t) * info.core_events;
      std::memcpy(core.events.data(), buf, size_cal);
      buf += size_cal;

      // core status
      size_cal = sizeof(uint32_t);
      std::memcpy(&core.core_status, buf, size_cal);
      buf += size_cal;

      // program counter
      size_cal = sizeof(uint32_t);
      std::memcpy(&core.program_counter, buf, size_cal);
      buf += size_cal;

      // stack ptr
      size_cal = sizeof(uint32_t);
      std::memcpy(&core.stack_ptr, buf, size_cal);
      buf += size_cal;

      // link reg
      size_cal = sizeof(uint32_t);
      std::memcpy(&core.link_reg, buf, size_cal);
      buf += size_cal;

      // Locks
      size_cal = sizeof(uint8_t) * info.core_locks;
      std::memcpy(core.lock_value.data(), buf, size_cal);
      buf += size_cal;
    }

    // add mem tiles and shim tiles offsets
    buf = buf + aie_mem_tile_status::size(info) * info.mem_rows + aie_shim_tile_status::size(info) * info.shim_rows;
  }
}

static void 
get_core_tile_info(aie_core_tile_status& core, bpt::ptree& pt)
{
  bpt::ptree core_pt;
  bpt::ptree status_array;
  bpt::ptree tmp;
  bpt::ptree tmp_array;
  std::vector<std::string> status_vec;
    
  core_status_to_string_array(core.core_status, status_vec);
  for (auto &x : status_vec) {
    bpt::ptree status_pt;
    status_pt.put("", x);
    status_array.push_back(std::make_pair("", status_pt));
  }
  core_pt.add_child("status", status_array);

  // fill program counter as array
  tmp.put("", core.program_counter);
  tmp_array.push_back(std::make_pair("", tmp));
  core_pt.add_child("pc", tmp_array);

  // fill stack pointer as array
  tmp_array = {};
  tmp.put("", core.stack_ptr);
  tmp_array.push_back(std::make_pair("", tmp));
  core_pt.add_child("sp", tmp_array);

  // fill link register as array
  tmp_array = {};
  tmp.put("", core.link_reg);
  tmp_array.push_back(std::make_pair("", tmp));
  core_pt.add_child("lr", tmp_array);

  pt.add_child("core", core_pt);

  // fill DMA status
  bpt::ptree dma_pt;
  populate_dma(core.dma, dma_pt, aie_tile_type::core);
  pt.add_child("dma", dma_pt);

  // fill Lock's info
  bpt::ptree lock_pt;
  populate_locks(core.lock_value, lock_pt);
  pt.add_child("lock", lock_pt);
}

bpt::ptree
aie_core_tile_status::format_status(std::vector<aie_col_status>& aie_cols,
                                    uint32_t start_col,
                                    uint32_t cols,
                                    aie_tiles_info& tiles_info)
{
  bpt::ptree pt_array;

  for (uint32_t col = start_col; col < cols; col++) {
    for (uint32_t row = 0; row < tiles_info.core_rows; row++) {
      bpt::ptree pt;
      pt.put("col", col);
      pt.put("row", row + tiles_info.core_row_start);
          
      get_core_tile_info(aie_cols[col].core_tile[row], pt);
      pt_array.push_back(std::make_pair(std::to_string(col) + "_" +
          std::to_string(row + tiles_info.core_row_start), pt));
    }
  }

  bpt::ptree pt_aie_core;
  pt_aie_core.add_child("aie_core", pt_array);  
  return pt_aie_core;
}

/* Functions related to Mem Tile */
uint64_t
aie_mem_tile_status::size() {
  return sizeof(aie_dma_status) * dma.size() + sizeof(uint32_t) * events.size() +
      sizeof(uint8_t) * lock_value.size();
}

uint64_t
aie_mem_tile_status::size(aie_tiles_info& info) {
  return sizeof(aie_dma_status) * info.mem_dma_channels + sizeof(uint32_t) * info.mem_events +
      sizeof(uint8_t) * info.mem_locks;
}

void
aie_mem_tile_status::parse_buf(char* buf, aie_tiles_info& info, std::vector<aie_col_status>& aie_cols)
{
  for (auto &aie_col : aie_cols) {
    // add core tiles offset
    buf += aie_core_tile_status::size(info) * info.core_rows;

    for (auto &mem : aie_col.mem_tile) {
      // DMA status
      uint32_t size_cal = sizeof(aie_dma_status) * info.mem_dma_channels;
      std::memcpy(mem.dma.data(), buf, size_cal);
      buf += size_cal;

      // Events
      size_cal = sizeof(uint32_t) * info.mem_events;
      std::memcpy(mem.events.data(), buf, size_cal);
      buf += size_cal;

      // Locks
      size_cal = sizeof(uint8_t) * info.mem_locks;
      std::memcpy(mem.lock_value.data(), buf, size_cal);
      buf += size_cal;
    }

    // add shim tiles offset
    buf += aie_shim_tile_status::size(info) * info.shim_rows;
  }
}

static void 
get_mem_tile_info(aie_mem_tile_status& mem, bpt::ptree& pt)
{
  // fill DMA status
  bpt::ptree dma_pt;
  populate_dma(mem.dma, dma_pt, aie_tile_type::mem);
  pt.add_child("dma", dma_pt);

  //fill Lock's info
  bpt::ptree lock_pt;
  populate_locks(mem.lock_value, lock_pt);
  pt.add_child("lock", lock_pt);
}

bpt::ptree
aie_mem_tile_status::format_status(std::vector<aie_col_status>& aie_cols,
                                   uint32_t start_col,
                                   uint32_t cols,
                                   aie_tiles_info& tiles_info)
{
  bpt::ptree pt_array;
   
  for (uint32_t col = start_col; col < cols; col++) {
    for (uint32_t row = 0; row < tiles_info.mem_rows; row++) {
      bpt::ptree pt;
      pt.put("col", col);
      pt.put("row", row + tiles_info.mem_row_start);
        
      get_mem_tile_info(aie_cols[col].mem_tile[row], pt);
      pt_array.push_back(std::make_pair(std::to_string(col) + "_" + 
          std::to_string(row + tiles_info.mem_row_start), pt));
    }
  }
    
  bpt::ptree pt_aie_mem;
  pt_aie_mem.add_child("aie_mem", pt_array);
  return pt_aie_mem;
}

/* Functions related to Shim Tile */
uint64_t
aie_shim_tile_status::size() {
  return sizeof(aie_dma_status) * dma.size() + sizeof(uint32_t) * events.size() +
     sizeof(uint8_t) * lock_value.size();
}

uint64_t
aie_shim_tile_status::size(aie_tiles_info& info) {
  return sizeof(aie_dma_status) * info.shim_dma_channels + sizeof(uint32_t) * info.shim_events +
      sizeof(uint8_t) * info.shim_locks;
}

void
aie_shim_tile_status::parse_buf(char* buf, aie_tiles_info& info, std::vector<aie_col_status>& aie_cols)
{
  for (auto &aie_col : aie_cols) {
    // add core tiles and mem tiles offsets
    buf = buf + aie_core_tile_status::size(info) * info.core_rows + aie_mem_tile_status::size(info) * info.mem_rows;

    for (auto &shim : aie_col.shim_tile) {
      // DMA status
      uint32_t size_cal = sizeof(aie_dma_status) * info.shim_dma_channels;
      std::memcpy(shim.dma.data(), buf, size_cal);
      buf += size_cal;

      // Events
      size_cal = sizeof(uint32_t) * info.shim_events;
      std::memcpy(shim.events.data(), buf, size_cal);
      buf += size_cal;

      // Locks
      size_cal = sizeof(uint8_t) * info.shim_locks;
      std::memcpy(shim.lock_value.data(), buf, size_cal);
      buf += size_cal;
    }
  }
}

static void 
get_shim_tile_info(aie_shim_tile_status& shim, bpt::ptree& pt)
{
  // fill DMA status
  bpt::ptree dma_pt;
  populate_dma(shim.dma, dma_pt, aie_tile_type::shim);
  pt.add_child("dma", dma_pt);

  //fill Lock's info
  bpt::ptree lock_pt;
  populate_locks(shim.lock_value, lock_pt);
  pt.add_child("lock", lock_pt);
}

bpt::ptree
aie_shim_tile_status::format_status(std::vector<aie_col_status>& aie_cols,
                                    uint32_t start_col,
                                    uint32_t cols,
                                    aie_tiles_info& tiles_info)
{
  bpt::ptree pt_array;
  
  for (uint32_t col = start_col; col < cols; col++) {
    for (uint32_t row = 0; row < tiles_info.shim_rows; row++) {
      bpt::ptree pt;
      pt.put("col", col);
      pt.put("row", row + tiles_info.shim_row_start);

      get_shim_tile_info(aie_cols[col].shim_tile[row], pt);
      pt_array.push_back(std::make_pair(std::to_string(col) + "_" + 
                         std::to_string(row + tiles_info.shim_row_start), pt));
    }
  }
    
  bpt::ptree pt_aie_shim;
  pt_aie_shim.add_child("aie_shim", pt_array);
  return pt_aie_shim;
}

/* Common functions*/
void
aie_status_version_check(uint16_t& major_ver, uint16_t& minor_ver)
{
  if (!((major_ver == asd_parser::aie_status_version_major) && (minor_ver == asd_parser::aie_status_version_minor)))
    throw std::runtime_error("Aie status version mismatch");
}

void
aie_info_sanity_check(uint32_t start_col, uint32_t num_cols, aie_tiles_info& info)
{
  if ((start_col + num_cols) > info.cols)
    throw std::runtime_error("Requested columns exceeds maximum available columns\n");

  if (info.col_size == 0)
    throw std::runtime_error("Getting Aie column size info from driver failed\n");

  // calculate single column size using aie_tiles_info
  uint64_t calculated_size = aie_core_tile_status::size(info) * info.core_rows +
      aie_shim_tile_status::size(info) * info.shim_rows + aie_mem_tile_status::size(info) * info.mem_rows;

  // check calucalted size is same as size info from driver
  if (calculated_size != info.col_size)
    throw std::runtime_error("calculated size doesnot match size information from driver, version mismatch\n");
}
}

