// #######################################################################
// Copyright (c) 2024 AMD, Inc.  All rights reserved.
//
// This   document  contains  proprietary information  which   is
// protected by  copyright. All rights  are reserved. No  part of
// this  document may be photocopied, reproduced or translated to
// another  program  language  without  prior written  consent of
// XILINX Inc., San Jose, CA. 95124
//
// Xilinx, Inc.
// XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
// COURTESY TO YOU.  BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
/// ONE POSSIBLE   IMPLEMENTATION OF THIS FEATURE, APPLICATION OR
// STANDARD, XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION
// IS FREE FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE
// FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
// XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
// THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO
// ANY WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE
// FROM CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE.
//
// ######################################################################

#ifndef AIE2PS_ATTRIBUTES_H_
#define AIE2PS_ATTRIBUTES_H_

namespace aie2ps
{

// Version-Specific Constants
// ###################################

// Hardware generation
// See: https://gitenterprise.xilinx.com/ai-engine/aie-rt/blob/main/driver/src/global/xaiegbl_defs.h#L46
const unsigned int hw_gen = 5;
// Tile architecture (used to determine broadcast direction)
const char * const tile_arch = "grid";
// Total number of rows/columns in AIE array
// Note: See section 3.12 of AIE2ps arch spec
const unsigned int max_rows = 14;
const unsigned int max_cols = 60;
// First row of AIE tiles 
// NOTE: row 0 is interface tiles, and rows 1-2 are memory tiles
const unsigned int row_offset = 3;
// Base address of AIE array
const unsigned long long aie_base = 0x20000000000;
// Tile stride (used in addressing)
const unsigned int tile_stride = 0x100000;
// AIE Clock frequency (in MHz)
const unsigned int clock_freq_mhz = 1250;
// Bit widths
const unsigned int stream_bit_width = 64;
const unsigned int cascade_bit_width = 512;
// Trace events per module/tile
const unsigned int num_trace_events = 8;
// Counters per module/tile
const unsigned int cm_num_counters = 4;
const unsigned int mm_num_counters = 4;
const unsigned int mm_num_counter_events = 2;
const unsigned int shim_num_counters = 6;
const unsigned int mem_num_counters = 6;
const unsigned int uc_num_event_counters = 5;
const unsigned int uc_num_latency_counters = 1;
// Broadcast channels per module/tile
const unsigned int cm_num_broadcasts = 16;
const unsigned int mm_num_broadcasts = 16;
const unsigned int shim_num_braodcasts = 16;
const unsigned int mem_num_broadcasts = 16;
// Stream switch event ports per module/tile
const unsigned int cm_num_ss_event_ports = 8;
const unsigned int shim_num_ss_event_ports = 8;
const unsigned int mem_num_ss_event_ports = 8;
// Event status registers (i.e., sticky bits)
const unsigned int cm_num_event_status_regs = 4;
const unsigned int mm_num_event_status_regs = 4;
const unsigned int shim_num_event_status_regs = 8;
const unsigned int mem_num_event_status_regs = 6;
// Microcontrollers (uC) per interface tile
const unsigned int shim_num_uc = 1;
// BD metadata per tile
const unsigned int mm_num_bds = 16;
const unsigned int mm_num_bd_regs = 6;
const unsigned int shim_num_bds = 16;
const unsigned int shim_num_bd_regs = 9;
const unsigned int mem_num_bds = 48;
const unsigned int mem_num_bd_regs = 8;
const unsigned int mem_num_bd_s2mm_channels = 1;
const unsigned int mem_num_bd_mm2s_channels = 1;
// Locks per tile
const unsigned int mm_num_locks = 16;
const unsigned int shim_num_locks = 16;
const unsigned int mem_num_locks = 64;
// Group events
const unsigned int cm_num_group_events = 9;
const unsigned int cm_group_core_stall_index = 3;
const unsigned int cm_group_program_flow_index = 4;
const unsigned int cm_group_stream_switch_index = 7;
// Event masks
const unsigned int shim_event_mask = 0xff;

// Version-Specific Event IDs
// ###################################

// AIE tile core modules
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

// AIE tile memory modules
const unsigned int mm_event_perf_count_0 = 5;
const unsigned int mm_event_perf_count_1 = 6;
const unsigned int mm_event_group_dma_activity = 18;
const unsigned int mm_event_dma_finish_bd_s2mm_chan0 = 23;
const unsigned int mm_event_dma_finish_bd_s2mm_chan1 = 24;
const unsigned int mm_event_dma_finish_bd_mm2s_chan0 = 25;
const unsigned int mm_event_dma_finish_bd_mm2s_chan1 = 26;
const unsigned int mm_event_dma_stall_s2mm_chan0 = 31;
const unsigned int mm_event_dma_stall_s2mm_chan1 = 32;
const unsigned int mm_event_dma_stall_mm2s_chan0 = 33;
const unsigned int mm_event_dma_stall_mm2s_chan1 = 34;
const unsigned int mm_event_dma_stream_starvation_s2mm_chan0 = 35;
const unsigned int mm_event_dma_stream_starvation_s2mm_chan1 = 36;
const unsigned int mm_event_dma_stream_backpressure_mm2s_chan0 = 37;
const unsigned int mm_event_dma_stream_backpressure_mm2s_chan1 = 38;
const unsigned int mm_event_dma_memory_backpressure_s2mm_chan0 = 39;
const unsigned int mm_event_dma_memory_backpressure_s2mm_chan1 = 40;
const unsigned int mm_event_dma_memory_starvation_mm2s_chan0 = 41;
const unsigned int mm_event_dma_memory_starvation_mm2s_chan1 = 42;
const unsigned int mm_event_group_lock = 43;
const unsigned int mm_event_group_memory_conflict = 76;
const unsigned int mm_event_group_error = 86;
const unsigned int mm_event_broadcast_14 = 121;
const unsigned int mm_event_broadcast_15 = 122;

// Interface tiles - general
const unsigned int shim_event_perf_count_0 = 5;
const unsigned int shim_event_perf_count_1 = 6;
const unsigned int shim_event_combo_event_3 = 10;
const unsigned int shim_event_group_dma_activity = 13;
const unsigned int shim_event_dma_s2mm_0_start_task = 14;
const unsigned int shim_event_dma_s2mm_1_start_task = 15;
const unsigned int shim_event_dma_mm2s_0_start_task = 16;
const unsigned int shim_event_dma_mm2s_1_start_task = 17;
const unsigned int shim_event_dma_s2mm_0_finished_bd = 18;
const unsigned int shim_event_dma_s2mm_1_finished_bd = 19;
const unsigned int shim_event_dma_mm2s_0_finished_bd = 20;
const unsigned int shim_event_dma_mm2s_1_finished_bd = 21;
const unsigned int shim_event_dma_s2mm_0_finished_task = 22;
const unsigned int shim_event_dma_s2mm_1_finished_task = 23;
const unsigned int shim_event_dma_mm2s_0_finished_task = 24;
const unsigned int shim_event_dma_mm2s_1_finished_task = 25;
const unsigned int shim_event_dma_s2mm_0_stalled_lock = 26;
const unsigned int shim_event_dma_s2mm_1_stalled_lock = 27;
const unsigned int shim_event_dma_mm2s_0_stalled_lock = 28;
const unsigned int shim_event_dma_mm2s_1_stalled_lock = 29;
const unsigned int shim_event_dma_s2mm_0_stream_starvation = 30;
const unsigned int shim_event_dma_s2mm_1_stream_starvation = 31;
const unsigned int shim_event_dma_mm2s_0_stream_backpressure = 32;
const unsigned int shim_event_dma_mm2s_1_stream_backpressure = 33;
const unsigned int shim_event_dma_s2mm_0_memory_backpressure = 34;
const unsigned int shim_event_dma_s2mm_1_memory_backpressure = 35;
const unsigned int shim_event_dma_mm2s_0_memory_starvation = 36;
const unsigned int shim_event_dma_mm2s_1_memory_starvation = 37;
const unsigned int shim_event_port_idle_0 = 133;
const unsigned int shim_event_port_running_0 = 134;
const unsigned int shim_event_port_stalled_0 = 135;
const unsigned int shim_event_port_tlast_0 = 136;
const unsigned int shim_event_port_idle_1 = 137;
const unsigned int shim_event_port_running_1 = 138;
const unsigned int shim_event_port_stalled_1 = 139;
const unsigned int shim_event_port_tlast_1 = 140;
const unsigned int shim_event_port_idle_2 = 141;
const unsigned int shim_event_port_running_2 = 142;
const unsigned int shim_event_port_stalled_2 = 143;
const unsigned int shim_event_port_tlast_2 = 144;
const unsigned int shim_event_port_idle_3 = 145;
const unsigned int shim_event_port_running_3 = 146;
const unsigned int shim_event_port_stalled_3 = 147;
const unsigned int shim_event_port_tlast_3 = 148;
const unsigned int shim_event_port_idle_4 = 149;
const unsigned int shim_event_port_running_4 = 150;
const unsigned int shim_event_port_stalled_4 = 151;
const unsigned int shim_event_port_tlast_4 = 152;
const unsigned int shim_event_port_idle_5 = 153;
const unsigned int shim_event_port_running_5 = 154;
const unsigned int shim_event_port_stalled_5 = 155;
const unsigned int shim_event_port_tlast_5 = 156;
const unsigned int shim_event_port_idle_6 = 157;
const unsigned int shim_event_port_running_6 = 158;
const unsigned int shim_event_port_stalled_6 = 159;
const unsigned int shim_event_port_tlast_6 = 160;
const unsigned int shim_event_port_idle_7 = 161;
const unsigned int shim_event_port_running_7 = 162;
const unsigned int shim_event_port_stalled_7 = 163;
const unsigned int shim_event_port_tlast_7 = 164;
const unsigned int shim_event_broadcast_0 = 166;
const unsigned int shim_event_broadcast_1 = 167;
const unsigned int shim_event_broadcast_2 = 168;
const unsigned int shim_event_broadcast_3 = 169;
const unsigned int shim_event_broadcast_4 = 170;
const unsigned int shim_event_broadcast_5 = 171;
const unsigned int shim_event_broadcast_6 = 172;
const unsigned int shim_event_broadcast_7 = 173;
const unsigned int shim_event_broadcast_8 = 174;
const unsigned int shim_event_broadcast_9 = 175;
const unsigned int shim_event_broadcast_10 = 176;
const unsigned int shim_event_broadcast_11 = 177;
const unsigned int shim_event_broadcast_12 = 178;
const unsigned int shim_event_broadcast_13 = 179;
const unsigned int shim_event_broadcast_14 = 180;
const unsigned int shim_event_broadcast_15 = 181;
const unsigned int shim_event_user_event_0 = 182;
const unsigned int shim_event_user_event_1 = 183;

// Interface tiles - uC specific
const unsigned int shim_event_dma_dm2mm_start_task = 185; 
const unsigned int shim_event_dma_mm2dm_start_task = 186;
const unsigned int shim_event_dma_dm2mm_finished_bd = 187;
const unsigned int shim_event_dma_mm2dm_finished_bd = 188;
const unsigned int shim_event_dma_dm2mm_finished_task = 189;
const unsigned int shim_event_dma_mm2dm_finished_task = 190;
const unsigned int shim_event_dma_dm2mm_local_memory_starvation = 191;
const unsigned int shim_event_dma_dm2mm_remote_memory_backpressure = 192;
const unsigned int shim_event_dma_mm2dm_local_memory_backpressure = 193;
const unsigned int shim_event_dma_mm2dm_remote_memory_starvation = 194;
const unsigned int shim_event_group_uc_module_errors = 195;
const unsigned int shim_event_axi_mm_uc_core_master_decode_error = 196;
const unsigned int shim_event_axi_mm_uc_dma_master_decode_error = 197;
const unsigned int shim_event_axi_mm_uc_core_master_slave_error = 198;
const unsigned int shim_event_axi_mm_uc_dma_master_slave_error = 199;
const unsigned int shim_event_shim_event_dma_dm2mm_error = 200;
const unsigned int shim_event_shim_event_dma_mm2dm_error = 201;
const unsigned int shim_event_shim_event_pm_ecc_error_1bit = 202;
const unsigned int shim_event_pm_ecc_error_2bit = 203;
const unsigned int shim_event_private_dm_ecc_error_1bit = 204;
const unsigned int shim_event_private_dm_ecc_error_2bit = 205;
const unsigned int shim_event_shared_dm_ecc_error_1bit = 206;
const unsigned int shim_event_shared_dm_ecc_error_2bit = 207;
const unsigned int shim_event_group_uc_core_streams = 208;
const unsigned int shim_event_axis_master_idle = 209;
const unsigned int shim_event_axis_master_running = 210;
const unsigned int shim_event_axis_master_stalled = 211;
const unsigned int shim_event_axis_master_tlast = 212;
const unsigned int shim_event_axis_slave_idle = 213;
const unsigned int shim_event_axis_slave_running = 214;
const unsigned int shim_event_axis_slave_stalled = 215;
const unsigned int shim_event_axis_slave_tlast = 216;
const unsigned int shim_event_group_uc_core_program_flow = 217;
const unsigned int shim_event_uc_core_sleep = 218;
const unsigned int shim_event_uc_core_interrupt = 219;
const unsigned int shim_event_uc_core_debug_sys_rst = 220;
const unsigned int shim_event_uc_core_debug_wakeup = 221;
const unsigned int shim_event_uc_core_timer1_interrupt = 222;
const unsigned int shim_event_uc_core_timer2_interrupt = 223;
const unsigned int shim_event_uc_core_timer3_interrupt = 224;
const unsigned int shim_event_uc_core_timer4_interrupt = 225;
const unsigned int shim_event_uc_core_reg_write = 226;
const unsigned int shim_event_uc_core_exception_taken = 227;
const unsigned int shim_event_uc_core_jump_taken = 228;
const unsigned int shim_event_uc_core_jump_hit = 229;
const unsigned int shim_event_uc_core_data_read = 230;
const unsigned int shim_event_uc_core_data_write = 231;
const unsigned int shim_event_uc_core_pipeline_halted_debug = 232;
const unsigned int shim_event_uc_core_stream_get = 233;
const unsigned int shim_event_uc_core_stream_put = 234;

// MicroBlaze Debug Module (MDM)
const unsigned int uc_event_valid_instruction = 0;
const unsigned int uc_event_load_word = 1;
const unsigned int uc_event_load_halfword = 2;
const unsigned int uc_event_load_byte = 3;
const unsigned int uc_event_store_word = 4;
const unsigned int uc_event_store_halfword = 5;
const unsigned int uc_event_store_byte = 6;
const unsigned int uc_event_unconditional_branch = 7;
const unsigned int uc_event_taken_conditional_branch = 8;
const unsigned int uc_event_not_taken_conditional_branch = 9;
const unsigned int uc_event_load_execution_r1 = 16;
const unsigned int uc_event_store_execution_r1 = 17;
const unsigned int uc_event_logical_execution = 18;
const unsigned int uc_event_arithmetic_execution = 19;
const unsigned int uc_event_multiply_execution = 20;
const unsigned int uc_event_barrel_shift_execution = 21;
const unsigned int uc_event_shift_execution = 22;
const unsigned int uc_event_exception = 23;
const unsigned int uc_event_interrupt = 24;
const unsigned int uc_event_pipeline_stall_operand_fetch = 25;
const unsigned int uc_event_pipeline_stall_execute = 26;
const unsigned int uc_event_pipeline_stall_memory = 27;
const unsigned int uc_event_integer_divide = 28;
const unsigned int uc_event_floating_point = 29;
const unsigned int uc_event_clock_cycles = 30;
const unsigned int uc_event_immediate = 31;
const unsigned int uc_event_pattern_compare = 32;
const unsigned int uc_event_sign_extend = 33;
const unsigned int uc_event_machine_status = 36;
const unsigned int uc_event_unconditional_branch_delay = 37;
const unsigned int uc_event_taken_conditional_branch_delay = 38;
const unsigned int uc_event_not_taken_conditional_branch_delay = 39;
const unsigned int uc_event_delay_slot = 40;
const unsigned int uc_event_load_execution = 41;
const unsigned int uc_event_store_execution = 42;
const unsigned int uc_event_mmu_data_access = 43;
const unsigned int uc_event_conditional_branch = 44;
const unsigned int uc_event_branch = 45;
const unsigned int uc_event_mmu_exception = 48;
const unsigned int uc_event_mmu_instruction_exception = 49;
const unsigned int uc_event_mmu_data_exception = 50;
const unsigned int uc_event_pipeline_stall = 51;
const unsigned int uc_event_mmu_side_access = 53;
const unsigned int uc_event_mmu_instruction_hit = 54;
const unsigned int uc_event_mmu_data_hit = 55;
const unsigned int uc_event_mmu_unified_hit = 56;
// The events below can be used with either event or latency counters
const unsigned int uc_event_interrupt_latency = 57;
const unsigned int uc_event_mmu_lookup_latency = 61;
const unsigned int uc_event_peripheral_data_read = 62;
const unsigned int uc_event_peripheral_data_write = 63;

// Memory tiles
const unsigned int mem_event_edge_detection_0 = 13;
const unsigned int mem_event_edge_detection_1 = 14;
const unsigned int mem_event_group_watchpoint = 15;
const unsigned int mem_event_dma_s2mm_sel0_start_task = 21;
const unsigned int mem_event_dma_s2mm_sel1_start_task = 22;
const unsigned int mem_event_dma_mm2s_sel0_start_task = 23;
const unsigned int mem_event_dma_mm2s_sel1_start_task = 24;
const unsigned int mem_event_dma_s2mm_sel0_finished_bd = 25;
const unsigned int mem_event_dma_s2mm_sel1_finished_bd = 26;
const unsigned int mem_event_dma_mm2s_sel0_finished_bd = 27;
const unsigned int mem_event_dma_mm2s_sel1_finished_bd = 28;
const unsigned int mem_event_dma_s2mm_sel0_finished_task = 29;
const unsigned int mem_event_dma_s2mm_sel1_finished_task = 30;
const unsigned int mem_event_dma_mm2s_sel0_finished_task = 31;
const unsigned int mem_event_dma_mm2s_sel1_finished_task = 32;
const unsigned int mem_event_dma_s2mm_sel0_stalled_lock = 33;
const unsigned int mem_event_dma_s2mm_sel1_stalled_lock = 34;
const unsigned int mem_event_dma_mm2s_sel0_stalled_lock = 35;
const unsigned int mem_event_dma_mm2s_sel1_stalled_lock = 36;
const unsigned int mem_event_dma_s2mm_sel0_stream_starvation = 37;
const unsigned int mem_event_dma_mm2s_sel0_stream_backpressure = 39;
const unsigned int mem_event_dma_s2mm_sel0_memory_backpressure = 41;
const unsigned int mem_event_dma_mm2s_sel0_memory_starvation = 43;
const unsigned int mem_event_dma_mm2s_sel1_memory_starvation = 44;
const unsigned int mem_event_group_lock = 45;
const unsigned int mem_event_port_idle_0 = 79;
const unsigned int mem_event_port_running_0 = 80;
const unsigned int mem_event_port_stalled_0 = 81;
const unsigned int mem_event_port_tlast_0 = 82;
const unsigned int mem_event_port_idle_1 = 83;
const unsigned int mem_event_port_running_1 = 84;
const unsigned int mem_event_port_stalled_1 = 85;
const unsigned int mem_event_port_tlast_1 = 86;
const unsigned int mem_event_port_idle_2 = 87;
const unsigned int mem_event_port_running_2 = 88;
const unsigned int mem_event_port_stalled_2 = 89;
const unsigned int mem_event_port_tlast_2 = 90;
const unsigned int mem_event_port_idle_3 = 91;
const unsigned int mem_event_port_running_3 = 92;
const unsigned int mem_event_port_stalled_3 = 93;
const unsigned int mem_event_port_tlast_3 = 94;
const unsigned int mem_event_port_idle_4 = 95;
const unsigned int mem_event_port_running_4 = 96;
const unsigned int mem_event_port_stalled_4 = 97;
const unsigned int mem_event_port_tlast_4 = 98;
const unsigned int mem_event_port_idle_5 = 99;
const unsigned int mem_event_port_running_5 = 100;
const unsigned int mem_event_port_stalled_5 = 101;
const unsigned int mem_event_port_tlast_5 = 102;
const unsigned int mem_event_port_idle_6 = 103;
const unsigned int mem_event_port_running_6 = 104;
const unsigned int mem_event_port_stalled_6 = 105;
const unsigned int mem_event_port_tlast_6 = 106;
const unsigned int mem_event_port_idle_7 = 107;
const unsigned int mem_event_port_running_7 = 108;
const unsigned int mem_event_port_stalled_7 = 109;
const unsigned int mem_event_port_tlast_7 = 110;
const unsigned int mem_event_group_memory_conflict = 111;
const unsigned int mem_event_memory_conflict_bank_0 = 112;
const unsigned int mem_event_memory_conflict_bank_1 = 113;
const unsigned int mem_event_memory_conflict_bank_2 = 114;
const unsigned int mem_event_memory_conflict_bank_3 = 115;
const unsigned int mem_event_memory_conflict_bank_4 = 116;
const unsigned int mem_event_memory_conflict_bank_5 = 117;
const unsigned int mem_event_memory_conflict_bank_6 = 118;
const unsigned int mem_event_memory_conflict_bank_7 = 119;
const unsigned int mem_event_memory_conflict_bank_8 = 120;
const unsigned int mem_event_memory_conflict_bank_9 = 121;
const unsigned int mem_event_memory_conflict_bank_10 = 122;
const unsigned int mem_event_memory_conflict_bank_11 = 123;
const unsigned int mem_event_memory_conflict_bank_12 = 124;
const unsigned int mem_event_memory_conflict_bank_13 = 125;
const unsigned int mem_event_memory_conflict_bank_14 = 126;
const unsigned int mem_event_memory_conflict_bank_15 = 127;
const unsigned int mem_event_group_errors = 128;
const unsigned int mem_event_user_event_0 = 159;
const unsigned int mem_event_user_event_1 = 160;

// Version-Specific Port Indices
// ###################################

const unsigned int cm_dma_channel0_port_index = 1;
const unsigned int cm_dma_channel1_port_index = 2;
const unsigned int cm_core_trace_slave_port_index = 23;
const unsigned int cm_mem_trace_slave_port_index = 24;

const unsigned int shim_south0_slave_port_index = 2;
const unsigned int shim_south0_master_port_index = 2;
const unsigned int shim_north0_slave_port_index = 14;
const unsigned int shim_north0_master_port_index = 12;

// Bit Definitions in Key Registers
// ###################################

const unsigned int uc_mdm_pccmdr_clear_bit = 4;
const unsigned int uc_mdm_pccmdr_start_bit = 3;
const unsigned int uc_mdm_pccmdr_stop_bit = 2;
const unsigned int uc_mdm_pccmdr_sample_bit = 1;
const unsigned int uc_mdm_pccmdr_reset_bit = 0;
const unsigned int uc_mdm_pcsr_overflow_bit = 1;
const unsigned int uc_mdm_pcsr_full_bit = 0;
const unsigned int uc_mdm_pcdrr_latency_reads = 4;

} // namespace aie2ps

#endif /* AIE2PS_ATTRIBUTES_H_ */
