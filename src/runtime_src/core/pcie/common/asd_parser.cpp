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

#include "asd_parser.h"
#include <iostream>

namespace asd_parser {
  namespace bpt = boost::property_tree;

  static std::vector<std::string> status_map;
  static std::vector<std::string> dma_s2mm_map;
  static std::vector<std::string> dma_mm2s_map;

  static void
  initialize_mapping()
  {
    // core status
    status_map.resize(uint32_t(core_status::XAIE_CORE_STATUS_MAX_BIT));
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_ENABLE_BIT)] = "Enable";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_RESET_BIT)] = "Reset";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_MEM_STALL_S_BIT)] = "Memory_Stall_S";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_MEM_STALL_W_BIT)] = "Memory_Stall_W";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_MEM_STALL_N_BIT)] = "Memory_Stall_N";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_MEM_STALL_E_BIT)] = "Memory_Stall_E";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_LOCK_STALL_S_BIT)] = "Lock_Stall_S";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_LOCK_STALL_W_BIT)] = "Lock_Stall_W";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_LOCK_STALL_N_BIT)] = "Lock_Stall_N";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_LOCK_STALL_E_BIT)] = "Lock_Stall_E";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_STREAM_STALL_SS0_BIT)] = "Stream_Stall_SS0";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_STREAM_STALL_MS0_BIT)] = "Stream_Stall_MS0";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_CASCADE_STALL_SCD_BIT)] = "Cascade_Stall_SCD";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_CASCADE_STALL_MCD_BIT)] = "Cascade_Stall_MCD";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_DEBUG_HALT_BIT)] = "Debug_Halt";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_ECC_ERROR_STALL_BIT)] = "ECC_Error_Stall";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_ECC_SCRUBBING_STALL_BIT)] = "ECC_Scrubbing_Stall";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_ERROR_HALT_BIT)] = "Error_Halt";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_DONE_BIT)] = "Core_Done";
    status_map[uint32_t(core_status::XAIE_CORE_STATUS_PROCESSOR_BUS_STALL_BIT)] = "Core_Proc_Bus_Stall";

    // DMA s2mm status
    dma_s2mm_map.resize(uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_MAX));
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_STATUS)] = "Status";
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_STALLED_LOCK_ACK)] = "Stalled_Lock_Acq";
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_STALLED_LOCK_REL)] = "Stalled_Lock_Rel";
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_STALLED_STREAM_STARVATION)] = "Stalled_Stream_Starvation";
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_STALLED_TCT_OR_COUNT_FIFO_FULL)] = "Stalled_TCT_Or_Count_FIFO_Full";
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_ERROR_LOCK_ACCESS_TO_UNAVAIL)] = "Error_Lock_Access_Unavail",
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_ERROR_DM_ACCESS_TO_UNAVAIL)] = "Error_DM_Access_Unavail",
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_ERROR_BD_UNAVAIL)] = "Error_BD_Unavail";
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_ERROR_BD_INVALID)] = "Error_BD_Invalid";
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_ERROR_FOT_LENGTH)] = "Error_FoT_Length";
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_ERROR_FOT_BDS_PER_TASK)] = "Error_Fot_BDs";
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_AXI_MM_DECODE_ERROR)] = "AXI-MM_decode_error";
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_AXI_MM_SLAVE_ERROR)] = "AXI-MM_slave_error";
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_TASK_QUEUE_OVERFLOW)] = "Task_Queue_Overflow";
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_CHANNEL_RUNNING)] = "Channel_Running";
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_TASK_QUEUE_SIZE)] = "Task_Queue_Size";
    dma_s2mm_map[uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_CURRENT_BD)] = "Cur_BD";

    // DMA mm2s status
    dma_mm2s_map.resize(uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_MAX));
    dma_mm2s_map[uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_STATUS)] = "Status";
    dma_mm2s_map[uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_STALLED_LOCK_ACK)] = "Stalled_Lock_Acq";
    dma_mm2s_map[uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_STALLED_LOCK_REL)] = "Stalled_Lock_Rel";
    dma_mm2s_map[uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_STALLED_STREAM_BACKPRESSURE)] = "Stalled_Stream_Back_Pressure";
    dma_mm2s_map[uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_STALLED_TCT)] = "Stalled_TCT";
    dma_mm2s_map[uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_ERROR_LOCK_ACCESS_TO_UNAVAIL)] = "Error_Lock_Access_Unavail";
    dma_mm2s_map[uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_ERROR_DM_ACCESS_TO_UNAVAIL)] = "Error_DM_Access_Unavail";
    dma_mm2s_map[uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_ERROR_BD_UNAVAIL)] = "Error_BD_Unavail";
    dma_mm2s_map[uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_ERROR_BD_INVALID)] = "Error_BD_Invalid";
    dma_mm2s_map[uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_AXI_MM_DECODE_ERROR)] = "AXI-MM_decode_error";
    dma_mm2s_map[uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_AXI_MM_SLAVE_ERROR)] = "AXI-MM_slave_error";
    dma_mm2s_map[uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_TASK_QUEUE_OVERFLOW)] = "Task_Queue_Overflow";
    dma_mm2s_map[uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_CHANNEL_RUNNING)] = "Channel_Running";
    dma_mm2s_map[uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_TASK_QUEUE_SIZE)] = "Task_Queue_Size";
    dma_mm2s_map[uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_CURRENT_BD)] = "Cur_BD";
  }

  struct initialize { 
    initialize() { initialize_mapping(); }
  };
  static initialize obj;

  static const uint8_t DMA_CHANNEL_STATUS = 0x3;
  static const uint8_t DMA_QUEUE_OVERFLOW = 0x1;
  static const uint8_t DMA_QUEUE_SIZE = 0x7;
  static const uint8_t DMA_CURRENT_BD = 0x3F;
  static const uint8_t DMA_DEFAULT = 0x1;

  /* Internal Functions */
  static aie_dma_int 
  get_dma_mm2s_status(uint32_t status, aie_tile_type tile_type)
  {
    aie_dma_int dma_status_int;

    for (auto flag = uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_STATUS); 
        flag <= uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_MAX); flag++) {
      // Below is for bits  8, 9, 10 in DMA_MM2S mem tile
      if ((tile_type != aie_tile_type::mem) && 
          ((flag == uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_ERROR_LOCK_ACCESS_TO_UNAVAIL)) || 
          (flag == uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_ERROR_DM_ACCESS_TO_UNAVAIL)) || 
          (flag == uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_ERROR_BD_UNAVAIL))))
        continue;

      // Below is for bits 16, 17 in DMA_MM2S shim tile
      if ((tile_type != aie_tile_type::shim) && 
          ((flag == uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_AXI_MM_DECODE_ERROR)) || 
          (flag == uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_AXI_MM_SLAVE_ERROR))))
        continue;

      if (!dma_mm2s_map[flag].empty()) {
        uint32_t val = (status >> flag);

        switch (flag) {
          case uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_STATUS) :
            val &= DMA_CHANNEL_STATUS;
            if (val == 0)
              dma_status_int.channel_status.emplace_back("Idle");
            else if (val == 1)
              dma_status_int.channel_status.emplace_back("Starting");
            else if (val == 2)
              dma_status_int.channel_status.emplace_back("Running");
            else
              dma_status_int.channel_status.emplace_back("Invalid State");
            
            break;
          case uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_TASK_QUEUE_OVERFLOW) :
            val &= DMA_QUEUE_OVERFLOW;
            if (val == 0)
              dma_status_int.queue_status = "okay";
            else
              dma_status_int.queue_status = "channel_overflow";

            break;
          case uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_TASK_QUEUE_SIZE) :
            dma_status_int.queue_size = (val & DMA_QUEUE_SIZE);
            break;
          case uint32_t(dma_mm2s_status::XAIE_DMA_STATUS_MM2S_CURRENT_BD) :
            dma_status_int.current_bd = (val & DMA_CURRENT_BD);
            break;
          default :
            val &= DMA_DEFAULT;
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

    for (auto flag = uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_STATUS); 
        flag <= uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_MAX); flag++) {
      // Below is for bits  8, 9 in DMA_S2MM mem tile
      if ((tile_type != aie_tile_type::mem) && 
          ((flag == uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_ERROR_LOCK_ACCESS_TO_UNAVAIL)) || 
          (flag == uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_ERROR_DM_ACCESS_TO_UNAVAIL))))
        continue;

      // Below is for bits 16, 17 in DMA_S2MM shim tile
      if ((tile_type != aie_tile_type::shim) && 
          ((flag == uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_AXI_MM_DECODE_ERROR)) || 
          (flag == uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_AXI_MM_SLAVE_ERROR))))
        continue;

      if (!dma_s2mm_map[flag].empty()) {
        uint32_t val = (status >> flag);

        switch (flag) {
          case uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_STATUS) :
            val &= DMA_CHANNEL_STATUS;
            if (val == 0)
              dma_status_int.channel_status.emplace_back("Idle");
            else if (val == 1)
              dma_status_int.channel_status.emplace_back("Starting");
            else if (val == 2)
              dma_status_int.channel_status.emplace_back("Running");
            else
              dma_status_int.channel_status.emplace_back("Invalid State");
            
            break;
          case uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_TASK_QUEUE_OVERFLOW) :
            val &= DMA_QUEUE_OVERFLOW;
            if (val == 0)
              dma_status_int.queue_status = "okay";
            else
              dma_status_int.queue_status = "channel_overflow";

            break;
          case uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_CHANNEL_RUNNING) :
            dma_status_int.queue_size = (val & DMA_QUEUE_SIZE);
            break;
          case uint32_t(dma_s2mm_status::XAIE_DMA_STATUS_S2MM_CURRENT_BD) :
            dma_status_int.current_bd = (val & DMA_CURRENT_BD);
            break;
          default :
            val &= DMA_DEFAULT;
            if (val)
              dma_status_int.channel_status.emplace_back(dma_s2mm_map[flag]); 
        }
      }
    }

    return dma_status_int;
  }

  static void
  populate_dma(std::vector<aie_dma_status> dma, bpt::ptree &dma_pt, aie_tile_type tile_type)
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
  populate_locks(std::vector<uint8_t> locks, bpt::ptree &lock_pt)
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
  core_status_to_string_array(uint32_t status, std::vector<std::string> &status_vec)
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
    return sizeof(aie_dma_status) * Dma.size() + sizeof(uint32_t) * Events.size() +
        sizeof(uint8_t) * LockValue.size() + sizeof(uint32_t) * 4 /*(cs,pc,sp,lr)*/;
  }

  uint64_t
  aie_core_tile_status::size(aie_tiles_info &info) {
    return sizeof(aie_dma_status) * info.core_dma_channels + sizeof(uint32_t) * info.core_events +
        sizeof(uint8_t) * info.core_locks + sizeof(uint32_t) * 4 /*(cs,pc,sp,lr)*/;
  }

  void
  aie_core_tile_status::parse_buf(char *buf, aie_tiles_info &info, std::vector<std::unique_ptr<aie_col_status>> &aie_cols) {
    for (auto &aie_col : aie_cols) {
      for (auto &core : aie_col->CoreTile) {
        // DMA status
        uint32_t size_cal = sizeof(aie_dma_status) * info.core_dma_channels;
        std::memcpy(core.Dma.data(), buf, size_cal);
        buf += size_cal;

        // Events
        size_cal = sizeof(uint32_t) * info.core_events;
        std::memcpy(core.Events.data(), buf, size_cal);
        buf += size_cal;

        // core status
        size_cal = sizeof(uint32_t);
        std::memcpy(&core.CoreStatus, buf, size_cal);
        buf += size_cal;

        // program counter
        size_cal = sizeof(uint32_t);
        std::memcpy(&core.ProgramCounter, buf, size_cal);
        buf += size_cal;

        // stack ptr
        size_cal = sizeof(uint32_t);
        std::memcpy(&core.StackPtr, buf, size_cal);
        buf += size_cal;

        // link reg
        size_cal = sizeof(uint32_t);
        std::memcpy(&core.LinkReg, buf, size_cal);
        buf += size_cal;

        // Locks
        size_cal = sizeof(uint8_t) * info.core_locks;
        std::memcpy(core.LockValue.data(), buf, size_cal);
        buf += size_cal;
      }

      // add mem tiles and shim tiles offsets
      buf = buf + aie_mem_tile_status::size(info) * info.mem_rows + aie_shim_tile_status::size(info) * info.shim_rows;
    }
  }

  static void 
  get_core_tile_info(aie_core_tile_status &core, bpt::ptree &pt)
  {
    bpt::ptree core_pt;
    bpt::ptree status_array;
    bpt::ptree tmp;
    bpt::ptree tmp_array;
    std::vector<std::string> status_vec;
    
    core_status_to_string_array(core.CoreStatus, status_vec);
    for (auto &x : status_vec) {
      bpt::ptree status_pt;
      status_pt.put("", x);
      status_array.push_back(std::make_pair("", status_pt));
    }
    core_pt.add_child("status", status_array);

    // fill program counter as array
    tmp.put("", core.ProgramCounter);
    tmp_array.push_back(std::make_pair("", tmp));
    core_pt.add_child("pc", tmp_array);

    // fill stack pointer as array
    tmp_array = {};
    tmp.put("", core.StackPtr);
    tmp_array.push_back(std::make_pair("", tmp));
    core_pt.add_child("sp", tmp_array);

    // fill link register as array
    tmp_array = {};
    tmp.put("", core.LinkReg);
    tmp_array.push_back(std::make_pair("", tmp));
    core_pt.add_child("lr", tmp_array);

    pt.add_child("core", core_pt);

    // fill DMA status
    bpt::ptree dma_pt;
    populate_dma(core.Dma, dma_pt, aie_tile_type::core);
    pt.add_child("dma", dma_pt);

    //fill Lock's info
    bpt::ptree lock_pt;
    populate_locks(core.LockValue, lock_pt);
    pt.add_child("lock", lock_pt);
  }

  bpt::ptree
  aie_core_tile_status::format_status(std::vector<std::unique_ptr<aie_col_status>> &aie_cols,
                                      uint32_t start_col,
                                      uint32_t cols,
                                      aie_tiles_info &tiles_info)
  {
    bpt::ptree pt_array;

    for (uint32_t col = start_col; col < cols; col++) {
      for (uint32_t row = 0; row < tiles_info.core_rows; row++) {
        bpt::ptree pt;
        pt.put("col", col);
        pt.put("row", row + tiles_info.core_row_start);
          
        get_core_tile_info(aie_cols[col]->CoreTile[row], pt);
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
    return sizeof(aie_dma_status) * Dma.size() + sizeof(uint32_t) * Events.size() +
        sizeof(uint8_t) * LockValue.size();
  }

  uint64_t
  aie_mem_tile_status::size(aie_tiles_info &info) {
    return sizeof(aie_dma_status) * info.mem_dma_channels + sizeof(uint32_t) * info.mem_events +
        sizeof(uint8_t) * info.mem_locks;
  }

  void
  aie_mem_tile_status::parse_buf(char *buf, aie_tiles_info &info, std::vector<std::unique_ptr<aie_col_status>> &aie_cols)
  {
    for (auto &aie_col : aie_cols) {
      // add core tiles offset
      buf += aie_core_tile_status::size(info) * info.core_rows;

      for (auto &mem : aie_col->MemTile) {
        // DMA status
        uint32_t size_cal = sizeof(aie_dma_status) * info.mem_dma_channels;
        std::memcpy(mem.Dma.data(), buf, size_cal);
        buf += size_cal;

        // Events
        size_cal = sizeof(uint32_t) * info.mem_events;
        std::memcpy(mem.Events.data(), buf, size_cal);
        buf += size_cal;

        // Locks
        size_cal = sizeof(uint8_t) * info.mem_locks;
        std::memcpy(mem.LockValue.data(), buf, size_cal);
        buf += size_cal;
      }

      // add shim tiles offset
      buf += aie_shim_tile_status::size(info) * info.shim_rows;
    }
  }

  static void 
  get_mem_tile_info(aie_mem_tile_status &mem, bpt::ptree &pt)
  {
    // fill DMA status
    bpt::ptree dma_pt;
    populate_dma(mem.Dma, dma_pt, aie_tile_type::mem);
    pt.add_child("dma", dma_pt);

    //fill Lock's info
    bpt::ptree lock_pt;
    populate_locks(mem.LockValue, lock_pt);
    pt.add_child("lock", lock_pt);
  }

  bpt::ptree
  aie_mem_tile_status::format_status(std::vector<std::unique_ptr<aie_col_status>> &aie_cols,
                                      uint32_t start_col,
                                      uint32_t cols,
                                      aie_tiles_info &tiles_info)
  {
    bpt::ptree pt_array;
    
    for (uint32_t col = start_col; col < cols; col++) {
      for (uint32_t row = 0; row < tiles_info.mem_rows; row++) {
        bpt::ptree pt;
        pt.put("col", col);
        pt.put("row", row + tiles_info.mem_row_start);
        
        get_mem_tile_info(aie_cols[col]->MemTile[row], pt);
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
    return sizeof(aie_dma_status) * Dma.size() + sizeof(uint32_t) * Events.size() +
        sizeof(uint8_t) * LockValue.size();
  }

  uint64_t
  aie_shim_tile_status::size(aie_tiles_info &info) {
    return sizeof(aie_dma_status) * info.shim_dma_channels + sizeof(uint32_t) * info.shim_events +
        sizeof(uint8_t) * info.shim_locks;
  }

  void
  aie_shim_tile_status::parse_buf(char *buf, aie_tiles_info &info, std::vector<std::unique_ptr<aie_col_status>> &aie_cols)
  {
    for (auto &aie_col : aie_cols) {
      // add core tiles and mem tiles offsets
      buf = buf + aie_core_tile_status::size(info) * info.core_rows + aie_mem_tile_status::size(info) * info.mem_rows;

      for (auto &shim : aie_col->ShimTile) {
        // DMA status
        uint32_t size_cal = sizeof(aie_dma_status) * info.shim_dma_channels;
        std::memcpy(shim.Dma.data(), buf, size_cal);
        buf += size_cal;

        // Events
        size_cal = sizeof(uint32_t) * info.shim_events;
        std::memcpy(shim.Events.data(), buf, size_cal);
        buf += size_cal;

        // Locks
        size_cal = sizeof(uint8_t) * info.shim_locks;
        std::memcpy(shim.LockValue.data(), buf, size_cal);
        buf += size_cal;
      }
    }
  }

  static void 
  get_shim_tile_info(aie_shim_tile_status &shim, bpt::ptree &pt)
  {
    // fill DMA status
    bpt::ptree dma_pt;
    populate_dma(shim.Dma, dma_pt, aie_tile_type::shim);
    pt.add_child("dma", dma_pt);

    //fill Lock's info
    bpt::ptree lock_pt;
    populate_locks(shim.LockValue, lock_pt);
    pt.add_child("lock", lock_pt);
  }

  bpt::ptree
  aie_shim_tile_status::format_status(std::vector<std::unique_ptr<aie_col_status>> &aie_cols,
                                      uint32_t start_col,
                                      uint32_t cols,
                                      aie_tiles_info &tiles_info)
  {
    bpt::ptree pt_array;
    
    for (uint32_t col = start_col; col < cols; col++) {
      for (uint32_t row = 0; row < tiles_info.shim_rows; row++) {
        bpt::ptree pt;
        pt.put("col", col);
        pt.put("row", row + tiles_info.shim_row_start);
        
        get_shim_tile_info(aie_cols[col]->ShimTile[row], pt);
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
  aie_status_version_check(uint16_t &major_ver, uint16_t &minor_ver)
  {
    if (!((major_ver == asd_parser::AIE_STATUS_VERSION_MAJOR) && (minor_ver == asd_parser::AIE_STATUS_VERSION_MINOR)))
      throw std::runtime_error("Aie status version mismatch");
  }

  void
  aie_info_sanity_check(uint32_t start_col, uint32_t num_cols, aie_tiles_info &info)
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