// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef AIE1_REGISTERS_H_
#define AIE1_REGISTERS_H_

namespace aie1 
{

// Register definitions for AIE1
// ###################################

// Register definitions for CM
// ###################################
// Program Counter
const unsigned int cm_program_counter = 0x00030280;
// Core MD0
const unsigned int cm_md0 = 0x00030440;
// Core MD1
const unsigned int cm_md1 = 0x00030450;
// Core MC0
const unsigned int cm_mc0 = 0x00030460;
// Core MC1
const unsigned int cm_mc1 = 0x00030470;
// Performance Counters 1-0 Start and Stop Event
const unsigned int cm_performance_control0 = 0x00031000;
// Performance Counters 3-2 Start and Stop Event
const unsigned int cm_performance_control1 = 0x00031004;
// Performance Counters Reset Events
const unsigned int cm_performance_control2 = 0x00031008;
// Performance Counter0
const unsigned int cm_performance_counter0 = 0x00031020;
// Performance Counter1
const unsigned int cm_performance_counter1 = 0x00031024;
// Performance Counter2
const unsigned int cm_performance_counter2 = 0x00031028;
// Performance Counter3
const unsigned int cm_performance_counter3 = 0x0003102c;
// Performance Counter0 Event Value.
const unsigned int cm_performance_counter0_event_value = 0x00031080;
// Performance Counter1 Event Value. When the Performance Counter1 reach this value, an event will be generated
const unsigned int cm_performance_counter1_event_value = 0x00031084;
// Performance Counter2 Event Value. When the Performance Counter2 reach this value, an event will be generated
const unsigned int cm_performance_counter2_event_value = 0x00031088;
// Performance Counter3 Event Value. When the Performance Counter3 reach this value, an event will be generated
const unsigned int cm_performance_counter3_event_value = 0x0003108c;
// Control of the AI Engine
const unsigned int cm_core_control = 0x00032000;
// The status of the AI Engine
const unsigned int cm_core_status = 0x00032004;
// Set enable event trigger
const unsigned int cm_enable_events = 0x00032008;
// Set reset event trigger
const unsigned int cm_reset_event = 0x0003200c;
// Debug control of manual debug stall and single step count
const unsigned int cm_debug_control0 = 0x00032010;
// Debug Halt Event Control
const unsigned int cm_debug_control1 = 0x00032014;
// Debug Halt Control
const unsigned int cm_debug_control2 = 0x00032018;
// Debug Status
const unsigned int cm_debug_status = 0x0003201c;
// PC_Event0
const unsigned int cm_pc_event0 = 0x00032020;
// PC_Event1
const unsigned int cm_pc_event1 = 0x00032024;
// PC_Event2
const unsigned int cm_pc_event2 = 0x00032028;
// PC_Event3
const unsigned int cm_pc_event3 = 0x0003202c;
// Error Halt Control
const unsigned int cm_error_halt_control = 0x00032030;
// Error Halt Event
const unsigned int cm_error_halt_event = 0x00032034;
// ECC scrubbing event
const unsigned int cm_ecc_scrubbing_event = 0x00032110;
// Control of Internal Timer
const unsigned int cm_timer_control = 0x00034000;
// Generate an internal event
const unsigned int cm_event_generate = 0x00034008;
// Control of which Internal Event to Broadcast0
const unsigned int cm_event_broadcast0 = 0x00034010;
// Control of which Internal Event to Broadcast1
const unsigned int cm_event_broadcast1 = 0x00034014;
// Control of which Internal Event to Broadcast2
const unsigned int cm_event_broadcast2 = 0x00034018;
// Control of which Internal Event to Broadcast3
const unsigned int cm_event_broadcast3 = 0x0003401c;
// Control of which Internal Event to Broadcast4
const unsigned int cm_event_broadcast4 = 0x00034020;
// Control of which Internal Event to Broadcast5
const unsigned int cm_event_broadcast5 = 0x00034024;
// Control of which Internal Event to Broadcast6
const unsigned int cm_event_broadcast6 = 0x00034028;
// Control of which Internal Event to Broadcast7
const unsigned int cm_event_broadcast7 = 0x0003402c;
// Control of which Internal Event to Broadcast8
const unsigned int cm_event_broadcast8 = 0x00034030;
// Control of which Internal Event to Broadcast9
const unsigned int cm_event_broadcast9 = 0x00034034;
// Control of which Internal Event to Broadcast10
const unsigned int cm_event_broadcast10 = 0x00034038;
// Control of which Internal Event to Broadcast11
const unsigned int cm_event_broadcast11 = 0x0003403c;
// Control of which Internal Event to Broadcast12
const unsigned int cm_event_broadcast12 = 0x00034040;
// Control of which Internal Event to Broadcast13
const unsigned int cm_event_broadcast13 = 0x00034044;
// Control of which Internal Event to Broadcast14
const unsigned int cm_event_broadcast14 = 0x00034048;
// Control of which Internal Event to Broadcast15
const unsigned int cm_event_broadcast15 = 0x0003404c;
// Set block of broadcast signals to South
const unsigned int cm_event_broadcast_block_south_set = 0x00034050;
// Clear block of broadcast signals to South
const unsigned int cm_event_broadcast_block_south_clr = 0x00034054;
// Current value of block for broadcast signals to South
const unsigned int cm_event_broadcast_block_south_value = 0x00034058;
// Set block of broadcast signals to West
const unsigned int cm_event_broadcast_block_west_set = 0x00034060;
// Clear block of broadcast signals to West
const unsigned int cm_event_broadcast_block_west_clr = 0x00034064;
// Current value of block for broadcast signals to West
const unsigned int cm_event_broadcast_block_west_value = 0x00034068;
// Set block of broadcast signals to North
const unsigned int cm_event_broadcast_block_north_set = 0x00034070;
// Clear block of broadcast signals to North
const unsigned int cm_event_broadcast_block_north_clr = 0x00034074;
// Current value of block for broadcast signals to North
const unsigned int cm_event_broadcast_block_north_value = 0x00034078;
// Set block of broadcast signals to East
const unsigned int cm_event_broadcast_block_east_set = 0x00034080;
// Clear block of broadcast signals to East
const unsigned int cm_event_broadcast_block_east_clr = 0x00034084;
// Current value of block for broadcast signals to East
const unsigned int cm_event_broadcast_block_east_value = 0x00034088;
// Control of Trace
const unsigned int cm_trace_control0 = 0x000340d0;
// Control of Trace: packet destination
const unsigned int cm_trace_control1 = 0x000340d4;
// Status of trace engine
const unsigned int cm_trace_status = 0x000340d8;
// Control of which Internal Event to Broadcast
const unsigned int cm_trace_event0 = 0x000340e0;
// Control of which Internal Event to Broadcast
const unsigned int cm_trace_event1 = 0x000340e4;
// Internal Timer Event Value.
const unsigned int cm_timer_trig_event_low_value = 0x000340f0;
// Internal Timer Event Value.
const unsigned int cm_timer_trig_event_high_value = 0x000340f4;
// Internal Timer Low part Value.
const unsigned int cm_timer_low = 0x000340f8;
// Internal Timer High part Value.
const unsigned int cm_timer_high = 0x000340fc;
// Internal event status register0
const unsigned int cm_event_status0 = 0x00034200;
// Internal event status register1
const unsigned int cm_event_status1 = 0x00034204;
// Internal event status register2
const unsigned int cm_event_status2 = 0x00034208;
// Internal event status register3
const unsigned int cm_event_status3 = 0x0003420c;
// Combo events input events
const unsigned int cm_combo_event_inputs = 0x00034400;
// Combo events input events
const unsigned int cm_combo_event_control = 0x00034404;
// Event enable for Group 0
const unsigned int cm_event_group_0_enable = 0x00034500;
// Event enable for PC Group
const unsigned int cm_event_group_pc_enable = 0x00034504;
// Event enable for AI Engine Stall Group
const unsigned int cm_event_group_core_stall_enable = 0x00034508;
// Event enable for AI Engine Program Flow Group
const unsigned int cm_event_group_core_program_flow_enable = 0x0003450c;
// Event enable for Non Fatal Error Group
const unsigned int cm_event_group_errors0_enable = 0x00034510;
// Event enable for AI Engine Fatal Error Group
const unsigned int cm_event_group_errors1_enable = 0x00034514;
// Event enable for Stream Switch Group
const unsigned int cm_event_group_stream_switch_enable = 0x00034518;
// Event enable for Broadcast group
const unsigned int cm_event_group_broadcast_enable = 0x0003451c;
// Event enable for User group
const unsigned int cm_event_group_user_event_enable = 0x00034520;
// Tile control register
const unsigned int cm_tile_control = 0x00036030;
// Status of control packet handling
const unsigned int cm_tile_control_packet_handler_status = 0x00036034;
// Clock control for the tile
const unsigned int cm_tile_clock_control = 0x00036040;
// Stream Switch Master Configuration AI Engine 0
const unsigned int cm_stream_switch_master_config_aie_core0 = 0x0003f000;
// Stream Switch Master Configuration AI Engine 1
const unsigned int cm_stream_switch_master_config_aie_core1 = 0x0003f004;
// Stream Switch Master Configuration DMA 0
const unsigned int cm_stream_switch_master_config_dma0 = 0x0003f008;
// Stream Switch Master Configuration DMA 1
const unsigned int cm_stream_switch_master_config_dma1 = 0x0003f00c;
// Stream Switch Master Configuration AI Engine Tile Ctrl
const unsigned int cm_stream_switch_master_config_tile_ctrl = 0x0003f010;
// Stream Switch Master Configuration FIFO 0
const unsigned int cm_stream_switch_master_config_fifo0 = 0x0003f014;
// Stream Switch Master Configuration FIFO 1
const unsigned int cm_stream_switch_master_config_fifo1 = 0x0003f018;
// Stream Switch Master Configuration South 0
const unsigned int cm_stream_switch_master_config_south0 = 0x0003f01c;
// Stream Switch Master Configuration South 1
const unsigned int cm_stream_switch_master_config_south1 = 0x0003f020;
// Stream Switch Master Configuration South 2
const unsigned int cm_stream_switch_master_config_south2 = 0x0003f024;
// Stream Switch Master Configuration South 3
const unsigned int cm_stream_switch_master_config_south3 = 0x0003f028;
// Stream Switch Master Configuration West 0
const unsigned int cm_stream_switch_master_config_west0 = 0x0003f02c;
// Stream Switch Master Configuration West 1
const unsigned int cm_stream_switch_master_config_west1 = 0x0003f030;
// Stream Switch Master Configuration West 2
const unsigned int cm_stream_switch_master_config_west2 = 0x0003f034;
// Stream Switch Master Configuration West 3
const unsigned int cm_stream_switch_master_config_west3 = 0x0003f038;
// Stream Switch Master Configuration North 0
const unsigned int cm_stream_switch_master_config_north0 = 0x0003f03c;
// Stream Switch Master Configuration North 1
const unsigned int cm_stream_switch_master_config_north1 = 0x0003f040;
// Stream Switch Master Configuration North 2
const unsigned int cm_stream_switch_master_config_north2 = 0x0003f044;
// Stream Switch Master Configuration North 3
const unsigned int cm_stream_switch_master_config_north3 = 0x0003f048;
// Stream Switch Master Configuration North 4
const unsigned int cm_stream_switch_master_config_north4 = 0x0003f04c;
// Stream Switch Master Configuration North 5
const unsigned int cm_stream_switch_master_config_north5 = 0x0003f050;
// Stream Switch Master Configuration East 0
const unsigned int cm_stream_switch_master_config_east0 = 0x0003f054;
// Stream Switch Master Configuration East 1
const unsigned int cm_stream_switch_master_config_east1 = 0x0003f058;
// Stream Switch Master Configuration East 2
const unsigned int cm_stream_switch_master_config_east2 = 0x0003f05c;
// Stream Switch Master Configuration East 3
const unsigned int cm_stream_switch_master_config_east3 = 0x0003f060;
// Stream Switch Slave Configuration AI Engine 0
const unsigned int cm_stream_switch_slave_config_aie_core0 = 0x0003f100;
// Stream Switch Slave Configuration AI Engine 1
const unsigned int cm_stream_switch_slave_config_aie_core1 = 0x0003f104;
// Stream Switch Slave Configuration DMA 0
const unsigned int cm_stream_switch_slave_config_dma_0 = 0x0003f108;
// Stream Switch Slave Configuration DMA 1
const unsigned int cm_stream_switch_slave_config_dma_1 = 0x0003f10c;
// Stream Switch Slave Configuration Tile Ctrl
const unsigned int cm_stream_switch_slave_config_tile_ctrl = 0x0003f110;
// Stream Switch Slave Configuration FIFO 0
const unsigned int cm_stream_switch_slave_config_fifo_0 = 0x0003f114;
// Stream Switch Slave Configuration FIFO 1
const unsigned int cm_stream_switch_slave_config_fifo_1 = 0x0003f118;
// Stream Switch Slave Configuration South 0
const unsigned int cm_stream_switch_slave_config_south_0 = 0x0003f11c;
// Stream Switch Slave Configuration South 1
const unsigned int cm_stream_switch_slave_config_south_1 = 0x0003f120;
// Stream Switch Slave Configuration South 2
const unsigned int cm_stream_switch_slave_config_south_2 = 0x0003f124;
// Stream Switch Slave Configuration South 3
const unsigned int cm_stream_switch_slave_config_south_3 = 0x0003f128;
// Stream Switch Slave Configuration South 4
const unsigned int cm_stream_switch_slave_config_south_4 = 0x0003f12c;
// Stream Switch Slave Configuration South 5
const unsigned int cm_stream_switch_slave_config_south_5 = 0x0003f130;
// Stream Switch Slave Configuration West 0
const unsigned int cm_stream_switch_slave_config_west_0 = 0x0003f134;
// Stream Switch Slave Configuration West 1
const unsigned int cm_stream_switch_slave_config_west_1 = 0x0003f138;
// Stream Switch Slave Configuration West 2
const unsigned int cm_stream_switch_slave_config_west_2 = 0x0003f13c;
// Stream Switch Slave Configuration West 3
const unsigned int cm_stream_switch_slave_config_west_3 = 0x0003f140;
// Stream Switch Slave Configuration North 0
const unsigned int cm_stream_switch_slave_config_north_0 = 0x0003f144;
// Stream Switch Slave Configuration North 1
const unsigned int cm_stream_switch_slave_config_north_1 = 0x0003f148;
// Stream Switch Slave Configuration North 2
const unsigned int cm_stream_switch_slave_config_north_2 = 0x0003f14c;
// Stream Switch Slave Configuration North 3
const unsigned int cm_stream_switch_slave_config_north_3 = 0x0003f150;
// Stream Switch Slave Configuration East 0
const unsigned int cm_stream_switch_slave_config_east_0 = 0x0003f154;
// Stream Switch Slave Configuration East 1
const unsigned int cm_stream_switch_slave_config_east_1 = 0x0003f158;
// Stream Switch Slave Configuration East 2
const unsigned int cm_stream_switch_slave_config_east_2 = 0x0003f15c;
// Stream Switch Slave Configuration East 3
const unsigned int cm_stream_switch_slave_config_east_3 = 0x0003f160;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int cm_stream_switch_slave_config_aie_trace = 0x0003f164;
// Stream Switch Slave Configuration Mem Trace
const unsigned int cm_stream_switch_slave_config_mem_trace = 0x0003f168;
// Stream Switch Slave Configuration AI Engine 0
const unsigned int cm_stream_switch_slave_aie_core0_slot0 = 0x0003f200;
// Stream Switch Slave Configuration AI Engine 0
const unsigned int cm_stream_switch_slave_aie_core0_slot1 = 0x0003f204;
// Stream Switch Slave Configuration AI Engine 0
const unsigned int cm_stream_switch_slave_aie_core0_slot2 = 0x0003f208;
// Stream Switch Slave Configuration AI Engine 0
const unsigned int cm_stream_switch_slave_aie_core0_slot3 = 0x0003f20c;
// Stream Switch Slave Configuration AI Engine 1
const unsigned int cm_stream_switch_slave_aie_core1_slot0 = 0x0003f210;
// Stream Switch Slave Configuration AI Engine 1
const unsigned int cm_stream_switch_slave_aie_core1_slot1 = 0x0003f214;
// Stream Switch Slave Configuration AI Engine 1
const unsigned int cm_stream_switch_slave_aie_core1_slot2 = 0x0003f218;
// Stream Switch Slave Configuration AI Engine 1
const unsigned int cm_stream_switch_slave_aie_core1_slot3 = 0x0003f21c;
// Stream Switch Slave Configuration DMA 0
const unsigned int cm_stream_switch_slave_dma_0_slot0 = 0x0003f220;
// Stream Switch Slave Configuration DMA 0
const unsigned int cm_stream_switch_slave_dma_0_slot1 = 0x0003f224;
// Stream Switch Slave Configuration DMA 0
const unsigned int cm_stream_switch_slave_dma_0_slot2 = 0x0003f228;
// Stream Switch Slave Configuration DMA 0
const unsigned int cm_stream_switch_slave_dma_0_slot3 = 0x0003f22c;
// Stream Switch Slave Configuration DMA 1
const unsigned int cm_stream_switch_slave_dma_1_slot0 = 0x0003f230;
// Stream Switch Slave Configuration DMA 1
const unsigned int cm_stream_switch_slave_dma_1_slot1 = 0x0003f234;
// Stream Switch Slave Configuration DMA 1
const unsigned int cm_stream_switch_slave_dma_1_slot2 = 0x0003f238;
// Stream Switch Slave Configuration DMA 1
const unsigned int cm_stream_switch_slave_dma_1_slot3 = 0x0003f23c;
// Stream Switch Slave Configuration Tile Ctrl
const unsigned int cm_stream_switch_slave_tile_ctrl_slot0 = 0x0003f240;
// Stream Switch Slave Configuration Tile Ctrl
const unsigned int cm_stream_switch_slave_tile_ctrl_slot1 = 0x0003f244;
// Stream Switch Slave Configuration Tile Ctrl
const unsigned int cm_stream_switch_slave_tile_ctrl_slot2 = 0x0003f248;
// Stream Switch Slave Configuration Tile Ctrl
const unsigned int cm_stream_switch_slave_tile_ctrl_slot3 = 0x0003f24c;
// Stream Switch Slave Configuration FIFO 0
const unsigned int cm_stream_switch_slave_fifo_0_slot0 = 0x0003f250;
// Stream Switch Slave Configuration FIFO 0
const unsigned int cm_stream_switch_slave_fifo_0_slot1 = 0x0003f254;
// Stream Switch Slave Configuration FIFO 0
const unsigned int cm_stream_switch_slave_fifo_0_slot2 = 0x0003f258;
// Stream Switch Slave Configuration FIFO 0
const unsigned int cm_stream_switch_slave_fifo_0_slot3 = 0x0003f25c;
// Stream Switch Slave Configuration FIFO 1
const unsigned int cm_stream_switch_slave_fifo_1_slot0 = 0x0003f260;
// Stream Switch Slave Configuration FIFO 1
const unsigned int cm_stream_switch_slave_fifo_1_slot1 = 0x0003f264;
// Stream Switch Slave Configuration FIFO 1
const unsigned int cm_stream_switch_slave_fifo_1_slot2 = 0x0003f268;
// Stream Switch Slave Configuration FIFO 1
const unsigned int cm_stream_switch_slave_fifo_1_slot3 = 0x0003f26c;
// Stream Switch Slave Configuration South 0
const unsigned int cm_stream_switch_slave_south_0_slot0 = 0x0003f270;
// Stream Switch Slave Configuration South 0
const unsigned int cm_stream_switch_slave_south_0_slot1 = 0x0003f274;
// Stream Switch Slave Configuration South 0
const unsigned int cm_stream_switch_slave_south_0_slot2 = 0x0003f278;
// Stream Switch Slave Configuration South 0
const unsigned int cm_stream_switch_slave_south_0_slot3 = 0x0003f27c;
// Stream Switch Slave Configuration South 1
const unsigned int cm_stream_switch_slave_south_1_slot0 = 0x0003f280;
// Stream Switch Slave Configuration South 1
const unsigned int cm_stream_switch_slave_south_1_slot1 = 0x0003f284;
// Stream Switch Slave Configuration South 1
const unsigned int cm_stream_switch_slave_south_1_slot2 = 0x0003f288;
// Stream Switch Slave Configuration South 1
const unsigned int cm_stream_switch_slave_south_1_slot3 = 0x0003f28c;
// Stream Switch Slave Configuration South 2
const unsigned int cm_stream_switch_slave_south_2_slot0 = 0x0003f290;
// Stream Switch Slave Configuration South 2
const unsigned int cm_stream_switch_slave_south_2_slot1 = 0x0003f294;
// Stream Switch Slave Configuration South 2
const unsigned int cm_stream_switch_slave_south_2_slot2 = 0x0003f298;
// Stream Switch Slave Configuration South 2
const unsigned int cm_stream_switch_slave_south_2_slot3 = 0x0003f29c;
// Stream Switch Slave Configuration South 3
const unsigned int cm_stream_switch_slave_south_3_slot0 = 0x0003f2a0;
// Stream Switch Slave Configuration South 3
const unsigned int cm_stream_switch_slave_south_3_slot1 = 0x0003f2a4;
// Stream Switch Slave Configuration South 3
const unsigned int cm_stream_switch_slave_south_3_slot2 = 0x0003f2a8;
// Stream Switch Slave Configuration South 3
const unsigned int cm_stream_switch_slave_south_3_slot3 = 0x0003f2ac;
// Stream Switch Slave Configuration South 4
const unsigned int cm_stream_switch_slave_south_4_slot0 = 0x0003f2b0;
// Stream Switch Slave Configuration South 4
const unsigned int cm_stream_switch_slave_south_4_slot1 = 0x0003f2b4;
// Stream Switch Slave Configuration South 4
const unsigned int cm_stream_switch_slave_south_4_slot2 = 0x0003f2b8;
// Stream Switch Slave Configuration South 4
const unsigned int cm_stream_switch_slave_south_4_slot3 = 0x0003f2bc;
// Stream Switch Slave Configuration South 5
const unsigned int cm_stream_switch_slave_south_5_slot0 = 0x0003f2c0;
// Stream Switch Slave Configuration South 5
const unsigned int cm_stream_switch_slave_south_5_slot1 = 0x0003f2c4;
// Stream Switch Slave Configuration South 5
const unsigned int cm_stream_switch_slave_south_5_slot2 = 0x0003f2c8;
// Stream Switch Slave Configuration South 5
const unsigned int cm_stream_switch_slave_south_5_slot3 = 0x0003f2cc;
// Stream Switch Slave Configuration West 0
const unsigned int cm_stream_switch_slave_west_0_slot0 = 0x0003f2d0;
// Stream Switch Slave Configuration West 0
const unsigned int cm_stream_switch_slave_west_0_slot1 = 0x0003f2d4;
// Stream Switch Slave Configuration West 0
const unsigned int cm_stream_switch_slave_west_0_slot2 = 0x0003f2d8;
// Stream Switch Slave Configuration West 0
const unsigned int cm_stream_switch_slave_west_0_slot3 = 0x0003f2dc;
// Stream Switch Slave Configuration West 1
const unsigned int cm_stream_switch_slave_west_1_slot0 = 0x0003f2e0;
// Stream Switch Slave Configuration West 1
const unsigned int cm_stream_switch_slave_west_1_slot1 = 0x0003f2e4;
// Stream Switch Slave Configuration West 1
const unsigned int cm_stream_switch_slave_west_1_slot2 = 0x0003f2e8;
// Stream Switch Slave Configuration West 1
const unsigned int cm_stream_switch_slave_west_1_slot3 = 0x0003f2ec;
// Stream Switch Slave Configuration West 2
const unsigned int cm_stream_switch_slave_west_2_slot0 = 0x0003f2f0;
// Stream Switch Slave Configuration West 2
const unsigned int cm_stream_switch_slave_west_2_slot1 = 0x0003f2f4;
// Stream Switch Slave Configuration West 2
const unsigned int cm_stream_switch_slave_west_2_slot2 = 0x0003f2f8;
// Stream Switch Slave Configuration West 2
const unsigned int cm_stream_switch_slave_west_2_slot3 = 0x0003f2fc;
// Stream Switch Slave Configuration West 3
const unsigned int cm_stream_switch_slave_west_3_slot0 = 0x0003f300;
// Stream Switch Slave Configuration West 3
const unsigned int cm_stream_switch_slave_west_3_slot1 = 0x0003f304;
// Stream Switch Slave Configuration West 3
const unsigned int cm_stream_switch_slave_west_3_slot2 = 0x0003f308;
// Stream Switch Slave Configuration West 3
const unsigned int cm_stream_switch_slave_west_3_slot3 = 0x0003f30c;
// Stream Switch Slave Configuration North 0
const unsigned int cm_stream_switch_slave_north_0_slot0 = 0x0003f310;
// Stream Switch Slave Configuration North 0
const unsigned int cm_stream_switch_slave_north_0_slot1 = 0x0003f314;
// Stream Switch Slave Configuration North 0
const unsigned int cm_stream_switch_slave_north_0_slot2 = 0x0003f318;
// Stream Switch Slave Configuration North 0
const unsigned int cm_stream_switch_slave_north_0_slot3 = 0x0003f31c;
// Stream Switch Slave Configuration North 1
const unsigned int cm_stream_switch_slave_north_1_slot0 = 0x0003f320;
// Stream Switch Slave Configuration North 1
const unsigned int cm_stream_switch_slave_north_1_slot1 = 0x0003f324;
// Stream Switch Slave Configuration North 1
const unsigned int cm_stream_switch_slave_north_1_slot2 = 0x0003f328;
// Stream Switch Slave Configuration North 1
const unsigned int cm_stream_switch_slave_north_1_slot3 = 0x0003f32c;
// Stream Switch Slave Configuration North 2
const unsigned int cm_stream_switch_slave_north_2_slot0 = 0x0003f330;
// Stream Switch Slave Configuration North 2
const unsigned int cm_stream_switch_slave_north_2_slot1 = 0x0003f334;
// Stream Switch Slave Configuration North 2
const unsigned int cm_stream_switch_slave_north_2_slot2 = 0x0003f338;
// Stream Switch Slave Configuration North 2
const unsigned int cm_stream_switch_slave_north_2_slot3 = 0x0003f33c;
// Stream Switch Slave Configuration North 3
const unsigned int cm_stream_switch_slave_north_3_slot0 = 0x0003f340;
// Stream Switch Slave Configuration North 3
const unsigned int cm_stream_switch_slave_north_3_slot1 = 0x0003f344;
// Stream Switch Slave Configuration North 3
const unsigned int cm_stream_switch_slave_north_3_slot2 = 0x0003f348;
// Stream Switch Slave Configuration North 3
const unsigned int cm_stream_switch_slave_north_3_slot3 = 0x0003f34c;
// Stream Switch Slave Configuration East 0
const unsigned int cm_stream_switch_slave_east_0_slot0 = 0x0003f350;
// Stream Switch Slave Configuration East 0
const unsigned int cm_stream_switch_slave_east_0_slot1 = 0x0003f354;
// Stream Switch Slave Configuration East 0
const unsigned int cm_stream_switch_slave_east_0_slot2 = 0x0003f358;
// Stream Switch Slave Configuration East 0
const unsigned int cm_stream_switch_slave_east_0_slot3 = 0x0003f35c;
// Stream Switch Slave Configuration East 1
const unsigned int cm_stream_switch_slave_east_1_slot0 = 0x0003f360;
// Stream Switch Slave Configuration East 1
const unsigned int cm_stream_switch_slave_east_1_slot1 = 0x0003f364;
// Stream Switch Slave Configuration East 1
const unsigned int cm_stream_switch_slave_east_1_slot2 = 0x0003f368;
// Stream Switch Slave Configuration East 1
const unsigned int cm_stream_switch_slave_east_1_slot3 = 0x0003f36c;
// Stream Switch Slave Configuration East 2
const unsigned int cm_stream_switch_slave_east_2_slot0 = 0x0003f370;
// Stream Switch Slave Configuration East 2
const unsigned int cm_stream_switch_slave_east_2_slot1 = 0x0003f374;
// Stream Switch Slave Configuration East 2
const unsigned int cm_stream_switch_slave_east_2_slot2 = 0x0003f378;
// Stream Switch Slave Configuration East 2
const unsigned int cm_stream_switch_slave_east_2_slot3 = 0x0003f37c;
// Stream Switch Slave Configuration East 3
const unsigned int cm_stream_switch_slave_east_3_slot0 = 0x0003f380;
// Stream Switch Slave Configuration East 3
const unsigned int cm_stream_switch_slave_east_3_slot1 = 0x0003f384;
// Stream Switch Slave Configuration East 3
const unsigned int cm_stream_switch_slave_east_3_slot2 = 0x0003f388;
// Stream Switch Slave Configuration East 3
const unsigned int cm_stream_switch_slave_east_3_slot3 = 0x0003f38c;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int cm_stream_switch_slave_aie_trace_slot0 = 0x0003f390;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int cm_stream_switch_slave_aie_trace_slot1 = 0x0003f394;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int cm_stream_switch_slave_aie_trace_slot2 = 0x0003f398;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int cm_stream_switch_slave_aie_trace_slot3 = 0x0003f39c;
// Stream Switch Slave Configuration Mem Trace
const unsigned int cm_stream_switch_slave_mem_trace_slot0 = 0x0003f3a0;
// Stream Switch Slave Configuration Mem Trace
const unsigned int cm_stream_switch_slave_mem_trace_slot1 = 0x0003f3a4;
// Stream Switch Slave Configuration Mem Trace
const unsigned int cm_stream_switch_slave_mem_trace_slot2 = 0x0003f3a8;
// Stream Switch Slave Configuration Mem Trace
const unsigned int cm_stream_switch_slave_mem_trace_slot3 = 0x0003f3ac;
// Select Stream Switch Ports for event generation
const unsigned int cm_stream_switch_event_port_selection_0 = 0x0003ff00;
// Select Stream Switch Ports for event generation
const unsigned int cm_stream_switch_event_port_selection_1 = 0x0003ff04;

// Register definitions for MM
// ###################################
// Performance Counters Start and Stop Events
const unsigned int mm_performance_control0 = 0x00011000;
// Performance Counters Reset Events
const unsigned int mm_performance_control1 = 0x00011008;
// Performance Counter0
const unsigned int mm_performance_counter0 = 0x00011020;
// Performance Counter1
const unsigned int mm_performance_counter1 = 0x00011024;
// Performance Counter0 Event Value.
const unsigned int mm_performance_counter0_event_value = 0x00011080;
// Performance Counter1 Event Value.
const unsigned int mm_performance_counter1_event_value = 0x00011084;
// Inhibits check bits (parity or ECC) update to memory on writes
const unsigned int mm_checkbit_error_generation = 0x00012000;
// ECC Scrubbing Event
const unsigned int mm_ecc_scrubbing_event = 0x00012110;
// ECC Failing Address
const unsigned int mm_ecc_failing_address = 0x00012120;
// Parity Failing Address
const unsigned int mm_parity_failing_address = 0x00012124;
// Reset Control
const unsigned int mm_reset_control = 0x00013000;
// Control of Internal Timer
const unsigned int mm_timer_control = 0x00014000;
// Generate an internal event
const unsigned int mm_event_generate = 0x00014008;
// Control of which Internal Event to Broadcast0
const unsigned int mm_event_broadcast0 = 0x00014010;
// Control of which Internal Event to Broadcast1
const unsigned int mm_event_broadcast1 = 0x00014014;
// Control of which Internal Event to Broadcast2
const unsigned int mm_event_broadcast2 = 0x00014018;
// Control of which Internal Event to Broadcast3
const unsigned int mm_event_broadcast3 = 0x0001401c;
// Control of which Internal Event to Broadcast4
const unsigned int mm_event_broadcast4 = 0x00014020;
// Control of which Internal Event to Broadcast5
const unsigned int mm_event_broadcast5 = 0x00014024;
// Control of which Internal Event to Broadcast6
const unsigned int mm_event_broadcast6 = 0x00014028;
// Control of which Internal Event to Broadcast7
const unsigned int mm_event_broadcast7 = 0x0001402c;
// Control of which Internal Event to Broadcast8
const unsigned int mm_event_broadcast8 = 0x00014030;
// Control of which Internal Event to Broadcast9
const unsigned int mm_event_broadcast9 = 0x00014034;
// Control of which Internal Event to Broadcast10
const unsigned int mm_event_broadcast10 = 0x00014038;
// Control of which Internal Event to Broadcast11
const unsigned int mm_event_broadcast11 = 0x0001403c;
// Control of which Internal Event to Broadcast12
const unsigned int mm_event_broadcast12 = 0x00014040;
// Control of which Internal Event to Broadcast13
const unsigned int mm_event_broadcast13 = 0x00014044;
// Control of which Internal Event to Broadcast14
const unsigned int mm_event_broadcast14 = 0x00014048;
// Control of which Internal Event to Broadcast15
const unsigned int mm_event_broadcast15 = 0x0001404c;
// Set block of broadcast signals to South
const unsigned int mm_event_broadcast_block_south_set = 0x00014050;
// Clear block of broadcast signals to South
const unsigned int mm_event_broadcast_block_south_clr = 0x00014054;
// Current value of block for broadcast signals to South
const unsigned int mm_event_broadcast_block_south_value = 0x00014058;
// Set block of broadcast signals to West
const unsigned int mm_event_broadcast_block_west_set = 0x00014060;
// Clear block of broadcast signals to West
const unsigned int mm_event_broadcast_block_west_clr = 0x00014064;
// Current value of block for broadcast signals to West
const unsigned int mm_event_broadcast_block_west_value = 0x00014068;
// Set block of broadcast signals to North
const unsigned int mm_event_broadcast_block_north_set = 0x00014070;
// Clear block of broadcast signals to North
const unsigned int mm_event_broadcast_block_north_clr = 0x00014074;
// Current value of block for broadcast signals to North
const unsigned int mm_event_broadcast_block_north_value = 0x00014078;
// Set block of broadcast signals to East
const unsigned int mm_event_broadcast_block_east_set = 0x00014080;
// Clear block of broadcast signals to East
const unsigned int mm_event_broadcast_block_east_clr = 0x00014084;
// Current value of block for broadcast signals to East
const unsigned int mm_event_broadcast_block_east_value = 0x00014088;
// Control of Trace
const unsigned int mm_trace_control0 = 0x000140d0;
// Control of Trace packet configuration
const unsigned int mm_trace_control1 = 0x000140d4;
// Status of trace engine
const unsigned int mm_trace_status = 0x000140d8;
// Control of which Internal Event to Broadcast
const unsigned int mm_trace_event0 = 0x000140e0;
// Control of which Internal Event to Broadcast
const unsigned int mm_trace_event1 = 0x000140e4;
// Internal Timer Event Value.
const unsigned int mm_timer_trig_event_low_value = 0x000140f0;
// Internal Timer Event Value.
const unsigned int mm_timer_trig_event_high_value = 0x000140f4;
// Internal Timer Low part Value.
const unsigned int mm_timer_low = 0x000140f8;
// Internal Timer High part Value.
const unsigned int mm_timer_high = 0x000140fc;
// Define Watchpoint0
const unsigned int mm_watchpoint0 = 0x00014100;
// Define Watchpoint1
const unsigned int mm_watchpoint1 = 0x00014104;
// Internal event status register0
const unsigned int mm_event_status0 = 0x00014200;
// Internal event status register1
const unsigned int mm_event_status1 = 0x00014204;
// Internal event status register2
const unsigned int mm_event_status2 = 0x00014208;
// Internal event status register3
const unsigned int mm_event_status3 = 0x0001420c;
// Reserved
const unsigned int mm_reserved0 = 0x00014210;
// Reserved
const unsigned int mm_reserved1 = 0x00014214;
// Reserved
const unsigned int mm_reserved2 = 0x00014218;
// Reserved
const unsigned int mm_reserved3 = 0x0001421c;
// Combo events input events
const unsigned int mm_combo_event_inputs = 0x00014400;
// Combo events input events
const unsigned int mm_combo_event_control = 0x00014404;
// Event enable for Group 0
const unsigned int mm_event_group_0_enable = 0x00014500;
// Event enable for Watchpoint Group
const unsigned int mm_event_group_watchpoint_enable = 0x00014504;
// Event enable for DMA Group
const unsigned int mm_event_group_dma_enable = 0x00014508;
// Event enable for Lock Group
const unsigned int mm_event_group_lock_enable = 0x0001450c;
// Event enable for Memory Conflict Group
const unsigned int mm_event_group_memory_conflict_enable = 0x00014510;
// Event enable for Error Group
const unsigned int mm_event_group_error_enable = 0x00014514;
// Event enable for Broadcast Group
const unsigned int mm_event_group_broadcast_enable = 0x00014518;
// Event enable for User Group
const unsigned int mm_event_group_user_event_enable = 0x0001451c;
// Spare register
const unsigned int mm_spare_reg = 0x00016000;
// DMA BD0 A Address and Lock
const unsigned int mm_dma_bd0_addr_a = 0x0001d000;
// DMA BD0 B Address and Lock
const unsigned int mm_dma_bd0_addr_b = 0x0001d004;
// DMA BD0 2D X Offset, Wrap, Increment
const unsigned int mm_dma_bd0_2d_x = 0x0001d008;
// DMA BD0 2D Y Offset, Wrap, Increment
const unsigned int mm_dma_bd0_2d_y = 0x0001d00c;
// DMA BD0 Packet header
const unsigned int mm_dma_bd0_packet = 0x0001d010;
// DMA BD0 Interleaved state
const unsigned int mm_dma_bd0_interleaved_state = 0x0001d014;
// DMA BD0 Length, Next BD, BD Count
const unsigned int mm_dma_bd0_control = 0x0001d018;
// DMA BD1 A Address and Lock
const unsigned int mm_dma_bd1_addr_a = 0x0001d020;
// DMA BD1 B Address and Lock
const unsigned int mm_dma_bd1_addr_b = 0x0001d024;
// DMA BD1 2D X Offset, Wrap, Increment
const unsigned int mm_dma_bd1_2d_x = 0x0001d028;
// DMA BD1 2D Y Offset, Wrap, Increment
const unsigned int mm_dma_bd1_2d_y = 0x0001d02c;
// DMA BD1 Dynamic packet header
const unsigned int mm_dma_bd1_packet = 0x0001d030;
// DMA BD1 Interleaved state
const unsigned int mm_dma_bd1_interleaved_state = 0x0001d034;
// DMA BD1 Length, Next BD, BD Count
const unsigned int mm_dma_bd1_control = 0x0001d038;
// DMA BD2 A Address and Lock
const unsigned int mm_dma_bd2_addr_a = 0x0001d040;
// DMA BD2 B Address and Lock
const unsigned int mm_dma_bd2_addr_b = 0x0001d044;
// DMA BD2 2D X Offset, Wrap, Increment
const unsigned int mm_dma_bd2_2d_x = 0x0001d048;
// DMA BD2 2D Y Offset, Wrap, Increment
const unsigned int mm_dma_bd2_2d_y = 0x0001d04c;
// DMA BD2 Dynamic packet header
const unsigned int mm_dma_bd2_packet = 0x0001d050;
// DMA BD2 Interleaved state
const unsigned int mm_dma_bd2_interleaved_state = 0x0001d054;
// DMA BD1 Length, Next BD, BD Count
const unsigned int mm_dma_bd2_control = 0x0001d058;
// DMA BD3 A Address and Lock
const unsigned int mm_dma_bd3_addr_a = 0x0001d060;
// DMA BD3 B Address and Lock
const unsigned int mm_dma_bd3_addr_b = 0x0001d064;
// DMA BD3 2D X Offset, Wrap, Increment
const unsigned int mm_dma_bd3_2d_x = 0x0001d068;
// DMA BD3 2D Y Offset, Wrap, Increment
const unsigned int mm_dma_bd3_2d_y = 0x0001d06c;
// DMA BD3 Dynamic packet header
const unsigned int mm_dma_bd3_packet = 0x0001d070;
// DMA BD3 Interleaved state
const unsigned int mm_dma_bd3_interleaved_state = 0x0001d074;
// DMA BD3 Length, Next BD, BD Count
const unsigned int mm_dma_bd3_control = 0x0001d078;
// DMA BD4 A Address and Lock
const unsigned int mm_dma_bd4_addr_a = 0x0001d080;
// DMA BD4 B Address and Lock
const unsigned int mm_dma_bd4_addr_b = 0x0001d084;
// DMA BD4 2D X Offset, Wrap, Increment
const unsigned int mm_dma_bd4_2d_x = 0x0001d088;
// DMA BD4 2D Y Offset, Wrap, Increment
const unsigned int mm_dma_bd4_2d_y = 0x0001d08c;
// DMA BD4 Dynamic packet header
const unsigned int mm_dma_bd4_packet = 0x0001d090;
// DMA BD4 Interleaved state
const unsigned int mm_dma_bd4_interleaved_state = 0x0001d094;
// DMA BD4 Length, Next BD, BD Count
const unsigned int mm_dma_bd4_control = 0x0001d098;
// DMA BD5 A Address and Lock
const unsigned int mm_dma_bd5_addr_a = 0x0001d0a0;
// DMA BD5 B Address and Lock
const unsigned int mm_dma_bd5_addr_b = 0x0001d0a4;
// DMA BD5 2D X Offset, Wrap, Increment
const unsigned int mm_dma_bd5_2d_x = 0x0001d0a8;
// DMA BD5 2D Y Offset, Wrap, Increment
const unsigned int mm_dma_bd5_2d_y = 0x0001d0ac;
// DMA BD5 Dynamic packet header
const unsigned int mm_dma_bd5_packet = 0x0001d0b0;
// DMA BD5 Interleaved state
const unsigned int mm_dma_bd5_interleaved_state = 0x0001d0b4;
// DMA BD5 Length, Next BD, BD Count
const unsigned int mm_dma_bd5_control = 0x0001d0b8;
// DMA BD6 A Address and Lock
const unsigned int mm_dma_bd6_addr_a = 0x0001d0c0;
// DMA BD6 B Address and Lock
const unsigned int mm_dma_bd6_addr_b = 0x0001d0c4;
// DMA BD6 2D X Offset, Wrap, Increment
const unsigned int mm_dma_bd6_2d_x = 0x0001d0c8;
// DMA BD6 2D Y Offset, Wrap, Increment
const unsigned int mm_dma_bd6_2d_y = 0x0001d0cc;
// DMA BD6 Dynamic packet header
const unsigned int mm_dma_bd6_packet = 0x0001d0d0;
// DMA BD6 Interleaved state
const unsigned int mm_dma_bd6_interleaved_state = 0x0001d0d4;
// DMA BD6 Length, Next BD, BD Count
const unsigned int mm_dma_bd6_control = 0x0001d0d8;
// DMA BD7 A Address and Lock
const unsigned int mm_dma_bd7_addr_a = 0x0001d0e0;
// DMA BD7 B Address and Lock
const unsigned int mm_dma_bd7_addr_b = 0x0001d0e4;
// DMA BD7 2D X Offset, Wrap, Increment
const unsigned int mm_dma_bd7_2d_x = 0x0001d0e8;
// DMA BD7 2D Y Offset, Wrap, Increment
const unsigned int mm_dma_bd7_2d_y = 0x0001d0ec;
// DMA BD7 Dynamic packet header
const unsigned int mm_dma_bd7_packet = 0x0001d0f0;
// DMA BD7 Interleaved state
const unsigned int mm_dma_bd7_interleaved_state = 0x0001d0f4;
// DMA BD7 Length, Next BD, BD Count
const unsigned int mm_dma_bd7_control = 0x0001d0f8;
// DMA BD8 A Address and Lock
const unsigned int mm_dma_bd8_addr_a = 0x0001d100;
// DMA BD8 B Address and Lock
const unsigned int mm_dma_bd8_addr_b = 0x0001d104;
// DMA BD8 2D X Offset, Wrap, Increment
const unsigned int mm_dma_bd8_2d_x = 0x0001d108;
// DMA BD8 2D Y Offset, Wrap, Increment
const unsigned int mm_dma_bd8_2d_y = 0x0001d10c;
// DMA BD8 Dynamic packet header
const unsigned int mm_dma_bd8_packet = 0x0001d110;
// DMA BD8 Interleaved state
const unsigned int mm_dma_bd8_interleaved_state = 0x0001d114;
// DMA BD8 Length, Next BD, BD Count
const unsigned int mm_dma_bd8_control = 0x0001d118;
// DMA BD9 A Address and Lock
const unsigned int mm_dma_bd9_addr_a = 0x0001d120;
// DMA BD9 B Address and Lock
const unsigned int mm_dma_bd9_addr_b = 0x0001d124;
// DMA BD9 2D X Offset, Wrap, Increment
const unsigned int mm_dma_bd9_2d_x = 0x0001d128;
// DMA BD9 2D Y Offset, Wrap, Increment
const unsigned int mm_dma_bd9_2d_y = 0x0001d12c;
// DMA BD9 Dynamic packet header
const unsigned int mm_dma_bd9_packet = 0x0001d130;
// DMA BD9 Interleaved state
const unsigned int mm_dma_bd9_interleaved_state = 0x0001d134;
// DMA BD9 Length, Next BD, BD Count
const unsigned int mm_dma_bd9_control = 0x0001d138;
// DMA BD10 A Address and Lock
const unsigned int mm_dma_bd10_addr_a = 0x0001d140;
// DMA BD10 B Address and Lock
const unsigned int mm_dma_bd10_addr_b = 0x0001d144;
// DMA BD10 2D X Offset, Wrap, Increment
const unsigned int mm_dma_bd10_2d_x = 0x0001d148;
// DMA BD10 2D Y Offset, Wrap, Increment
const unsigned int mm_dma_bd10_2d_y = 0x0001d14c;
// DMA BD10 Dynamic packet header
const unsigned int mm_dma_bd10_packet = 0x0001d150;
// DMA BD10 Interleaved state
const unsigned int mm_dma_bd10_interleaved_state = 0x0001d154;
// DMA BD10 Length, Next BD, BD Count
const unsigned int mm_dma_bd10_control = 0x0001d158;
// DMA BD11 A Address and Lock
const unsigned int mm_dma_bd11_addr_a = 0x0001d160;
// DMA BD11 B Address and Lock
const unsigned int mm_dma_bd11_addr_b = 0x0001d164;
// DMA BD11 2D X Offset, Wrap, Increment
const unsigned int mm_dma_bd11_2d_x = 0x0001d168;
// DMA BD11 2D Y Offset, Wrap, Increment
const unsigned int mm_dma_bd11_2d_y = 0x0001d16c;
// DMA BD11 Dynamic packet header
const unsigned int mm_dma_bd11_packet = 0x0001d170;
// DMA BD11 Interleaved state
const unsigned int mm_dma_bd11_interleaved_state = 0x0001d174;
// DMA BD11 Length, Next BD, BD Count
const unsigned int mm_dma_bd11_control = 0x0001d178;
// DMA BD12 A Address and Lock
const unsigned int mm_dma_bd12_addr_a = 0x0001d180;
// DMA BD12 B Address and Lock
const unsigned int mm_dma_bd12_addr_b = 0x0001d184;
// DMA BD12 2D X Offset, Wrap, Increment
const unsigned int mm_dma_bd12_2d_x = 0x0001d188;
// DMA BD12 2D Y Offset, Wrap, Increment
const unsigned int mm_dma_bd12_2d_y = 0x0001d18c;
// DMA BD12 Dynamic packet header
const unsigned int mm_dma_bd12_packet = 0x0001d190;
// DMA BD12 Interleaved state
const unsigned int mm_dma_bd12_interleaved_state = 0x0001d194;
// DMA BD12 Length, Next BD, BD Count
const unsigned int mm_dma_bd12_control = 0x0001d198;
// DMA BD13 A Address and Lock
const unsigned int mm_dma_bd13_addr_a = 0x0001d1a0;
// DMA BD13 B Address and Lock
const unsigned int mm_dma_bd13_addr_b = 0x0001d1a4;
// DMA BD13 2D X Offset, Wrap, Increment
const unsigned int mm_dma_bd13_2d_x = 0x0001d1a8;
// DMA BD13 2D Y Offset, Wrap, Increment
const unsigned int mm_dma_bd13_2d_y = 0x0001d1ac;
// DMA BD13 Dynamic packet header
const unsigned int mm_dma_bd13_packet = 0x0001d1b0;
// DMA BD13 Interleaved state
const unsigned int mm_dma_bd13_interleaved_state = 0x0001d1b4;
// DMA BD13 Length, Next BD, BD Count
const unsigned int mm_dma_bd13_control = 0x0001d1b8;
// DMA BD14 A Address and Lock
const unsigned int mm_dma_bd14_addr_a = 0x0001d1c0;
// DMA BD14 B Address and Lock
const unsigned int mm_dma_bd14_addr_b = 0x0001d1c4;
// DMA BD14 2D X Offset, Wrap, Increment
const unsigned int mm_dma_bd14_2d_x = 0x0001d1c8;
// DMA BD14 2D Y Offset, Wrap, Increment
const unsigned int mm_dma_bd14_2d_y = 0x0001d1cc;
// DMA BD14 Dynamic packet header
const unsigned int mm_dma_bd14_packet = 0x0001d1d0;
// DMA BD14 Interleaved state
const unsigned int mm_dma_bd14_interleaved_state = 0x0001d1d4;
// DMA BD14 Length, Next BD, BD Count
const unsigned int mm_dma_bd14_control = 0x0001d1d8;
// DMA BD15 A Address and Lock
const unsigned int mm_dma_bd15_addr_a = 0x0001d1e0;
// DMA BD15 B Address and Lock
const unsigned int mm_dma_bd15_addr_b = 0x0001d1e4;
// DMA BD15 2D X Offset, Wrap, Increment
const unsigned int mm_dma_bd15_2d_x = 0x0001d1e8;
// DMA BD15 2D Y Offset, Wrap, Increment
const unsigned int mm_dma_bd15_2d_y = 0x0001d1ec;
// DMA BD15 Dynamic packet header
const unsigned int mm_dma_bd15_packet = 0x0001d1f0;
// DMA BD15 Interleaved state
const unsigned int mm_dma_bd15_interleaved_state = 0x0001d1f4;
// DMA BD15 Length, Next BD, BD Count
const unsigned int mm_dma_bd15_control = 0x0001d1f8;
// DMA Control Register S2MM Ch0
const unsigned int mm_dma_s2mm_0_ctrl = 0x0001de00;
// DMA Control Register S2MM Ch0
const unsigned int mm_dma_s2mm_0_start_queue = 0x0001de04;
// DMA Control Register S2MM Ch1
const unsigned int mm_dma_s2mm_1_ctrl = 0x0001de08;
// DMA Control Register S2MM Ch1
const unsigned int mm_dma_s2mm_1_start_queue = 0x0001de0c;
// DMA Control Register MM2S Ch0
const unsigned int mm_dma_mm2s_0_ctrl = 0x0001de10;
// DMA Control Register MM2S Ch0
const unsigned int mm_dma_mm2s_0_start_queue = 0x0001de14;
// DMA Control Register MM2S Ch1
const unsigned int mm_dma_mm2s_1_ctrl = 0x0001de18;
// DMA Control Register MM2S Ch1
const unsigned int mm_dma_mm2s_1_start_queue = 0x0001de1c;
// DMA S2MM Status Register
const unsigned int mm_dma_s2mm_status = 0x0001df00;
// DMA MM2S Status Register
const unsigned int mm_dma_mm2s_status = 0x0001df10;
// DMA FIFO Counter Register
const unsigned int mm_dma_fifo_counter = 0x0001df20;
// Lock0 Release Request with No value
const unsigned int mm_lock0_release_nv = 0x0001e000;
// Lock0 Release Request with '0' value
const unsigned int mm_lock0_release_v0 = 0x0001e020;
// Lock0 Release Request with '1' value
const unsigned int mm_lock0_release_v1 = 0x0001e030;
// Lock0 Acquire Request with No value
const unsigned int mm_lock0_acquire_nv = 0x0001e040;
// Lock0 Acquire Request with '0' value
const unsigned int mm_lock0_acquire_v0 = 0x0001e060;
// Lock0 Acquire Request with '1' value
const unsigned int mm_lock0_acquire_v1 = 0x0001e070;
// Lock1 Release Request with No value
const unsigned int mm_lock1_release_nv = 0x0001e080;
// Lock1 Release Request with '0' value
const unsigned int mm_lock1_release_v0 = 0x0001e0a0;
// Lock1 Release Request with '1' value
const unsigned int mm_lock1_release_v1 = 0x0001e0b0;
// Lock1 Acquire Request with No value
const unsigned int mm_lock1_acquire_nv = 0x0001e0c0;
// Lock1 Acquire Request with '0' value
const unsigned int mm_lock1_acquire_v0 = 0x0001e0e0;
// Lock1 Acquire Request with '1' value
const unsigned int mm_lock1_acquire_v1 = 0x0001e0f0;
// Lock2 Release Request with No value
const unsigned int mm_lock2_release_nv = 0x0001e100;
// Lock2 Release Request with '0' value
const unsigned int mm_lock2_release_v0 = 0x0001e120;
// Lock2 Release Request with '1' value
const unsigned int mm_lock2_release_v1 = 0x0001e130;
// Lock2 Acquire Request with No value
const unsigned int mm_lock2_acquire_nv = 0x0001e140;
// Lock2 Acquire Request with '0' value
const unsigned int mm_lock2_acquire_v0 = 0x0001e160;
// Lock2 Acquire Request with '1' value
const unsigned int mm_lock2_acquire_v1 = 0x0001e170;
// Lock3 Release Request with No value
const unsigned int mm_lock3_release_nv = 0x0001e180;
// Lock3 Release Request with '0' value
const unsigned int mm_lock3_release_v0 = 0x0001e1a0;
// Lock3 Release Request with '1' value
const unsigned int mm_lock3_release_v1 = 0x0001e1b0;
// Lock3 Acquire Request with No value
const unsigned int mm_lock3_acquire_nv = 0x0001e1c0;
// Lock3 Acquire Request with '0' value
const unsigned int mm_lock3_acquire_v0 = 0x0001e1e0;
// Lock3 Acquire Request with '1' value
const unsigned int mm_lock3_acquire_v1 = 0x0001e1f0;
// Lock4 Release Request with No value
const unsigned int mm_lock4_release_nv = 0x0001e200;
// Lock4 Release Request with '0' value
const unsigned int mm_lock4_release_v0 = 0x0001e220;
// Lock4 Release Request with '1' value
const unsigned int mm_lock4_release_v1 = 0x0001e230;
// Lock4 Acquire Request with No value
const unsigned int mm_lock4_acquire_nv = 0x0001e240;
// Lock4 Acquire Request with '0' value
const unsigned int mm_lock4_acquire_v0 = 0x0001e260;
// Lock4 Acquire Request with '1' value
const unsigned int mm_lock4_acquire_v1 = 0x0001e270;
// Lock5 Release Request with No value
const unsigned int mm_lock5_release_nv = 0x0001e280;
// Lock5 Release Request with '0' value
const unsigned int mm_lock5_release_v0 = 0x0001e2a0;
// Lock5 Release Request with '1' value
const unsigned int mm_lock5_release_v1 = 0x0001e2b0;
// Lock5 Acquire Request with No value
const unsigned int mm_lock5_acquire_nv = 0x0001e2c0;
// Lock5 Acquire Request with '0' value
const unsigned int mm_lock5_acquire_v0 = 0x0001e2e0;
// Lock5 Acquire Request with '1' value
const unsigned int mm_lock5_acquire_v1 = 0x0001e2f0;
// Lock6 Release Request with No value
const unsigned int mm_lock6_release_nv = 0x0001e300;
// Lock6 Release Request with '0' value
const unsigned int mm_lock6_release_v0 = 0x0001e320;
// Lock6 Release Request with '1' value
const unsigned int mm_lock6_release_v1 = 0x0001e330;
// Lock6 Acquire Request with No value
const unsigned int mm_lock6_acquire_nv = 0x0001e340;
// Lock6 Acquire Request with '0' value
const unsigned int mm_lock6_acquire_v0 = 0x0001e360;
// Lock6 Acquire Request with '1' value
const unsigned int mm_lock6_acquire_v1 = 0x0001e370;
// Lock7 Release Request with No value
const unsigned int mm_lock7_release_nv = 0x0001e380;
// Lock7 Release Request with '0' value
const unsigned int mm_lock7_release_v0 = 0x0001e3a0;
// Lock7 Release Request with '1' value
const unsigned int mm_lock7_release_v1 = 0x0001e3b0;
// Lock7 Acquire Request with No value
const unsigned int mm_lock7_acquire_nv = 0x0001e3c0;
// Lock7 Acquire Request with '0' value
const unsigned int mm_lock7_acquire_v0 = 0x0001e3e0;
// Lock7 Acquire Request with '1' value
const unsigned int mm_lock7_acquire_v1 = 0x0001e3f0;
// Lock8 Release Request with No value
const unsigned int mm_lock8_release_nv = 0x0001e400;
// Lock8 Release Request with '0' value
const unsigned int mm_lock8_release_v0 = 0x0001e420;
// Lock8 Release Request with '1' value
const unsigned int mm_lock8_release_v1 = 0x0001e430;
// Lock8 Acquire Request with No value
const unsigned int mm_lock8_acquire_nv = 0x0001e440;
// Lock8 Acquire Request with '0' value
const unsigned int mm_lock8_acquire_v0 = 0x0001e460;
// Lock8 Acquire Request with '1' value
const unsigned int mm_lock8_acquire_v1 = 0x0001e470;
// Lock9 Release Request with No value
const unsigned int mm_lock9_release_nv = 0x0001e480;
// Lock9 Release Request with '0' value
const unsigned int mm_lock9_release_v0 = 0x0001e4a0;
// Lock9 Release Request with '1' value
const unsigned int mm_lock9_release_v1 = 0x0001e4b0;
// Lock9 Acquire Request with No value
const unsigned int mm_lock9_acquire_nv = 0x0001e4c0;
// Lock9 Acquire Request with '0' value
const unsigned int mm_lock9_acquire_v0 = 0x0001e4e0;
// Lock9 Acquire Request with '1' value
const unsigned int mm_lock9_acquire_v1 = 0x0001e4f0;
// Lock10 Release Request with No value
const unsigned int mm_lock10_release_nv = 0x0001e500;
// Lock10 Release Request with '0' value
const unsigned int mm_lock10_release_v0 = 0x0001e520;
// Lock10 Release Request with '1' value
const unsigned int mm_lock10_release_v1 = 0x0001e530;
// Lock10 Acquire Request with No value
const unsigned int mm_lock10_acquire_nv = 0x0001e540;
// Lock10 Acquire Request with '0' value
const unsigned int mm_lock10_acquire_v0 = 0x0001e560;
// Lock10 Acquire Request with '1' value
const unsigned int mm_lock10_acquire_v1 = 0x0001e570;
// Lock11 Release Request with No value
const unsigned int mm_lock11_release_nv = 0x0001e580;
// Lock11 Release Request with '0' value
const unsigned int mm_lock11_release_v0 = 0x0001e5a0;
// Lock11 Release Request with '1' value
const unsigned int mm_lock11_release_v1 = 0x0001e5b0;
// Lock11 Acquire Request with No value
const unsigned int mm_lock11_acquire_nv = 0x0001e5c0;
// Lock11 Acquire Request with '0' value
const unsigned int mm_lock11_acquire_v0 = 0x0001e5e0;
// Lock11 Acquire Request with '1' value
const unsigned int mm_lock11_acquire_v1 = 0x0001e5f0;
// Lock12 Release Request with No value
const unsigned int mm_lock12_release_nv = 0x0001e600;
// Lock12 Release Request with '0' value
const unsigned int mm_lock12_release_v0 = 0x0001e620;
// Lock12 Release Request with '1' value
const unsigned int mm_lock12_release_v1 = 0x0001e630;
// Lock12 Acquire Request with No value
const unsigned int mm_lock12_acquire_nv = 0x0001e640;
// Lock12 Acquire Request with '0' value
const unsigned int mm_lock12_acquire_v0 = 0x0001e660;
// Lock12 Acquire Request with '1' value
const unsigned int mm_lock12_acquire_v1 = 0x0001e670;
// Lock13 Release Request with No value
const unsigned int mm_lock13_release_nv = 0x0001e680;
// Lock13 Release Request with '0' value
const unsigned int mm_lock13_release_v0 = 0x0001e6a0;
// Lock13 Release Request with '1' value
const unsigned int mm_lock13_release_v1 = 0x0001e6b0;
// Lock13 Acquire Request with No value
const unsigned int mm_lock13_acquire_nv = 0x0001e6c0;
// Lock13 Acquire Request with '0' value
const unsigned int mm_lock13_acquire_v0 = 0x0001e6e0;
// Lock13 Acquire Request with '1' value
const unsigned int mm_lock13_acquire_v1 = 0x0001e6f0;
// Lock14 Release Request with No value
const unsigned int mm_lock14_release_nv = 0x0001e700;
// Lock14 Release Request with '0' value
const unsigned int mm_lock14_release_v0 = 0x0001e720;
// Lock14 Release Request with '1' value
const unsigned int mm_lock14_release_v1 = 0x0001e730;
// Lock14 Acquire Request with No value
const unsigned int mm_lock14_acquire_nv = 0x0001e740;
// Lock14 Acquire Request with '0' value
const unsigned int mm_lock14_acquire_v0 = 0x0001e760;
// Lock14 Acquire Request with '1' value
const unsigned int mm_lock14_acquire_v1 = 0x0001e770;
// Lock15 Release Request with No value
const unsigned int mm_lock15_release_nv = 0x0001e780;
// Lock15 Release Request with '0' value
const unsigned int mm_lock15_release_v0 = 0x0001e7a0;
// Lock15 Release Request with '1' value
const unsigned int mm_lock15_release_v1 = 0x0001e7b0;
// Lock15 Acquire Request with No value
const unsigned int mm_lock15_acquire_nv = 0x0001e7c0;
// Lock15 Acquire Request with '0' value
const unsigned int mm_lock15_acquire_v0 = 0x0001e7e0;
// Lock15 Acquire Request with '1' value
const unsigned int mm_lock15_acquire_v1 = 0x0001e7f0;
// All Locks Acquire Status and Value
const unsigned int mm_all_lock_state_value = 0x0001ef00;
// Control for which lock values events are generated
const unsigned int mm_lock_event_value_control_0 = 0x0001ef20;
// Control for which lock values events are generated
const unsigned int mm_lock_event_value_control_1 = 0x0001ef24;

// Register definitions for SHIM
// ###################################
// Step size between DMA BD register groups
const unsigned int shim_dma_bd_step_size = 0x14;
// Step size between DMA S2MM register groups
const unsigned int shim_dma_s2mm_step_size = 0x8;
// All Locks Acquire Status and Value
const unsigned int shim_all_lock_state_value = 0x00014f00;
// DMA BD0 Low Address
const unsigned int shim_dma_bd0_addr_low = 0x0001d000;
// DMA BD0 Buffer Length
const unsigned int shim_dma_bd0_buffer_length = 0x0001d004;
// DMA BD0 Control
const unsigned int shim_dma_bd0_control = 0x0001d008;
// DMA BD0 AXI Configuration
const unsigned int shim_dma_bd0_axi_config = 0x0001d00c;
// DMA BD0 Packet
const unsigned int shim_dma_bd0_packet = 0x0001d010;
// DMA S2MM Channel 0 Control
const unsigned int shim_dma_s2mm_0_ctrl = 0x0001d140;
// DMA S2MM Channel 0 Start Queue
const unsigned int shim_dma_s2mm_0_start_queue = 0x0001d144;
// Performance Counters 1-0 Start and Stop Events
const unsigned int shim_performance_control0 = 0x00031000;
const unsigned int shim_performance_start_stop_0_1 = 0x00031000;
// Performance Counters 1-0 Reset Events
const unsigned int shim_performance_control1 = 0x00031008;
const unsigned int shim_performance_reset_0_1 = 0x00031008;
// Performance Counter0
const unsigned int shim_performance_counter0 = 0x00031020;
// Performance Counter1
const unsigned int shim_performance_counter1 = 0x00031024;
// Performance Counter0 Event Value.
const unsigned int shim_performance_counter0_event_value = 0x00031080;
// Performance Counter1 Event Value. When the Performance Counter1 reach this value, an event will be generated
const unsigned int shim_performance_counter1_event_value = 0x00031084;
// Generate an internal event
const unsigned int shim_event_generate = 0x00034008;
// Control of which Internal Event to Broadcast0
const unsigned int shim_event_broadcast_a_0 = 0x00034010;
// Control of which Internal Event to Broadcast1
const unsigned int shim_event_broadcast_a_1 = 0x00034014;
// Control of which Internal Event to Broadcast2
const unsigned int shim_event_broadcast_a_2 = 0x00034018;
// Control of which Internal Event to Broadcast3
const unsigned int shim_event_broadcast_a_3 = 0x0003401c;
// Control of which Internal Event to Broadcast4
const unsigned int shim_event_broadcast_a_4 = 0x00034020;
// Control of which Internal Event to Broadcast5
const unsigned int shim_event_broadcast_a_5 = 0x00034024;
// Control of which Internal Event to Broadcast6
const unsigned int shim_event_broadcast_a_6 = 0x00034028;
// Control of which Internal Event to Broadcast7
const unsigned int shim_event_broadcast_a_7 = 0x0003402c;
// Control of which Internal Event to Broadcast8
const unsigned int shim_event_broadcast_a_8 = 0x00034030;
// Control of which Internal Event to Broadcast9
const unsigned int shim_event_broadcast_a_9 = 0x00034034;
// Control of which Internal Event to Broadcast10
const unsigned int shim_event_broadcast_a_10 = 0x00034038;
// Control of which Internal Event to Broadcast11
const unsigned int shim_event_broadcast_a_11 = 0x0003403c;
// Control of which Internal Event to Broadcast12
const unsigned int shim_event_broadcast_a_12 = 0x00034040;
// Control of which Internal Event to Broadcast13
const unsigned int shim_event_broadcast_a_13 = 0x00034044;
// Control of which Internal Event to Broadcast14
const unsigned int shim_event_broadcast_a_14 = 0x00034048;
// Control of which Internal Event to Broadcast15
const unsigned int shim_event_broadcast_a_15 = 0x0003404c;
// Set block of broadcast signals to South
const unsigned int shim_event_broadcast_a_block_south_set = 0x00034050;
// Clear block of broadcast signals to South
const unsigned int shim_event_broadcast_a_block_south_clr = 0x00034054;
// Current value of block for broadcast signals to South
const unsigned int shim_event_broadcast_a_block_south_value = 0x00034058;
// Set block of broadcast signals to West
const unsigned int shim_event_broadcast_a_block_west_set = 0x00034060;
// Clear block of broadcast signals to West
const unsigned int shim_event_broadcast_a_block_west_clr = 0x00034064;
// Current value of block for broadcast signals to West
const unsigned int shim_event_broadcast_a_block_west_value = 0x00034068;
// Set block of broadcast signals to North
const unsigned int shim_event_broadcast_a_block_north_set = 0x00034070;
// Clear block of broadcast signals to North
const unsigned int shim_event_broadcast_a_block_north_clr = 0x00034074;
// Current value of block for broadcast signals to North
const unsigned int shim_event_broadcast_a_block_north_value = 0x00034078;
// Set block of broadcast signals to East
const unsigned int shim_event_broadcast_a_block_east_set = 0x00034080;
// Clear block of broadcast signals to East
const unsigned int shim_event_broadcast_a_block_east_clr = 0x00034084;
// Current value of block for broadcast signals to East
const unsigned int shim_event_broadcast_a_block_east_value = 0x00034088;
// Control of Trace
const unsigned int shim_trace_control0 = 0x000340d0;
// Control of Trace: packet destination
const unsigned int shim_trace_control1 = 0x000340d4;
// Status of trace engine
const unsigned int shim_trace_status = 0x000340d8;
// Control of which Internal Event to Broadcast
const unsigned int shim_trace_event0 = 0x000340e0;
// Control of which Internal Event to Broadcast
const unsigned int shim_trace_event1 = 0x000340e4;
// Internal Timer Event Value.
const unsigned int shim_timer_trig_event_low_value = 0x000340f0;
// Internal Timer Event Value.
const unsigned int shim_timer_trig_event_high_value = 0x000340f4;
// Internal Timer Low part Value.
const unsigned int shim_timer_low = 0x000340f8;
// Internal Timer High part Value.
const unsigned int shim_timer_high = 0x000340fc;
// Internal event status register0
const unsigned int shim_event_status0 = 0x00034200;
// Internal event status register1
const unsigned int shim_event_status1 = 0x00034204;
// Internal event status register2
const unsigned int shim_event_status2 = 0x00034208;
// Internal event status register3
const unsigned int shim_event_status3 = 0x0003420c;
// Combo events input events
const unsigned int shim_combo_event_inputs = 0x00034400;
// Combo events input events
const unsigned int shim_combo_event_control = 0x00034404;
// Event enable for DMA Group
const unsigned int shim_event_group_dma_enable = 0x00034504;
// Stream Switch Ports 0-3 for event generation
const unsigned int shim_stream_switch_event_port_selection_0 = 0x0003ff00;
// Stream Switch Ports 4-7 for event generation
const unsigned int shim_stream_switch_event_port_selection_1 = 0x0003ff04;

// Register definitions for MEM
// ###################################
// NOTE: these are dummy values needed by scripts but never used
// Performance Counter0
const unsigned int mem_performance_counter0 = 0x0;

} // namespace aie1

#endif /* AIE1_REGISTERS_H_ */
