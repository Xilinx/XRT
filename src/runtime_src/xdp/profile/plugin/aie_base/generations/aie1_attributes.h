// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef AIE1_ATTRIBUTES_H_
#define AIE1_ATTRIBUTES_H_

namespace aie1 
{

// Version-Specific Constants
// ###################################

// Hardware generation
const unsigned int hw_gen = 1;
// Tile architecture (used to determine broadcast direction)
const char * const tile_arch = "checkerboard";
// Total number of rows/columns in AIE array
const unsigned int max_rows = 8;
const unsigned int max_cols = 50;
// First row of AIE tiles
// NOTE: row 0 is interface tiles
const unsigned int row_offset = 1;
// Base address of AIE array
const unsigned long long aie_base = 0x20000000000;
// Tile stride (used in addressing)
const unsigned int tile_stride = 0x40000;
// AIE Clock frequency (in MHz)
const unsigned int clock_freq_mhz = 1250;
// Bit widths
const unsigned int stream_bit_width = 32;
const unsigned int cascade_bit_width = 384;
// Trace events per module/tile
const unsigned int num_trace_events = 8;
// Counters per module/tile
const unsigned int cm_num_counters = 4;
const unsigned int mm_num_counters = 2;
const unsigned int mm_num_counter_events = 2;
const unsigned int shim_num_counters = 2;
const unsigned int mem_num_counters = 0;
// Broadcast channels per module/tile
const unsigned int cm_num_broadcasts = 16;
const unsigned int mm_num_broadcasts = 16;
const unsigned int shim_num_braodcasts = 16;
const unsigned int mem_num_broadcasts = 16;
// Stream switch event ports per module/tile
const unsigned int cm_num_ss_event_ports = 8;
const unsigned int shim_num_ss_event_ports = 8;
const unsigned int mem_num_ss_event_ports = 0;
// Event status registers (i.e., sticky bits)
const unsigned int cm_num_event_status_regs = 4;
const unsigned int mm_num_event_status_regs = 4;
const unsigned int shim_num_event_status_regs = 4;
const unsigned int mem_num_event_status_regs = 0;
// Microcontrollers (uC) per interface tile
const unsigned int shim_num_uc = 0;
// BD metadata per tile
const unsigned int mm_num_bds = 16;
const unsigned int mm_num_bd_regs = 7;
const unsigned int shim_num_bds = 0;
const unsigned int shim_num_bd_regs = 0;
const unsigned int mem_num_bds = 0;
const unsigned int mem_num_bd_regs = 0;
// Locks per tile
const unsigned int mm_num_locks = 16;
const unsigned int shim_num_locks = 16;
const unsigned int mem_num_locks = 0;
// Group events
const unsigned int cm_num_group_events = 9;
const unsigned int cm_group_core_stall_index = 3;
const unsigned int cm_group_program_flow_index = 4;
const unsigned int cm_group_stream_switch_index = 7;
// Event masks
const unsigned int shim_event_mask = 0x7f;

// Version-Specific Event IDs
// ###################################

const unsigned int cm_event_none = 0;
const unsigned int cm_event_true = 1;
const unsigned int cm_event_perf_count_0 = 5;
const unsigned int cm_event_perf_count_1 = 6;
const unsigned int cm_event_perf_count_2 = 7;
const unsigned int cm_event_perf_count_3 = 8;
const unsigned int cm_event_combo_event_0 = 9;
const unsigned int cm_event_combo_event_1 = 10;
const unsigned int cm_event_combo_event_2 = 11;
const unsigned int cm_event_combo_event_3 = 12;
const unsigned int cm_event_group_core_stall = 22;
const unsigned int cm_event_memory_stall = 23;
const unsigned int cm_event_stream_stall = 24;
const unsigned int cm_event_cascade_stall = 25;
const unsigned int cm_event_lock_stall = 26;
const unsigned int cm_event_active = 28;
const unsigned int cm_event_disabled = 29;
const unsigned int cm_event_group_core_program_flow = 32;
const unsigned int cm_event_instr_event_0 = 33;
const unsigned int cm_event_instr_event_1 = 34;
const unsigned int cm_event_instr_call = 35;
const unsigned int cm_event_instr_return = 36;
const unsigned int cm_event_instr_vector = 37;
const unsigned int cm_event_instr_load = 38;
const unsigned int cm_event_instr_store = 39;
const unsigned int cm_event_instr_stream_get = 40;
const unsigned int cm_event_instr_stream_put = 41;
const unsigned int cm_event_instr_cascade_get = 42;
const unsigned int cm_event_instr_cascade_put = 43;
const unsigned int cm_event_fp_huge = 50;
const unsigned int cm_event_fp_tiny = 51;
const unsigned int cm_event_fp_invalid = 52;
const unsigned int cm_event_fp_infinity = 53;
const unsigned int cm_event_group_stream_switch = 73;
const unsigned int cm_event_port_idle_0 = 74;
const unsigned int cm_event_port_running_0 = 75;
const unsigned int cm_event_port_stalled_0 = 76;
const unsigned int cm_event_port_tlast_0 = 77;
const unsigned int cm_event_port_idle_1 = 78;
const unsigned int cm_event_port_running_1 = 79;
const unsigned int cm_event_port_stalled_1 = 80;
const unsigned int cm_event_port_tlast_1 = 81;
const unsigned int cm_event_port_idle_2 = 82;
const unsigned int cm_event_port_running_2 = 83;
const unsigned int cm_event_port_stalled_2 = 84;
const unsigned int cm_event_port_tlast_2 = 85;
const unsigned int cm_event_port_idle_3 = 86;
const unsigned int cm_event_port_running_3 = 87;
const unsigned int cm_event_port_stalled_3 = 88;
const unsigned int cm_event_port_tlast_3 = 89;
const unsigned int cm_event_port_idle_4 = 90;
const unsigned int cm_event_port_running_4 = 91;
const unsigned int cm_event_port_stalled_4 = 92;
const unsigned int cm_event_port_tlast_4 = 93;
const unsigned int cm_event_port_idle_5 = 94;
const unsigned int cm_event_port_running_5 = 95;
const unsigned int cm_event_port_stalled_5 = 96;
const unsigned int cm_event_port_tlast_5 = 97;
const unsigned int cm_event_port_idle_6 = 98;
const unsigned int cm_event_port_running_6 = 99;
const unsigned int cm_event_port_stalled_6 = 100;
const unsigned int cm_event_port_tlast_6 = 101;
const unsigned int cm_event_port_idle_7 = 102;
const unsigned int cm_event_port_running_7 = 103;
const unsigned int cm_event_port_stalled_7 = 104;
const unsigned int cm_event_port_tlast_7 = 105;
const unsigned int cm_event_broadcast_0 = 107;
const unsigned int cm_event_broadcast_1 = 108;
const unsigned int cm_event_broadcast_2 = 109;
const unsigned int cm_event_broadcast_3 = 110;
const unsigned int cm_event_broadcast_4 = 111;
const unsigned int cm_event_broadcast_5 = 112;
const unsigned int cm_event_broadcast_6 = 113;
const unsigned int cm_event_broadcast_7 = 114;
const unsigned int cm_event_broadcast_8 = 115;
const unsigned int cm_event_broadcast_9 = 116;
const unsigned int cm_event_broadcast_10 = 117;
const unsigned int cm_event_broadcast_11 = 118;
const unsigned int cm_event_broadcast_12 = 119;
const unsigned int cm_event_broadcast_13 = 120;
const unsigned int cm_event_broadcast_14 = 121;
const unsigned int cm_event_broadcast_15 = 122;
const unsigned int cm_event_user_event_0 = 124;
const unsigned int cm_event_user_event_1 = 125;
const unsigned int cm_event_user_event_2 = 126;
const unsigned int cm_event_user_event_3 = 127;

const unsigned int mm_event_perf_count_0 = 5;
const unsigned int mm_event_perf_count_1 = 6;
const unsigned int mm_event_group_dma_activity = 20;
const unsigned int mm_event_dma_finish_bd_s2mm_chan0 = 25;
const unsigned int mm_event_dma_finish_bd_s2mm_chan1 = 26;
const unsigned int mm_event_dma_finish_bd_mm2s_chan0 = 27;
const unsigned int mm_event_dma_finish_bd_mm2s_chan1 = 28;
const unsigned int mm_event_dma_stall_s2mm_chan0 = 33;
const unsigned int mm_event_dma_stall_s2mm_chan1 = 34;
const unsigned int mm_event_dma_stall_mm2s_chan0 = 35;
const unsigned int mm_event_dma_stall_mm2s_chan1 = 36;
const unsigned int mm_event_group_lock = 43;
const unsigned int mm_event_group_memory_conflict = 76;
const unsigned int mm_event_group_error = 86;
const unsigned int mm_event_broadcast_14 = 121;
const unsigned int mm_event_broadcast_15 = 122;

const unsigned int shim_event_perf_count_0 = 5;
const unsigned int shim_event_perf_count_1 = 6;
const unsigned int shim_event_combo_event_3 = 10;
const unsigned int shim_event_group_dma_activity = 11;
const unsigned int shim_event_dma_s2mm_0_start_bd = 12;
const unsigned int shim_event_dma_s2mm_1_start_bd = 13;
const unsigned int shim_event_dma_mm2s_0_start_bd = 14;
const unsigned int shim_event_dma_mm2s_1_start_bd = 15;
const unsigned int shim_event_dma_s2mm_0_finished_bd = 16;
const unsigned int shim_event_dma_s2mm_1_finished_bd = 17;
const unsigned int shim_event_dma_mm2s_0_finished_bd = 18;
const unsigned int shim_event_dma_mm2s_1_finished_bd = 19;
const unsigned int shim_event_dma_s2mm_0_stalled_lock = 24;
const unsigned int shim_event_dma_s2mm_1_stalled_lock = 25;
const unsigned int shim_event_dma_mm2s_0_stalled_lock = 26;
const unsigned int shim_event_dma_mm2s_1_stalled_lock = 27;
const unsigned int shim_event_port_idle_0 = 74;
const unsigned int shim_event_port_running_0 = 75;
const unsigned int shim_event_port_stalled_0 = 76;
const unsigned int shim_event_port_tlast_0 = 77;
const unsigned int shim_event_port_idle_1 = 78;
const unsigned int shim_event_port_running_1 = 79;
const unsigned int shim_event_port_stalled_1 = 80;
const unsigned int shim_event_port_tlast_1 = 81;
const unsigned int shim_event_port_idle_2 = 82;
const unsigned int shim_event_port_running_2 = 83;
const unsigned int shim_event_port_stalled_2 = 84;
const unsigned int shim_event_port_tlast_2 = 85;
const unsigned int shim_event_port_idle_3 = 86;
const unsigned int shim_event_port_running_3 = 87;
const unsigned int shim_event_port_stalled_3 = 88;
const unsigned int shim_event_port_tlast_3 = 89;
const unsigned int shim_event_port_idle_4 = 90;
const unsigned int shim_event_port_running_4 = 91;
const unsigned int shim_event_port_stalled_4 = 92;
const unsigned int shim_event_port_tlast_4 = 93;
const unsigned int shim_event_port_idle_5 = 94;
const unsigned int shim_event_port_running_5 = 95;
const unsigned int shim_event_port_stalled_5 = 96;
const unsigned int shim_event_port_tlast_5 = 97;
const unsigned int shim_event_port_idle_6 = 98;
const unsigned int shim_event_port_running_6 = 99;
const unsigned int shim_event_port_stalled_6 = 100;
const unsigned int shim_event_port_tlast_6 = 101;
const unsigned int shim_event_port_idle_7 = 102;
const unsigned int shim_event_port_running_7 = 103;
const unsigned int shim_event_port_stalled_7 = 104;
const unsigned int shim_event_port_tlast_7 = 105;
const unsigned int shim_event_broadcast_0 = 107;
const unsigned int shim_event_broadcast_1 = 108;
const unsigned int shim_event_broadcast_2 = 109;
const unsigned int shim_event_broadcast_3 = 110;
const unsigned int shim_event_broadcast_4 = 111;
const unsigned int shim_event_broadcast_5 = 112;
const unsigned int shim_event_broadcast_6 = 113;
const unsigned int shim_event_broadcast_7 = 114;
const unsigned int shim_event_broadcast_8 = 115;
const unsigned int shim_event_broadcast_9 = 116;
const unsigned int shim_event_broadcast_10 = 117;
const unsigned int shim_event_broadcast_11 = 118;
const unsigned int shim_event_broadcast_12 = 119;
const unsigned int shim_event_broadcast_13 = 120;
const unsigned int shim_event_broadcast_14 = 121;
const unsigned int shim_event_broadcast_15 = 122;
const unsigned int shim_event_user_event_0 = 124;
const unsigned int shim_event_user_event_1 = 125;

// Version-Specific Port Indices
// ###################################

const unsigned int cm_dma_channel0_port_index = 2;
const unsigned int cm_dma_channel1_port_index = 3;
const unsigned int cm_core_trace_slave_port_index = 25;
const unsigned int cm_mem_trace_slave_port_index = 26;

const unsigned int shim_south0_slave_port_index = 3;
const unsigned int shim_south0_master_port_index = 3;
const unsigned int shim_north0_slave_port_index = 15;
const unsigned int shim_north0_master_port_index = 13;

} // namespace aie1

#endif /* AIE1_ATTRIBUTES_H_ */
