// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

#include "asd_parser.h"
#include "query_requests.h"

namespace asd_parser {
namespace bpt = boost::property_tree;
using u32 = uint32_t;

static std::vector<std::string> status_map;
static std::vector<std::string> dma_s2mm_map;
static std::vector<std::string> dma_mm2s_map;

static void
initialize_mapping()
{
  // core status
  status_map.resize(static_cast<u32>(core_status::xaie_core_status_max_bit));
  status_map[static_cast<u32>(core_status::xaie_core_status_enable_bit)] = "Enable";
  status_map[static_cast<u32>(core_status::xaie_core_status_reset_bit)] = "Reset";
  status_map[static_cast<u32>(core_status::xaie_core_status_mem_stall_s_bit)] = "Memory_Stall_S";
  status_map[static_cast<u32>(core_status::xaie_core_status_mem_stall_w_bit)] = "Memory_Stall_W";
  status_map[static_cast<u32>(core_status::xaie_core_status_mem_stall_n_bit)] = "Memory_Stall_N";
  status_map[static_cast<u32>(core_status::xaie_core_status_mem_stall_e_bit)] = "Memory_Stall_E";
  status_map[static_cast<u32>(core_status::xaie_core_status_lock_stall_s_bit)] = "Lock_Stall_S";
  status_map[static_cast<u32>(core_status::xaie_core_status_lock_stall_w_bit)] = "Lock_Stall_W";
  status_map[static_cast<u32>(core_status::xaie_core_status_lock_stall_n_bit)] = "Lock_Stall_N";
  status_map[static_cast<u32>(core_status::xaie_core_status_lock_stall_e_bit)] = "Lock_Stall_E";
  status_map[static_cast<u32>(core_status::xaie_core_status_stream_stall_ss0_bit)] = "Stream_Stall_SS0";
  status_map[static_cast<u32>(core_status::xaie_core_status_stream_stall_ms0_bit)] = "Stream_Stall_MS0";
  status_map[static_cast<u32>(core_status::xaie_core_status_cascade_stall_scd_bit)] = "Cascade_Stall_SCD";
  status_map[static_cast<u32>(core_status::xaie_core_status_cascade_stall_mcd_bit)] = "Cascade_Stall_MCD";
  status_map[static_cast<u32>(core_status::xaie_core_status_debug_halt_bit)] = "Debug_Halt";
  status_map[static_cast<u32>(core_status::xaie_core_status_ecc_error_stall_bit)] = "ECC_Error_Stall";
  status_map[static_cast<u32>(core_status::xaie_core_status_ecc_scrubbing_stall_bit)] = "ECC_Scrubbing_Stall";
  status_map[static_cast<u32>(core_status::xaie_core_status_error_halt_bit)] = "Error_Halt";
  status_map[static_cast<u32>(core_status::xaie_core_status_done_bit)] = "Core_Done";
  status_map[static_cast<u32>(core_status::xaie_core_status_processor_bus_stall_bit)] = "Core_Proc_Bus_Stall";

  // DMA s2mm status
  dma_s2mm_map.resize(static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_max));
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_status)] = "Status";
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_stalled_lock_ack)] = "Stalled_Lock_Acq";
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_stalled_lock_rel)] = "Stalled_Lock_Rel";
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_stalled_stream_starvation)] = 
      "Stalled_Stream_Starvation";
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_stalled_tct_or_count_fifo_full)] = 
      "Stalled_TCT_Or_Count_FIFO_Full";
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_error_lock_access_to_unavail)] = 
      "Error_Lock_Access_Unavail",
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_error_dm_access_to_unavail)] = 
      "Error_DM_Access_Unavail",
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_error_bd_unavail)] = "Error_BD_Unavail";
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_error_bd_invalid)] = "Error_BD_Invalid";
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_error_fot_length)] = "Error_FoT_Length";
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_error_fot_bds_per_task)] = "Error_Fot_BDs";
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_axi_mm_decode_error)] = "AXI-MM_decode_error";       
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_axi_mm_slave_error)] = "AXI-MM_slave_error";
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_task_queue_overflow)] = "Task_Queue_Overflow";       
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_channel_running)] = "Channel_Running";
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_task_queue_size)] = "Task_Queue_Size";
  dma_s2mm_map[static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_current_bd)] = "Cur_BD";

  // DMA mm2s status
  dma_mm2s_map.resize(static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_max));
  dma_mm2s_map[static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_status)] = "Status";
  dma_mm2s_map[static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_stalled_lock_ack)] = "Stalled_Lock_Acq";
  dma_mm2s_map[static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_stalled_lock_rel)] = "Stalled_Lock_Rel";
  dma_mm2s_map[static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_stalled_stream_backpressure)] = 
      "Stalled_Stream_Back_Pressure";
  dma_mm2s_map[static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_stalled_tct)] = "Stalled_TCT";
  dma_mm2s_map[static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_error_lock_access_to_unavail)] = 
      "Error_Lock_Access_Unavail";
  dma_mm2s_map[static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_error_dm_access_to_unavail)] = 
      "Error_DM_Access_Unavail";
  dma_mm2s_map[static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_error_bd_unavail)] = "Error_BD_Unavail";
  dma_mm2s_map[static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_error_bd_invalid)] = "Error_BD_Invalid";
  dma_mm2s_map[static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_axi_mm_decode_error)] = "AXI-MM_decode_error";       
  dma_mm2s_map[static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_axi_mm_slave_error)] = "AXI-MM_slave_error";
  dma_mm2s_map[static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_task_queue_overflow)] = "Task_Queue_Overflow";       
  dma_mm2s_map[static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_channel_running)] = "Channel_Running";
  dma_mm2s_map[static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_task_queue_size)] = "Task_Queue_Size";
  dma_mm2s_map[static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_current_bd)] = "Cur_BD";
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
static constexpr uint8_t lock_mask = 0x3f;

/* Internal Functions */
static aie_dma_int 
get_dma_mm2s_status(uint32_t status, aie_tile_type tile_type)
{
  aie_dma_int dma_status_int{};

  for (auto flag = static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_status); 
      flag < static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_max); flag++) {
    // Below is for bits  8, 9, 10 in DMA_MM2S mem tile
    if ((tile_type != aie_tile_type::mem) && 
        ((flag == static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_error_lock_access_to_unavail)) || 
        (flag == static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_error_dm_access_to_unavail)) || 
        (flag == static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_error_bd_unavail))))
      continue;

    // Below is for bits 16, 17 in DMA_MM2S shim tile
    if ((tile_type != aie_tile_type::shim) && 
        ((flag == static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_axi_mm_decode_error)) || 
        (flag == static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_axi_mm_slave_error))))
      continue;

    if (!dma_mm2s_map[flag].empty()) {
      uint32_t val = (status >> flag);

      switch (flag) {
        case static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_status) :
          val &= dma_channel_status;
          if (val == 0)
            dma_status_int.channel_status = "Idle";
          else if (val == 1)
            dma_status_int.channel_status = "Starting";
          else if (val == 2)
            dma_status_int.channel_status = "Running";
          else
            dma_status_int.channel_status = "Invalid State";
          break;
        case static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_task_queue_overflow) :
          val &= dma_queue_overflow;
          if (val == 0)
            dma_status_int.queue_status = "okay";
          else
            dma_status_int.queue_status = "channel_overflow";
          break;
        case static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_task_queue_size) :
          dma_status_int.queue_size = (val & dma_queue_size);
          break;
        case static_cast<u32>(dma_mm2s_status::xaie_dma_status_mm2s_current_bd) :
          dma_status_int.current_bd = (val & dma_current_bd);
          break;
        default:
          val &= dma_default;
          if (val)
            dma_status_int.channel_status = dma_mm2s_map[flag]; 
          break;
      }
    }
  }

  return dma_status_int;
}

static aie_dma_int 
get_dma_s2mm_status(uint32_t status, aie_tile_type tile_type)
{
  aie_dma_int dma_status_int{};

  for (auto flag = static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_status); 
      flag < static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_max); flag++) {
    // Below is for bits  8, 9 in DMA_S2MM mem tile
    if ((tile_type != aie_tile_type::mem) && 
        ((flag == static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_error_lock_access_to_unavail)) || 
        (flag == static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_error_dm_access_to_unavail))))
      continue;

    // Below is for bits 16, 17 in DMA_S2MM shim tile
    if ((tile_type != aie_tile_type::shim) && 
        ((flag == static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_axi_mm_decode_error)) || 
        (flag == static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_axi_mm_slave_error))))
      continue;

    if (!dma_s2mm_map[flag].empty()) {
      uint32_t val = (status >> flag);

      switch (flag) {
        case static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_status) :
          val &= dma_channel_status;
          if (val == 0)
            dma_status_int.channel_status = "Idle";
          else if (val == 1)
            dma_status_int.channel_status = "Starting";
          else if (val == 2)
            dma_status_int.channel_status = "Running";
          else
            dma_status_int.channel_status = "Invalid State";
          break;
        case static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_task_queue_overflow) :
          val &= dma_queue_overflow;
          if (val == 0)
            dma_status_int.queue_status = "okay";
          else
            dma_status_int.queue_status = "channel_overflow";
          break;
        case static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_channel_running) :
          dma_status_int.queue_size = (val & dma_queue_size);
          break;
        case static_cast<u32>(dma_s2mm_status::xaie_dma_status_s2mm_current_bd) :
          dma_status_int.current_bd = (val & dma_current_bd);
          break;
        default :
          val &= dma_default;
          if (val)
            dma_status_int.channel_status = dma_s2mm_map[flag];
          break;
      }
    }
  }

  return dma_status_int;
}

static bpt::ptree
populate_channel(const aie_dma_int& channel)
{
  bpt::ptree pt_channel;

  pt_channel.put("status", channel.channel_status);
  pt_channel.put("queue_size", std::to_string(channel.queue_size));
  pt_channel.put("queue_status", channel.queue_status);
  pt_channel.put("current_bd", std::to_string(channel.current_bd));

  return pt_channel;
}

static bpt::ptree
populate_dma(const std::vector<aie_dma_status>& dma, aie_tile_type tile_type)
{
  bpt::ptree dma_pt;
  bpt::ptree mm2s_channels;
  bpt::ptree s2mm_channels;

  for (uint32_t i = 0; i < dma.size(); i++)
  {
    bpt::ptree mm2s_channel = populate_channel(get_dma_mm2s_status(dma[i].mm2s_status, tile_type));
    bpt::ptree s2mm_channel = populate_channel(get_dma_s2mm_status(dma[i].s2mm_status, tile_type));

    mm2s_channels.push_back(std::make_pair("", mm2s_channel));
    s2mm_channels.push_back(std::make_pair("", s2mm_channel));
  }

  dma_pt.add_child("mm2s_channels", mm2s_channels);
  dma_pt.add_child("s2mm_channels", s2mm_channels);

  return dma_pt;
}

static bpt::ptree
populate_locks(const std::vector<uint8_t>& locks)
{
  bpt::ptree pt_locks;

  for (uint32_t i = 0; i < locks.size(); i++) {
    bpt::ptree pt_lock;

    pt_lock.put("id", std::to_string(i));
    pt_lock.put("events", locks[i] & lock_mask);
    pt_locks.push_back(std::make_pair("", pt_lock));
  }

  return pt_locks;
}

static std::vector<std::string>
core_status_to_string_array(uint32_t status)
{
  std::vector<std::string> status_vec;

  // if neither Enable bit is set nor Reset bit is set, then core status is 'Disable'
  if (!((status & (1 << static_cast<uint8_t>(core_status::xaie_core_status_enable_bit))) || 
      (status & (1 << static_cast<uint8_t>(core_status::xaie_core_status_reset_bit))))) {
    status_vec.push_back("Disable");
  }

  for (auto flag = static_cast<u32>(core_status::xaie_core_status_enable_bit); 
      flag < static_cast<u32>(core_status::xaie_core_status_max_bit); flag++) {
    // check for set bits
    if (status & (1 << flag))
      status_vec.push_back(status_map[flag]);
  }

  return status_vec;
}

/* Functions related to Core Tile */
uint64_t
aie_core_tile_status::
size(const aie_tiles_info& info)
{
  return sizeof(aie_dma_status) * info.core_dma_channels + sizeof(uint32_t) * info.core_events * 2 /*core, mem mode*/ +
      sizeof(uint8_t) * info.core_locks + sizeof(uint32_t) * 4 /*(cs,pc,sp,lr)*/;
}

// Convert Raw buffer data received from driver to core tile status
// Sanity checks on buffer size is done in previous calls, so there will be no overflow
// Buffer format: 
//        Multiple columns with core, mem, shim tiles information
// +-----------------------------------+
// | core rows | mem rows | shim rows  |  col 0
// |-----------------------------------|
// | core rows | mem rows | shim rows  |  col 1
// |-----------------------------------|
// |          .........                |  col N
// +-----------------------------------+
static void
parse_core_tile_buf(const std::vector<char>& raw_buf, const aie_tiles_info& info, std::vector<aie_tiles_status>& aie_status)
{
  auto buf = raw_buf.data();

  for (auto &aie_col_status : aie_status) {
    for (auto &core : aie_col_status.core_tiles) {
      // DMA status
      uint32_t size_cal = sizeof(aie_dma_status) * info.core_dma_channels;
      std::memcpy(core.dma.data(), buf, size_cal);
      buf += size_cal;

      // Events
      // core mode events
      size_cal = sizeof(uint32_t) * info.core_events;
      std::memcpy(core.core_mode_events.data(), buf, size_cal);
      buf += size_cal;
      // mem mode events
      size_cal = sizeof(uint32_t) * info.core_events;
      std::memcpy(core.mem_mode_events.data(), buf, size_cal);
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
    buf = buf + aie_mem_tile_status::size(info) * info.mem_rows +
        aie_shim_tile_status::size(info) * info.shim_rows;
  }
}

static bpt::ptree
get_core_tile_info(const aie_core_tile_status& core)
{
  bpt::ptree pt;
  bpt::ptree core_pt;
  bpt::ptree status_array;

  for (const auto& status_str : core_status_to_string_array(core.core_status)) {
    if (status_str.empty())
      continue;
    bpt::ptree status_pt;
    status_pt.put("", status_str);
    status_array.push_back(std::make_pair("", status_pt));
  }
  core_pt.add_child("status", status_array);

  core_pt.put("pc", (boost::format("0x%08x") % core.program_counter).str());
  core_pt.put("sp", (boost::format("0x%08x") % core.stack_ptr).str());
  core_pt.put("lr", (boost::format("0x%08x") % core.link_reg).str());

  pt.add_child("core", core_pt);
  pt.add_child("dma", populate_dma(core.dma, aie_tile_type::core));
  pt.add_child("locks", populate_locks(core.lock_value));

  return pt;
}

/* Functions related to Mem Tile */
uint64_t
aie_mem_tile_status::
size(const aie_tiles_info& info)
{
  return sizeof(aie_dma_status) * info.mem_dma_channels + sizeof(uint32_t) * info.mem_events +
      sizeof(uint8_t) * info.mem_locks;
}

// Convert Raw buffer data received from driver to mem tile status
// Sanity checks on buffer size is done in previous calls, so there will be no overflow
static void
parse_mem_tile_buf(const std::vector<char>& raw_buf, const aie_tiles_info& info, std::vector<aie_tiles_status>& aie_status)
{
  auto buf = raw_buf.data();

  for (auto &aie_col_status : aie_status) {
    // add core tiles offset
    buf += aie_core_tile_status::size(info) * info.core_rows;

    for (auto &mem : aie_col_status.mem_tiles) {
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

static bpt::ptree
get_mem_tile_info(const aie_mem_tile_status& mem)
{
  bpt::ptree pt;

  // fill DMA status
  auto dma_pt = populate_dma(mem.dma, aie_tile_type::mem);
  pt.add_child("dma", dma_pt);

  //fill Lock's info
  auto lock_pt = populate_locks(mem.lock_value);
  pt.add_child("locks", lock_pt);

  return pt;
}

/* Functions related to Shim Tile */
uint64_t
aie_shim_tile_status::
size(const aie_tiles_info& info)
{
  return sizeof(aie_dma_status) * info.shim_dma_channels + sizeof(uint32_t) * info.shim_events +
      sizeof(uint8_t) * info.shim_locks;
}

// Convert Raw buffer data received from driver to shim tile status
// Sanity checks on buffer size is done in previous calls, so there will be no overflow
static void
parse_shim_tile_buf(const std::vector<char>& raw_buf, const aie_tiles_info& info, std::vector<aie_tiles_status>& aie_status)
{
  auto buf = raw_buf.data();

  for (auto &aie_col_status : aie_status) {
    // add core tiles and mem tiles offsets
    buf = buf + aie_core_tile_status::size(info) * info.core_rows +
        aie_mem_tile_status::size(info) * info.mem_rows;
    for (auto &shim : aie_col_status.shim_tiles) {
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

static bpt::ptree
get_shim_tile_info(const aie_shim_tile_status& shim)
{
  bpt::ptree pt;

  // fill DMA status
  auto dma_pt = populate_dma(shim.dma, aie_tile_type::shim);
  pt.add_child("dma", dma_pt);

  //fill Lock's info
  auto lock_pt = populate_locks(shim.lock_value);
  pt.add_child("locks", lock_pt);

  return pt;
}

/* Common functions*/
static void
aie_status_version_check(uint16_t major_ver, uint16_t minor_ver)
{
  if (!((major_ver == asd_parser::aie_status_version_major) && (minor_ver == asd_parser::aie_status_version_minor)))
    throw std::runtime_error("Aie status version mismatch");
}

static void
aie_info_sanity_check(const aie_tiles_info& info)
{
  if (info.col_size == 0)
    throw std::runtime_error("Getting Aie column size info from driver failed\n");

  // calculate single column size using aie_tiles_info
  uint64_t calculated_size = aie_core_tile_status::size(info) * info.core_rows +
      aie_shim_tile_status::size(info) * info.shim_rows + aie_mem_tile_status::size(info) * info.mem_rows;

  // check calucalted size is same as size info from driver
  if (calculated_size != info.col_size)
    throw std::runtime_error("calculated size doesnot match size information from driver, version mismatch\n");
}

boost::property_tree::ptree
get_formated_tiles_info(const xrt_core::device* device, aie_tile_type tile_type)
{
  aie_tiles_info info{0};
  uint32_t cols_filled = 0;

  return get_formated_tiles_info(device, tile_type, info, cols_filled);
}

struct aie_status {
  std::vector<aie_tiles_status> status;
  uint32_t columns_filled;
};

static aie_status
get_aie_data(const xrt_core::device* device, const aie_tiles_info& info, aie_tile_type tile_type)
{
  xrt_core::query::aie_tiles_status_info::parameters arg{0};
  arg.max_num_cols = info.cols;
  arg.col_size = info.col_size;

  auto tiles_status = xrt_core::device_query<xrt_core::query::aie_tiles_status_info>(device, arg);
  if (tiles_status.cols_filled == 0)
    throw std::runtime_error("No open HW-Context\n");

  std::vector<asd_parser::aie_tiles_status> aie_status;

  // Allocate an entry for each active column
  // See core/xrt/src/runtime_src/core/common/design_notes.md entry 1
  uint32_t cols_filled = tiles_status.cols_filled;
  while (cols_filled) {
    if (cols_filled & 0x1)
      aie_status.emplace_back(info);

    cols_filled >>= 1;
  }

  switch (tile_type) {
    case aie_tile_type::core:
      parse_core_tile_buf(tiles_status.buf, info, aie_status);
      break;
    case aie_tile_type::shim:
      parse_shim_tile_buf(tiles_status.buf, info, aie_status);
      break;
    case aie_tile_type::mem:
      parse_mem_tile_buf(tiles_status.buf, info, aie_status);
      break;
    default :
      throw std::runtime_error("Unknown tile type in formatting Aie tiles status info");
  }

  return {.status = aie_status, .columns_filled = tiles_status.cols_filled};
}

static bpt::ptree
format_status(const xrt_core::device* device, const aie_tiles_info& info, const aie_tile_type tile_type)
{
  bpt::ptree pt_aie_core;
  bpt::ptree pt_cols;

  const auto aie_data = get_aie_data(device, info, tile_type);

  uint32_t active_columns = 0;
  for (uint16_t col = 0; col < info.cols; col++) {
    bpt::ptree pt_col;

    // See core/xrt/src/runtime_src/core/common/design_notes.md entry 1
    pt_col.put("col", col);
    if (!(aie_data.columns_filled & (1 << col))) {
      pt_col.put("status", "inactive");
      pt_cols.push_back(std::make_pair("", pt_col));
      continue;
    }
    pt_col.put("status", "active");

    bpt::ptree pt_tiles;
    for (uint16_t row = 0; row < info.get_tile_count(tile_type); row++) {
      bpt::ptree pt_tile;
      switch (tile_type) {
        case aie_tile_type::core:
          pt_tile = get_core_tile_info(aie_data.status[active_columns].core_tiles[row]);
          break;
        case aie_tile_type::shim:
          pt_tile = get_shim_tile_info(aie_data.status[active_columns].shim_tiles[row]);
          break;
        case aie_tile_type::mem:
          pt_tile = get_mem_tile_info(aie_data.status[active_columns].mem_tiles[row]);
          break;
      }
      pt_tile.put("row", row + info.get_tile_start(tile_type));

      pt_tiles.push_back(std::make_pair("", pt_tile));
    }
    pt_col.add_child("tiles", pt_tiles);
    pt_cols.push_back(std::make_pair("", pt_col));

    active_columns++;
  }

  pt_aie_core.add_child("columns", pt_cols);
  return pt_aie_core;
}

boost::property_tree::ptree
get_formated_tiles_info(const xrt_core::device* device, aie_tile_type tile_type, aie_tiles_info& info,
                        uint32_t& cols_filled)
{
  boost::property_tree::ptree pt;
  try {
    auto version = xrt_core::device_query<xrt_core::query::aie_status_version>(device);
    aie_status_version_check(version.major, version.minor);

    info = xrt_core::device_query<xrt_core::query::aie_tiles_stats>(device);
    if (!((info.major == asd_parser::aie_tiles_info_version_major) && (info.minor == asd_parser::aie_tiles_info_version_minor)))
      throw std::runtime_error("version mismatch for aie_tiles_info structure");

    aie_info_sanity_check(info);

    pt = format_status(device, info, tile_type);
  }
  catch (const std::exception&) {
    return pt;
  }
  return pt;
}
}

