// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef AIE2_REGISTERS_H_
#define AIE2_REGISTERS_H_

namespace aie2
{

// Register definitions for AIE2
// ###################################

// Register definitions for CM
// ###################################
// Program Counter
const unsigned int cm_program_counter = 0x00031100;
// Performance Counters 1-0 Start and Stop Events
const unsigned int cm_performance_control0 = 0x00031500;
// Performance Counters 3-2 Start and Stop Events
const unsigned int cm_performance_control1 = 0x00031504;
// Performance Counters Reset Events
const unsigned int cm_performance_control2 = 0x00031508;
// Performance Counter0
const unsigned int cm_performance_counter0 = 0x00031520;
// Performance Counter1
const unsigned int cm_performance_counter1 = 0x00031524;
// Performance Counter2
const unsigned int cm_performance_counter2 = 0x00031528;
// Performance Counter3
const unsigned int cm_performance_counter3 = 0x0003152c;
// Performance Counter0 Event Value.
const unsigned int cm_performance_counter0_event_value = 0x00031580;
// Performance Counter1 Event Value. When the Performance Counter1 reach this value, an event will be generated
const unsigned int cm_performance_counter1_event_value = 0x00031584;
// Performance Counter2 Event Value. When the Performance Counter2 reach this value, an event will be generated
const unsigned int cm_performance_counter2_event_value = 0x00031588;
// Performance Counter3 Event Value. When the Performance Counter3 reach this value, an event will be generated
const unsigned int cm_performance_counter3_event_value = 0x0003158c;
// Control of the AI Engine
const unsigned int cm_core_control = 0x00032000;
// The status of the AI Engine
const unsigned int cm_core_status = 0x00032004;
// Set enable events
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
// Core Processor Bus Control
const unsigned int cm_core_processor_bus = 0x00032038;
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
// Control of which Internal Event to Trace
const unsigned int cm_trace_event0 = 0x000340e0;
// Control of which Internal Event to Trace
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
// Configuration for edge detection events
const unsigned int cm_edge_detection_event_control = 0x00034408;
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
// Control Accumulator Cascade
const unsigned int cm_accumulator_control = 0x00036060;
// Control for memory (privileged)
const unsigned int cm_memory_control = 0x00036070;
// Stream Switch Master Configuration AI Engine 0
const unsigned int cm_stream_switch_master_config_aie_core0 = 0x0003f000;
// Stream Switch Master Configuration DMA 0
const unsigned int cm_stream_switch_master_config_dma0 = 0x0003f004;
// Stream Switch Master Configuration DMA 1
const unsigned int cm_stream_switch_master_config_dma1 = 0x0003f008;
// Stream Switch Master Configuration AI Engine Tile Ctrl
const unsigned int cm_stream_switch_master_config_tile_ctrl = 0x0003f00c;
// Stream Switch Master Configuration FIFO 0
const unsigned int cm_stream_switch_master_config_fifo0 = 0x0003f010;
// Stream Switch Master Configuration South 0
const unsigned int cm_stream_switch_master_config_south0 = 0x0003f014;
// Stream Switch Master Configuration South 1
const unsigned int cm_stream_switch_master_config_south1 = 0x0003f018;
// Stream Switch Master Configuration South 2
const unsigned int cm_stream_switch_master_config_south2 = 0x0003f01c;
// Stream Switch Master Configuration South 3
const unsigned int cm_stream_switch_master_config_south3 = 0x0003f020;
// Stream Switch Master Configuration West 0
const unsigned int cm_stream_switch_master_config_west0 = 0x0003f024;
// Stream Switch Master Configuration West 1
const unsigned int cm_stream_switch_master_config_west1 = 0x0003f028;
// Stream Switch Master Configuration West 2
const unsigned int cm_stream_switch_master_config_west2 = 0x0003f02c;
// Stream Switch Master Configuration West 3
const unsigned int cm_stream_switch_master_config_west3 = 0x0003f030;
// Stream Switch Master Configuration North 0
const unsigned int cm_stream_switch_master_config_north0 = 0x0003f034;
// Stream Switch Master Configuration North 1
const unsigned int cm_stream_switch_master_config_north1 = 0x0003f038;
// Stream Switch Master Configuration North 2
const unsigned int cm_stream_switch_master_config_north2 = 0x0003f03c;
// Stream Switch Master Configuration North 3
const unsigned int cm_stream_switch_master_config_north3 = 0x0003f040;
// Stream Switch Master Configuration North 4
const unsigned int cm_stream_switch_master_config_north4 = 0x0003f044;
// Stream Switch Master Configuration North 5
const unsigned int cm_stream_switch_master_config_north5 = 0x0003f048;
// Stream Switch Master Configuration East 0
const unsigned int cm_stream_switch_master_config_east0 = 0x0003f04c;
// Stream Switch Master Configuration East 1
const unsigned int cm_stream_switch_master_config_east1 = 0x0003f050;
// Stream Switch Master Configuration East 2
const unsigned int cm_stream_switch_master_config_east2 = 0x0003f054;
// Stream Switch Master Configuration East 3
const unsigned int cm_stream_switch_master_config_east3 = 0x0003f058;
// Stream Switch Slave Configuration AI Engine 0
const unsigned int cm_stream_switch_slave_config_aie_core0 = 0x0003f100;
// Stream Switch Slave Configuration DMA 0
const unsigned int cm_stream_switch_slave_config_dma_0 = 0x0003f104;
// Stream Switch Slave Configuration DMA 1
const unsigned int cm_stream_switch_slave_config_dma_1 = 0x0003f108;
// Stream Switch Slave Configuration Tile Ctrl
const unsigned int cm_stream_switch_slave_config_tile_ctrl = 0x0003f10c;
// Stream Switch Slave Configuration FIFO 0
const unsigned int cm_stream_switch_slave_config_fifo_0 = 0x0003f110;
// Stream Switch Slave Configuration South 0
const unsigned int cm_stream_switch_slave_config_south_0 = 0x0003f114;
// Stream Switch Slave Configuration South 1
const unsigned int cm_stream_switch_slave_config_south_1 = 0x0003f118;
// Stream Switch Slave Configuration South 2
const unsigned int cm_stream_switch_slave_config_south_2 = 0x0003f11c;
// Stream Switch Slave Configuration South 3
const unsigned int cm_stream_switch_slave_config_south_3 = 0x0003f120;
// Stream Switch Slave Configuration South 4
const unsigned int cm_stream_switch_slave_config_south_4 = 0x0003f124;
// Stream Switch Slave Configuration South 5
const unsigned int cm_stream_switch_slave_config_south_5 = 0x0003f128;
// Stream Switch Slave Configuration West 0
const unsigned int cm_stream_switch_slave_config_west_0 = 0x0003f12c;
// Stream Switch Slave Configuration West 1
const unsigned int cm_stream_switch_slave_config_west_1 = 0x0003f130;
// Stream Switch Slave Configuration West 2
const unsigned int cm_stream_switch_slave_config_west_2 = 0x0003f134;
// Stream Switch Slave Configuration West 3
const unsigned int cm_stream_switch_slave_config_west_3 = 0x0003f138;
// Stream Switch Slave Configuration North 0
const unsigned int cm_stream_switch_slave_config_north_0 = 0x0003f13c;
// Stream Switch Slave Configuration North 1
const unsigned int cm_stream_switch_slave_config_north_1 = 0x0003f140;
// Stream Switch Slave Configuration North 2
const unsigned int cm_stream_switch_slave_config_north_2 = 0x0003f144;
// Stream Switch Slave Configuration North 3
const unsigned int cm_stream_switch_slave_config_north_3 = 0x0003f148;
// Stream Switch Slave Configuration East 0
const unsigned int cm_stream_switch_slave_config_east_0 = 0x0003f14c;
// Stream Switch Slave Configuration East 1
const unsigned int cm_stream_switch_slave_config_east_1 = 0x0003f150;
// Stream Switch Slave Configuration East 2
const unsigned int cm_stream_switch_slave_config_east_2 = 0x0003f154;
// Stream Switch Slave Configuration East 3
const unsigned int cm_stream_switch_slave_config_east_3 = 0x0003f158;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int cm_stream_switch_slave_config_aie_trace = 0x0003f15c;
// Stream Switch Slave Configuration Mem Trace
const unsigned int cm_stream_switch_slave_config_mem_trace = 0x0003f160;
// Stream Switch Slave Configuration AI Engine 0
const unsigned int cm_stream_switch_slave_aie_core0_slot0 = 0x0003f200;
// Stream Switch Slave Configuration AI Engine 0
const unsigned int cm_stream_switch_slave_aie_core0_slot1 = 0x0003f204;
// Stream Switch Slave Configuration AI Engine 0
const unsigned int cm_stream_switch_slave_aie_core0_slot2 = 0x0003f208;
// Stream Switch Slave Configuration AI Engine 0
const unsigned int cm_stream_switch_slave_aie_core0_slot3 = 0x0003f20c;
// Stream Switch Slave Configuration DMA 0
const unsigned int cm_stream_switch_slave_dma_0_slot0 = 0x0003f210;
// Stream Switch Slave Configuration DMA 0
const unsigned int cm_stream_switch_slave_dma_0_slot1 = 0x0003f214;
// Stream Switch Slave Configuration DMA 0
const unsigned int cm_stream_switch_slave_dma_0_slot2 = 0x0003f218;
// Stream Switch Slave Configuration DMA 0
const unsigned int cm_stream_switch_slave_dma_0_slot3 = 0x0003f21c;
// Stream Switch Slave Configuration DMA 1
const unsigned int cm_stream_switch_slave_dma_1_slot0 = 0x0003f220;
// Stream Switch Slave Configuration DMA 1
const unsigned int cm_stream_switch_slave_dma_1_slot1 = 0x0003f224;
// Stream Switch Slave Configuration DMA 1
const unsigned int cm_stream_switch_slave_dma_1_slot2 = 0x0003f228;
// Stream Switch Slave Configuration DMA 1
const unsigned int cm_stream_switch_slave_dma_1_slot3 = 0x0003f22c;
// Stream Switch Slave Configuration Tile Ctrl
const unsigned int cm_stream_switch_slave_tile_ctrl_slot0 = 0x0003f230;
// Stream Switch Slave Configuration Tile Ctrl
const unsigned int cm_stream_switch_slave_tile_ctrl_slot1 = 0x0003f234;
// Stream Switch Slave Configuration Tile Ctrl
const unsigned int cm_stream_switch_slave_tile_ctrl_slot2 = 0x0003f238;
// Stream Switch Slave Configuration Tile Ctrl
const unsigned int cm_stream_switch_slave_tile_ctrl_slot3 = 0x0003f23c;
// Stream Switch Slave Configuration FIFO 0
const unsigned int cm_stream_switch_slave_fifo_0_slot0 = 0x0003f240;
// Stream Switch Slave Configuration FIFO 0
const unsigned int cm_stream_switch_slave_fifo_0_slot1 = 0x0003f244;
// Stream Switch Slave Configuration FIFO 0
const unsigned int cm_stream_switch_slave_fifo_0_slot2 = 0x0003f248;
// Stream Switch Slave Configuration FIFO 0
const unsigned int cm_stream_switch_slave_fifo_0_slot3 = 0x0003f24c;
// Stream Switch Slave Configuration South 0
const unsigned int cm_stream_switch_slave_south_0_slot0 = 0x0003f250;
// Stream Switch Slave Configuration South 0
const unsigned int cm_stream_switch_slave_south_0_slot1 = 0x0003f254;
// Stream Switch Slave Configuration South 0
const unsigned int cm_stream_switch_slave_south_0_slot2 = 0x0003f258;
// Stream Switch Slave Configuration South 0
const unsigned int cm_stream_switch_slave_south_0_slot3 = 0x0003f25c;
// Stream Switch Slave Configuration South 1
const unsigned int cm_stream_switch_slave_south_1_slot0 = 0x0003f260;
// Stream Switch Slave Configuration South 1
const unsigned int cm_stream_switch_slave_south_1_slot1 = 0x0003f264;
// Stream Switch Slave Configuration South 1
const unsigned int cm_stream_switch_slave_south_1_slot2 = 0x0003f268;
// Stream Switch Slave Configuration South 1
const unsigned int cm_stream_switch_slave_south_1_slot3 = 0x0003f26c;
// Stream Switch Slave Configuration South 2
const unsigned int cm_stream_switch_slave_south_2_slot0 = 0x0003f270;
// Stream Switch Slave Configuration South 2
const unsigned int cm_stream_switch_slave_south_2_slot1 = 0x0003f274;
// Stream Switch Slave Configuration South 2
const unsigned int cm_stream_switch_slave_south_2_slot2 = 0x0003f278;
// Stream Switch Slave Configuration South 2
const unsigned int cm_stream_switch_slave_south_2_slot3 = 0x0003f27c;
// Stream Switch Slave Configuration South 3
const unsigned int cm_stream_switch_slave_south_3_slot0 = 0x0003f280;
// Stream Switch Slave Configuration South 3
const unsigned int cm_stream_switch_slave_south_3_slot1 = 0x0003f284;
// Stream Switch Slave Configuration South 3
const unsigned int cm_stream_switch_slave_south_3_slot2 = 0x0003f288;
// Stream Switch Slave Configuration South 3
const unsigned int cm_stream_switch_slave_south_3_slot3 = 0x0003f28c;
// Stream Switch Slave Configuration South 4
const unsigned int cm_stream_switch_slave_south_4_slot0 = 0x0003f290;
// Stream Switch Slave Configuration South 4
const unsigned int cm_stream_switch_slave_south_4_slot1 = 0x0003f294;
// Stream Switch Slave Configuration South 4
const unsigned int cm_stream_switch_slave_south_4_slot2 = 0x0003f298;
// Stream Switch Slave Configuration South 4
const unsigned int cm_stream_switch_slave_south_4_slot3 = 0x0003f29c;
// Stream Switch Slave Configuration South 5
const unsigned int cm_stream_switch_slave_south_5_slot0 = 0x0003f2a0;
// Stream Switch Slave Configuration South 5
const unsigned int cm_stream_switch_slave_south_5_slot1 = 0x0003f2a4;
// Stream Switch Slave Configuration South 5
const unsigned int cm_stream_switch_slave_south_5_slot2 = 0x0003f2a8;
// Stream Switch Slave Configuration South 5
const unsigned int cm_stream_switch_slave_south_5_slot3 = 0x0003f2ac;
// Stream Switch Slave Configuration West 0
const unsigned int cm_stream_switch_slave_west_0_slot0 = 0x0003f2b0;
// Stream Switch Slave Configuration West 0
const unsigned int cm_stream_switch_slave_west_0_slot1 = 0x0003f2b4;
// Stream Switch Slave Configuration West 0
const unsigned int cm_stream_switch_slave_west_0_slot2 = 0x0003f2b8;
// Stream Switch Slave Configuration West 0
const unsigned int cm_stream_switch_slave_west_0_slot3 = 0x0003f2bc;
// Stream Switch Slave Configuration West 1
const unsigned int cm_stream_switch_slave_west_1_slot0 = 0x0003f2c0;
// Stream Switch Slave Configuration West 1
const unsigned int cm_stream_switch_slave_west_1_slot1 = 0x0003f2c4;
// Stream Switch Slave Configuration West 1
const unsigned int cm_stream_switch_slave_west_1_slot2 = 0x0003f2c8;
// Stream Switch Slave Configuration West 1
const unsigned int cm_stream_switch_slave_west_1_slot3 = 0x0003f2cc;
// Stream Switch Slave Configuration West 2
const unsigned int cm_stream_switch_slave_west_2_slot0 = 0x0003f2d0;
// Stream Switch Slave Configuration West 2
const unsigned int cm_stream_switch_slave_west_2_slot1 = 0x0003f2d4;
// Stream Switch Slave Configuration West 2
const unsigned int cm_stream_switch_slave_west_2_slot2 = 0x0003f2d8;
// Stream Switch Slave Configuration West 2
const unsigned int cm_stream_switch_slave_west_2_slot3 = 0x0003f2dc;
// Stream Switch Slave Configuration West 3
const unsigned int cm_stream_switch_slave_west_3_slot0 = 0x0003f2e0;
// Stream Switch Slave Configuration West 3
const unsigned int cm_stream_switch_slave_west_3_slot1 = 0x0003f2e4;
// Stream Switch Slave Configuration West 3
const unsigned int cm_stream_switch_slave_west_3_slot2 = 0x0003f2e8;
// Stream Switch Slave Configuration West 3
const unsigned int cm_stream_switch_slave_west_3_slot3 = 0x0003f2ec;
// Stream Switch Slave Configuration North 0
const unsigned int cm_stream_switch_slave_north_0_slot0 = 0x0003f2f0;
// Stream Switch Slave Configuration North 0
const unsigned int cm_stream_switch_slave_north_0_slot1 = 0x0003f2f4;
// Stream Switch Slave Configuration North 0
const unsigned int cm_stream_switch_slave_north_0_slot2 = 0x0003f2f8;
// Stream Switch Slave Configuration North 0
const unsigned int cm_stream_switch_slave_north_0_slot3 = 0x0003f2fc;
// Stream Switch Slave Configuration North 1
const unsigned int cm_stream_switch_slave_north_1_slot0 = 0x0003f300;
// Stream Switch Slave Configuration North 1
const unsigned int cm_stream_switch_slave_north_1_slot1 = 0x0003f304;
// Stream Switch Slave Configuration North 1
const unsigned int cm_stream_switch_slave_north_1_slot2 = 0x0003f308;
// Stream Switch Slave Configuration North 1
const unsigned int cm_stream_switch_slave_north_1_slot3 = 0x0003f30c;
// Stream Switch Slave Configuration North 2
const unsigned int cm_stream_switch_slave_north_2_slot0 = 0x0003f310;
// Stream Switch Slave Configuration North 2
const unsigned int cm_stream_switch_slave_north_2_slot1 = 0x0003f314;
// Stream Switch Slave Configuration North 2
const unsigned int cm_stream_switch_slave_north_2_slot2 = 0x0003f318;
// Stream Switch Slave Configuration North 2
const unsigned int cm_stream_switch_slave_north_2_slot3 = 0x0003f31c;
// Stream Switch Slave Configuration North 3
const unsigned int cm_stream_switch_slave_north_3_slot0 = 0x0003f320;
// Stream Switch Slave Configuration North 3
const unsigned int cm_stream_switch_slave_north_3_slot1 = 0x0003f324;
// Stream Switch Slave Configuration North 3
const unsigned int cm_stream_switch_slave_north_3_slot2 = 0x0003f328;
// Stream Switch Slave Configuration North 3
const unsigned int cm_stream_switch_slave_north_3_slot3 = 0x0003f32c;
// Stream Switch Slave Configuration East 0
const unsigned int cm_stream_switch_slave_east_0_slot0 = 0x0003f330;
// Stream Switch Slave Configuration East 0
const unsigned int cm_stream_switch_slave_east_0_slot1 = 0x0003f334;
// Stream Switch Slave Configuration East 0
const unsigned int cm_stream_switch_slave_east_0_slot2 = 0x0003f338;
// Stream Switch Slave Configuration East 0
const unsigned int cm_stream_switch_slave_east_0_slot3 = 0x0003f33c;
// Stream Switch Slave Configuration East 1
const unsigned int cm_stream_switch_slave_east_1_slot0 = 0x0003f340;
// Stream Switch Slave Configuration East 1
const unsigned int cm_stream_switch_slave_east_1_slot1 = 0x0003f344;
// Stream Switch Slave Configuration East 1
const unsigned int cm_stream_switch_slave_east_1_slot2 = 0x0003f348;
// Stream Switch Slave Configuration East 1
const unsigned int cm_stream_switch_slave_east_1_slot3 = 0x0003f34c;
// Stream Switch Slave Configuration East 2
const unsigned int cm_stream_switch_slave_east_2_slot0 = 0x0003f350;
// Stream Switch Slave Configuration East 2
const unsigned int cm_stream_switch_slave_east_2_slot1 = 0x0003f354;
// Stream Switch Slave Configuration East 2
const unsigned int cm_stream_switch_slave_east_2_slot2 = 0x0003f358;
// Stream Switch Slave Configuration East 2
const unsigned int cm_stream_switch_slave_east_2_slot3 = 0x0003f35c;
// Stream Switch Slave Configuration East 3
const unsigned int cm_stream_switch_slave_east_3_slot0 = 0x0003f360;
// Stream Switch Slave Configuration East 3
const unsigned int cm_stream_switch_slave_east_3_slot1 = 0x0003f364;
// Stream Switch Slave Configuration East 3
const unsigned int cm_stream_switch_slave_east_3_slot2 = 0x0003f368;
// Stream Switch Slave Configuration East 3
const unsigned int cm_stream_switch_slave_east_3_slot3 = 0x0003f36c;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int cm_stream_switch_slave_aie_trace_slot0 = 0x0003f370;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int cm_stream_switch_slave_aie_trace_slot1 = 0x0003f374;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int cm_stream_switch_slave_aie_trace_slot2 = 0x0003f378;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int cm_stream_switch_slave_aie_trace_slot3 = 0x0003f37c;
// Stream Switch Slave Configuration Mem Trace
const unsigned int cm_stream_switch_slave_mem_trace_slot0 = 0x0003f380;
// Stream Switch Slave Configuration Mem Trace
const unsigned int cm_stream_switch_slave_mem_trace_slot1 = 0x0003f384;
// Stream Switch Slave Configuration Mem Trace
const unsigned int cm_stream_switch_slave_mem_trace_slot2 = 0x0003f388;
// Stream Switch Slave Configuration Mem Trace
const unsigned int cm_stream_switch_slave_mem_trace_slot3 = 0x0003f38c;
// Stream Switch Deterministic Merge Arbiter:0 Slave:0,1
const unsigned int cm_stream_switch_deterministic_merge_arb0_slave0_1 = 0x0003f800;
// Stream Switch Deterministic Merge Arbiter:0 Slave:2,3
const unsigned int cm_stream_switch_deterministic_merge_arb0_slave2_3 = 0x0003f804;
// Stream Switch Deterministic Merge Arbiter:0 Control
const unsigned int cm_stream_switch_deterministic_merge_arb0_ctrl = 0x0003f808;
// Stream Switch Deterministic Merge Arbiter:1 Slave:0,1
const unsigned int cm_stream_switch_deterministic_merge_arb1_slave0_1 = 0x0003f810;
// Stream Switch Deterministic Merge Arbiter:1 Slave:2,3
const unsigned int cm_stream_switch_deterministic_merge_arb1_slave2_3 = 0x0003f814;
// Stream Switch Deterministic Merge Arbiter:1 Control
const unsigned int cm_stream_switch_deterministic_merge_arb1_ctrl = 0x0003f818;
// Select Stream Switch Ports for event generation
const unsigned int cm_stream_switch_event_port_selection_0 = 0x0003ff00;
// Select Stream Switch Ports for event generation
const unsigned int cm_stream_switch_event_port_selection_1 = 0x0003ff04;
// Status bits for Parity errors on stream switch ports
const unsigned int cm_stream_switch_parity_status = 0x0003ff10;
// Injection of Parity errors on stream switch ports
const unsigned int cm_stream_switch_parity_injection = 0x0003ff20;
// Status of control packet handling
const unsigned int cm_tile_control_packet_handler_status = 0x0003ff30;
// Status of Stream Switch Adaptive Clock Gate Status
const unsigned int cm_stream_switch_adaptive_clock_gate_status = 0x0003ff34;
// Status of Stream Switch Adaptive Clock Gate Abort Period
const unsigned int cm_stream_switch_adaptive_clock_gate_abort_period = 0x0003ff38;
// Control clock gating of modules (privileged)
const unsigned int cm_module_clock_control = 0x00060000;
// Reset of modules (privileged)
const unsigned int cm_module_reset_control = 0x00060010;


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
// Control of which Internal Event to Trace
const unsigned int mm_trace_event0 = 0x000140e0;
// Control of which Internal Event to Trace
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
// Configuration for edge detection events
const unsigned int mm_edge_detection_event_control = 0x00014408;
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
// Control for memory (privileged)
const unsigned int mm_memory_control = 0x00016010;
// DMA BD0 0
const unsigned int mm_dma_bd0_0 = 0x0001d000;
// DMA BD0 1
const unsigned int mm_dma_bd0_1 = 0x0001d004;
// DMA BD0 2
const unsigned int mm_dma_bd0_2 = 0x0001d008;
// DMA BD0 3
const unsigned int mm_dma_bd0_3 = 0x0001d00c;
// DMA BD0 4
const unsigned int mm_dma_bd0_4 = 0x0001d010;
// DMA BD0 5
const unsigned int mm_dma_bd0_5 = 0x0001d014;
// DMA BD1 0
const unsigned int mm_dma_bd1_0 = 0x0001d020;
// DMA BD1 1
const unsigned int mm_dma_bd1_1 = 0x0001d024;
// DMA BD1 2
const unsigned int mm_dma_bd1_2 = 0x0001d028;
// DMA BD1 3
const unsigned int mm_dma_bd1_3 = 0x0001d02c;
// DMA BD1 4
const unsigned int mm_dma_bd1_4 = 0x0001d030;
// DMA BD1 5
const unsigned int mm_dma_bd1_5 = 0x0001d034;
// DMA BD2 0
const unsigned int mm_dma_bd2_0 = 0x0001d040;
// DMA BD2 1
const unsigned int mm_dma_bd2_1 = 0x0001d044;
// DMA BD2 2
const unsigned int mm_dma_bd2_2 = 0x0001d048;
// DMA BD2 3
const unsigned int mm_dma_bd2_3 = 0x0001d04c;
// DMA BD2 4
const unsigned int mm_dma_bd2_4 = 0x0001d050;
// DMA BD2 5
const unsigned int mm_dma_bd2_5 = 0x0001d054;
// DMA BD3 0
const unsigned int mm_dma_bd3_0 = 0x0001d060;
// DMA BD3 1
const unsigned int mm_dma_bd3_1 = 0x0001d064;
// DMA BD3 2
const unsigned int mm_dma_bd3_2 = 0x0001d068;
// DMA BD3 3
const unsigned int mm_dma_bd3_3 = 0x0001d06c;
// DMA BD3 4
const unsigned int mm_dma_bd3_4 = 0x0001d070;
// DMA BD3 5
const unsigned int mm_dma_bd3_5 = 0x0001d074;
// DMA BD4 0
const unsigned int mm_dma_bd4_0 = 0x0001d080;
// DMA BD4 1
const unsigned int mm_dma_bd4_1 = 0x0001d084;
// DMA BD4 2
const unsigned int mm_dma_bd4_2 = 0x0001d088;
// DMA BD4 3
const unsigned int mm_dma_bd4_3 = 0x0001d08c;
// DMA BD4 4
const unsigned int mm_dma_bd4_4 = 0x0001d090;
// DMA BD4 5
const unsigned int mm_dma_bd4_5 = 0x0001d094;
// DMA BD5 0
const unsigned int mm_dma_bd5_0 = 0x0001d0a0;
// DMA BD5 1
const unsigned int mm_dma_bd5_1 = 0x0001d0a4;
// DMA BD5 2
const unsigned int mm_dma_bd5_2 = 0x0001d0a8;
// DMA BD5 3
const unsigned int mm_dma_bd5_3 = 0x0001d0ac;
// DMA BD5 4
const unsigned int mm_dma_bd5_4 = 0x0001d0b0;
// DMA BD5 5
const unsigned int mm_dma_bd5_5 = 0x0001d0b4;
// DMA BD6 0
const unsigned int mm_dma_bd6_0 = 0x0001d0c0;
// DMA BD6 1
const unsigned int mm_dma_bd6_1 = 0x0001d0c4;
// DMA BD6 2
const unsigned int mm_dma_bd6_2 = 0x0001d0c8;
// DMA BD6 3
const unsigned int mm_dma_bd6_3 = 0x0001d0cc;
// DMA BD6 4
const unsigned int mm_dma_bd6_4 = 0x0001d0d0;
// DMA BD6 5
const unsigned int mm_dma_bd6_5 = 0x0001d0d4;
// DMA BD7 0
const unsigned int mm_dma_bd7_0 = 0x0001d0e0;
// DMA BD7 1
const unsigned int mm_dma_bd7_1 = 0x0001d0e4;
// DMA BD7 2
const unsigned int mm_dma_bd7_2 = 0x0001d0e8;
// DMA BD7 3
const unsigned int mm_dma_bd7_3 = 0x0001d0ec;
// DMA BD7 4
const unsigned int mm_dma_bd7_4 = 0x0001d0f0;
// DMA BD7 5
const unsigned int mm_dma_bd7_5 = 0x0001d0f4;
// DMA BD8 0
const unsigned int mm_dma_bd8_0 = 0x0001d100;
// DMA BD8 1
const unsigned int mm_dma_bd8_1 = 0x0001d104;
// DMA BD8 2
const unsigned int mm_dma_bd8_2 = 0x0001d108;
// DMA BD8 3
const unsigned int mm_dma_bd8_3 = 0x0001d10c;
// DMA BD8 4
const unsigned int mm_dma_bd8_4 = 0x0001d110;
// DMA BD8 5
const unsigned int mm_dma_bd8_5 = 0x0001d114;
// DMA BD9 0
const unsigned int mm_dma_bd9_0 = 0x0001d120;
// DMA BD9 1
const unsigned int mm_dma_bd9_1 = 0x0001d124;
// DMA BD9 2
const unsigned int mm_dma_bd9_2 = 0x0001d128;
// DMA BD9 3
const unsigned int mm_dma_bd9_3 = 0x0001d12c;
// DMA BD9 4
const unsigned int mm_dma_bd9_4 = 0x0001d130;
// DMA BD9 5
const unsigned int mm_dma_bd9_5 = 0x0001d134;
// DMA BD10 0
const unsigned int mm_dma_bd10_0 = 0x0001d140;
// DMA BD10 1
const unsigned int mm_dma_bd10_1 = 0x0001d144;
// DMA BD10 2
const unsigned int mm_dma_bd10_2 = 0x0001d148;
// DMA BD10 3
const unsigned int mm_dma_bd10_3 = 0x0001d14c;
// DMA BD10 4
const unsigned int mm_dma_bd10_4 = 0x0001d150;
// DMA BD10 5
const unsigned int mm_dma_bd10_5 = 0x0001d154;
// DMA BD11 01
const unsigned int mm_dma_bd11_0 = 0x0001d160;
// DMA BD11 1
const unsigned int mm_dma_bd11_1 = 0x0001d164;
// DMA BD11 2
const unsigned int mm_dma_bd11_2 = 0x0001d168;
// DMA BD11 3
const unsigned int mm_dma_bd11_3 = 0x0001d16c;
// DMA BD11 4
const unsigned int mm_dma_bd11_4 = 0x0001d170;
// DMA BD11 5
const unsigned int mm_dma_bd11_5 = 0x0001d174;
// DMA BD12 01
const unsigned int mm_dma_bd12_0 = 0x0001d180;
// DMA BD12 1
const unsigned int mm_dma_bd12_1 = 0x0001d184;
// DMA BD12 2
const unsigned int mm_dma_bd12_2 = 0x0001d188;
// DMA BD12 3
const unsigned int mm_dma_bd12_3 = 0x0001d18c;
// DMA BD12 4
const unsigned int mm_dma_bd12_4 = 0x0001d190;
// DMA BD12 5
const unsigned int mm_dma_bd12_5 = 0x0001d194;
// DMA BD13 01
const unsigned int mm_dma_bd13_0 = 0x0001d1a0;
// DMA BD13 1
const unsigned int mm_dma_bd13_1 = 0x0001d1a4;
// DMA BD13 2
const unsigned int mm_dma_bd13_2 = 0x0001d1a8;
// DMA BD13 3
const unsigned int mm_dma_bd13_3 = 0x0001d1ac;
// DMA BD13 4
const unsigned int mm_dma_bd13_4 = 0x0001d1b0;
// DMA BD13 5
const unsigned int mm_dma_bd13_5 = 0x0001d1b4;
// DMA BD14 01
const unsigned int mm_dma_bd14_0 = 0x0001d1c0;
// DMA BD14 1
const unsigned int mm_dma_bd14_1 = 0x0001d1c4;
// DMA BD14 2
const unsigned int mm_dma_bd14_2 = 0x0001d1c8;
// DMA BD14 3
const unsigned int mm_dma_bd14_3 = 0x0001d1cc;
// DMA BD14 4
const unsigned int mm_dma_bd14_4 = 0x0001d1d0;
// DMA BD14 5
const unsigned int mm_dma_bd14_5 = 0x0001d1d4;
// DMA BD15 01
const unsigned int mm_dma_bd15_0 = 0x0001d1e0;
// DMA BD15 1
const unsigned int mm_dma_bd15_1 = 0x0001d1e4;
// DMA BD15 2
const unsigned int mm_dma_bd15_2 = 0x0001d1e8;
// DMA BD15 3
const unsigned int mm_dma_bd15_3 = 0x0001d1ec;
// DMA BD15 4
const unsigned int mm_dma_bd15_4 = 0x0001d1f0;
// DMA BD15 5
const unsigned int mm_dma_bd15_5 = 0x0001d1f4;
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
// DMA S2MM Status Register Ch0
const unsigned int mm_dma_s2mm_status_0 = 0x0001df00;
// DMA S2MM Status Register Ch1
const unsigned int mm_dma_s2mm_status_1 = 0x0001df04;
// DMA MM2S Status Register Ch0
const unsigned int mm_dma_mm2s_status_0 = 0x0001df10;
// DMA MM2S Status Register Ch1
const unsigned int mm_dma_mm2s_status_1 = 0x0001df14;
// DMA S2MM Current Write Count Ch0
const unsigned int mm_dma_s2mm_current_write_count_0 = 0x0001df18;
// DMA S2MM Current Write Count Ch1
const unsigned int mm_dma_s2mm_current_write_count_1 = 0x0001df1c;
// DMA S2MM FoT Count FIFO Pop Ch0
const unsigned int mm_dma_s2mm_fot_count_fifo_pop_0 = 0x0001df20;
// DMA S2MM FoT Count FIFO Pop Ch1
const unsigned int mm_dma_s2mm_fot_count_fifo_pop_1 = 0x0001df24;
// Value of lock 0
const unsigned int mm_lock0_value = 0x0001f000;
// Value of lock 1
const unsigned int mm_lock1_value = 0x0001f010;
// Value of lock 2
const unsigned int mm_lock2_value = 0x0001f020;
// Value of lock 3
const unsigned int mm_lock3_value = 0x0001f030;
// Value of lock 4
const unsigned int mm_lock4_value = 0x0001f040;
// Value of lock 5
const unsigned int mm_lock5_value = 0x0001f050;
// Value of lock 6
const unsigned int mm_lock6_value = 0x0001f060;
// Value of lock 7
const unsigned int mm_lock7_value = 0x0001f070;
// Value of lock 8
const unsigned int mm_lock8_value = 0x0001f080;
// Value of lock 9
const unsigned int mm_lock9_value = 0x0001f090;
// Value of lock 10
const unsigned int mm_lock10_value = 0x0001f0a0;
// Value of lock 11
const unsigned int mm_lock11_value = 0x0001f0b0;
// Value of lock 12
const unsigned int mm_lock12_value = 0x0001f0c0;
// Value of lock 13
const unsigned int mm_lock13_value = 0x0001f0d0;
// Value of lock 14
const unsigned int mm_lock14_value = 0x0001f0e0;
// Value of lock 15
const unsigned int mm_lock15_value = 0x0001f0f0;
// Select lock for lock event generation 0
const unsigned int mm_locks_event_selection_0 = 0x0001f100;
// Select lock for lock event generation 1
const unsigned int mm_locks_event_selection_1 = 0x0001f104;
// Select lock for lock event generation 2
const unsigned int mm_locks_event_selection_2 = 0x0001f108;
// Select lock for lock event generation 3
const unsigned int mm_locks_event_selection_3 = 0x0001f10c;
// Select lock for lock event generation 4
const unsigned int mm_locks_event_selection_4 = 0x0001f110;
// Select lock for lock event generation 5
const unsigned int mm_locks_event_selection_5 = 0x0001f114;
// Select lock for lock event generation 6
const unsigned int mm_locks_event_selection_6 = 0x0001f118;
// Select lock for lock event generation 7
const unsigned int mm_locks_event_selection_7 = 0x0001f11c;
// Status bits for lock overflow, write to clear
const unsigned int mm_locks_overflow = 0x0001f120;
// Status bits for lock underflow, write to clear
const unsigned int mm_locks_underflow = 0x0001f128;
// Lock Request. 16kB address space: = 0x40000 - = 0x43FFC, Lock_Id [13:10], Acq_Rel (9), Change_Value [8:2]
const unsigned int mm_lock_request = 0x00040000;

// Register definitions for MEM
// ###################################
// Performance Counters 1-0 Start and Stop Events
const unsigned int mem_performance_control0 = 0x00091000;
// Performance Counters 3-2 Start and Stop Events
const unsigned int mem_performance_control1 = 0x00091004;
// Performance Counters Reset Events
const unsigned int mem_performance_control2 = 0x00091008;
// Performance Counter0
const unsigned int mem_performance_counter0 = 0x00091020;
// Performance Counter1
const unsigned int mem_performance_counter1 = 0x00091024;
// Performance Counter2
const unsigned int mem_performance_counter2 = 0x00091028;
// Performance Counter3
const unsigned int mem_performance_counter3 = 0x0009102c;
// Performance Counter0 Event Value.
const unsigned int mem_performance_counter0_event_value = 0x00091080;
// Performance Counter1 Event Value.
const unsigned int mem_performance_counter1_event_value = 0x00091084;
// Performance Counter2 Event Value.
const unsigned int mem_performance_counter2_event_value = 0x00091088;
// Performance Counter3 Event Value.
const unsigned int mem_performance_counter3_event_value = 0x0009108c;
// Inhibits ECC check bits update to memory on writes
const unsigned int mem_checkbit_error_generation = 0x00092000;
// ECC Scrubbing Event
const unsigned int mem_ecc_scrubbing_event = 0x00092110;
// ECC Failing Address
const unsigned int mem_ecc_failing_address = 0x00092120;
// Control of Internal Timer
const unsigned int mem_timer_control = 0x00094000;
// Generate an internal event
const unsigned int mem_event_generate = 0x00094008;
// Control of which Internal Event to Broadcast0
const unsigned int mem_event_broadcast0 = 0x00094010;
// Control of which Internal Event to Broadcast1
const unsigned int mem_event_broadcast1 = 0x00094014;
// Control of which Internal Event to Broadcast2
const unsigned int mem_event_broadcast2 = 0x00094018;
// Control of which Internal Event to Broadcast3
const unsigned int mem_event_broadcast3 = 0x0009401c;
// Control of which Internal Event to Broadcast4
const unsigned int mem_event_broadcast4 = 0x00094020;
// Control of which Internal Event to Broadcast5
const unsigned int mem_event_broadcast5 = 0x00094024;
// Control of which Internal Event to Broadcast6
const unsigned int mem_event_broadcast6 = 0x00094028;
// Control of which Internal Event to Broadcast7
const unsigned int mem_event_broadcast7 = 0x0009402c;
// Control of which Internal Event to Broadcast8
const unsigned int mem_event_broadcast8 = 0x00094030;
// Control of which Internal Event to Broadcast9
const unsigned int mem_event_broadcast9 = 0x00094034;
// Control of which Internal Event to Broadcast10
const unsigned int mem_event_broadcast10 = 0x00094038;
// Control of which Internal Event to Broadcast11
const unsigned int mem_event_broadcast11 = 0x0009403c;
// Control of which Internal Event to Broadcast12
const unsigned int mem_event_broadcast12 = 0x00094040;
// Control of which Internal Event to Broadcast13
const unsigned int mem_event_broadcast13 = 0x00094044;
// Control of which Internal Event to Broadcast14
const unsigned int mem_event_broadcast14 = 0x00094048;
// Control of which Internal Event to Broadcast15
const unsigned int mem_event_broadcast15 = 0x0009404c;
// Set block of broadcast signals to South
const unsigned int mem_event_broadcast_a_block_south_set = 0x00094050;
// Clear block of broadcast signals to South
const unsigned int mem_event_broadcast_a_block_south_clr = 0x00094054;
// Current value of block for broadcast signals to South
const unsigned int mem_event_broadcast_a_block_south_value = 0x00094058;
// Set block of broadcast switch A signals to West
const unsigned int mem_event_broadcast_a_block_west_set = 0x00094060;
// Clear block of broadcast switch A signals to West
const unsigned int mem_event_broadcast_a_block_west_clr = 0x00094064;
// Current value of block for broadcast switch A signals to West
const unsigned int mem_event_broadcast_a_block_west_value = 0x00094068;
// Set block of broadcast switch A signals to North
const unsigned int mem_event_broadcast_a_block_north_set = 0x00094070;
// Clear block of broadcast switch A signals to North
const unsigned int mem_event_broadcast_a_block_north_clr = 0x00094074;
// Current value of block for broadcast switch A signals to North
const unsigned int mem_event_broadcast_a_block_north_value = 0x00094078;
// Set block of broadcast switch A signals to East
const unsigned int mem_event_broadcast_a_block_east_set = 0x00094080;
// Clear block of broadcast switch A signals to East
const unsigned int mem_event_broadcast_a_block_east_clr = 0x00094084;
// Current value of block for broadcast switch A signals to East
const unsigned int mem_event_broadcast_a_block_east_value = 0x00094088;
// Set block of broadcast switch B signals to South
const unsigned int mem_event_broadcast_b_block_south_set = 0x00094090;
// Clear block of broadcast switch B signals to South
const unsigned int mem_event_broadcast_b_block_south_clr = 0x00094094;
// Current value of block for broadcast switch B signals to South
const unsigned int mem_event_broadcast_b_block_south_value = 0x00094098;
// Set block of broadcast switch B signals to West
const unsigned int mem_event_broadcast_b_block_west_set = 0x000940a0;
// Clear block of broadcast switch B signals to West
const unsigned int mem_event_broadcast_b_block_west_clr = 0x000940a4;
// Current value of block for broadcast switch B signals to West
const unsigned int mem_event_broadcast_b_block_west_value = 0x000940a8;
// Set block of broadcast switch B signals to North
const unsigned int mem_event_broadcast_b_block_north_set = 0x000940b0;
// Clear block of broadcast switch B signals to North
const unsigned int mem_event_broadcast_b_block_north_clr = 0x000940b4;
// Current value of block for broadcast switch B signals to North
const unsigned int mem_event_broadcast_b_block_north_value = 0x000940b8;
// Set block of broadcast switch B signals to East
const unsigned int mem_event_broadcast_b_block_east_set = 0x000940c0;
// Clear block of broadcast switch B signals to East
const unsigned int mem_event_broadcast_b_block_east_clr = 0x000940c4;
// Current value of block for broadcast switch B signals to East
const unsigned int mem_event_broadcast_b_block_east_value = 0x000940c8;
// Control of Trace
const unsigned int mem_trace_control0 = 0x000940d0;
// Control of Trace packet configuration
const unsigned int mem_trace_control1 = 0x000940d4;
// Status of trace engine
const unsigned int mem_trace_status = 0x000940d8;
// Control of which Internal Event to Trace
const unsigned int mem_trace_event0 = 0x000940e0;
// Control of which Internal Event to Trace
const unsigned int mem_trace_event1 = 0x000940e4;
// Internal Timer Event Value.
const unsigned int mem_timer_trig_event_low_value = 0x000940f0;
// Internal Timer Event Value.
const unsigned int mem_timer_trig_event_high_value = 0x000940f4;
// Internal Timer Low part Value.
const unsigned int mem_timer_low = 0x000940f8;
// Internal Timer High part Value.
const unsigned int mem_timer_high = 0x000940fc;
// Define Watchpoint0
const unsigned int mem_watchpoint0 = 0x00094100;
// Define Watchpoint1
const unsigned int mem_watchpoint1 = 0x00094104;
// Define Watchpoint2
const unsigned int mem_watchpoint2 = 0x00094108;
// Define Watchpoint3
const unsigned int mem_watchpoint3 = 0x0009410c;
// Internal event status register0
const unsigned int mem_event_status0 = 0x00094200;
// Internal event status register1
const unsigned int mem_event_status1 = 0x00094204;
// Internal event status register2
const unsigned int mem_event_status2 = 0x00094208;
// Internal event status register3
const unsigned int mem_event_status3 = 0x0009420c;
// Internal event status register4
const unsigned int mem_event_status4 = 0x00094210;
// Internal event status register5
const unsigned int mem_event_status5 = 0x00094214;
// Reserved
const unsigned int mem_reserved0 = 0x00094220;
// Reserved
const unsigned int mem_reserved1 = 0x00094224;
// Reserved
const unsigned int mem_reserved2 = 0x00094228;
// Reserved
const unsigned int mem_reserved3 = 0x0009422c;
// Combo events input events
const unsigned int mem_combo_event_inputs = 0x00094400;
// Combo events input events
const unsigned int mem_combo_event_control = 0x00094404;
// Configuration for edge detection events
const unsigned int mem_edge_detection_event_control = 0x00094408;
// Event enable for Group 0
const unsigned int mem_event_group_0_enable = 0x00094500;
// Event enable for Watchpoint Group
const unsigned int mem_event_group_watchpoint_enable = 0x00094504;
// Event enable for DMA Group
const unsigned int mem_event_group_dma_enable = 0x00094508;
// Event enable for Lock Group
const unsigned int mem_event_group_lock_enable = 0x0009450c;
// Event enable for Stream Switch Group
const unsigned int mem_event_group_stream_switch_enable = 0x00094510;
// Event enable for Memory Conflict Group
const unsigned int mem_event_group_memory_conflict_enable = 0x00094514;
// Event enable for Error Group
const unsigned int mem_event_group_error_enable = 0x00094518;
// Event enable for Broadcast Group
const unsigned int mem_event_group_broadcast_enable = 0x0009451c;
// Event enable for User Group
const unsigned int mem_event_group_user_event_enable = 0x00094520;
// Spare register
const unsigned int mem_spare_reg = 0x00096000;
// Tile control register
const unsigned int mem_tile_control = 0x00096030;
// Trigger for CSSD
const unsigned int mem_cssd_trigger = 0x00096040;
// Control for memory (privileged)
const unsigned int mem_memory_control = 0x00096048;
// DMA BD0
const unsigned int mem_dma_bd0_0 = 0x000a0000;
// DMA BD0 1
const unsigned int mem_dma_bd0_1 = 0x000a0004;
// DMA BD0 2
const unsigned int mem_dma_bd0_2 = 0x000a0008;
// DMA BD0 3
const unsigned int mem_dma_bd0_3 = 0x000a000c;
// DMA BD0 4
const unsigned int mem_dma_bd0_4 = 0x000a0010;
// DMA BD0 5
const unsigned int mem_dma_bd0_5 = 0x000a0014;
// DMA BD0 6
const unsigned int mem_dma_bd0_6 = 0x000a0018;
// DMA BD0 7
const unsigned int mem_dma_bd0_7 = 0x000a001c;
// DMA BD1
const unsigned int mem_dma_bd1_0 = 0x000a0020;
// DMA BD1 1
const unsigned int mem_dma_bd1_1 = 0x000a0024;
// DMA BD1 2
const unsigned int mem_dma_bd1_2 = 0x000a0028;
// DMA BD1 3
const unsigned int mem_dma_bd1_3 = 0x000a002c;
// DMA BD1 4
const unsigned int mem_dma_bd1_4 = 0x000a0030;
// DMA BD1 5
const unsigned int mem_dma_bd1_5 = 0x000a0034;
// DMA BD1 6
const unsigned int mem_dma_bd1_6 = 0x000a0038;
// DMA BD1 7
const unsigned int mem_dma_bd1_7 = 0x000a003c;
// DMA BD2
const unsigned int mem_dma_bd2_0 = 0x000a0040;
// DMA BD2 1
const unsigned int mem_dma_bd2_1 = 0x000a0044;
// DMA BD2 2
const unsigned int mem_dma_bd2_2 = 0x000a0048;
// DMA BD2 3
const unsigned int mem_dma_bd2_3 = 0x000a004c;
// DMA BD2 4
const unsigned int mem_dma_bd2_4 = 0x000a0050;
// DMA BD2 5
const unsigned int mem_dma_bd2_5 = 0x000a0054;
// DMA BD2 6
const unsigned int mem_dma_bd2_6 = 0x000a0058;
// DMA BD2 7
const unsigned int mem_dma_bd2_7 = 0x000a005c;
// DMA BD3
const unsigned int mem_dma_bd3_0 = 0x000a0060;
// DMA BD3 1
const unsigned int mem_dma_bd3_1 = 0x000a0064;
// DMA BD3 2
const unsigned int mem_dma_bd3_2 = 0x000a0068;
// DMA BD3 3
const unsigned int mem_dma_bd3_3 = 0x000a006c;
// DMA BD3 4
const unsigned int mem_dma_bd3_4 = 0x000a0070;
// DMA BD3 5
const unsigned int mem_dma_bd3_5 = 0x000a0074;
// DMA BD3 6
const unsigned int mem_dma_bd3_6 = 0x000a0078;
// DMA BD3 7
const unsigned int mem_dma_bd3_7 = 0x000a007c;
// DMA BD4
const unsigned int mem_dma_bd4_0 = 0x000a0080;
// DMA BD4 1
const unsigned int mem_dma_bd4_1 = 0x000a0084;
// DMA BD4 2
const unsigned int mem_dma_bd4_2 = 0x000a0088;
// DMA BD4 3
const unsigned int mem_dma_bd4_3 = 0x000a008c;
// DMA BD4 4
const unsigned int mem_dma_bd4_4 = 0x000a0090;
// DMA BD4 5
const unsigned int mem_dma_bd4_5 = 0x000a0094;
// DMA BD4 6
const unsigned int mem_dma_bd4_6 = 0x000a0098;
// DMA BD4 7
const unsigned int mem_dma_bd4_7 = 0x000a009c;
// DMA BD5
const unsigned int mem_dma_bd5_0 = 0x000a00a0;
// DMA BD5 1
const unsigned int mem_dma_bd5_1 = 0x000a00a4;
// DMA BD5 2
const unsigned int mem_dma_bd5_2 = 0x000a00a8;
// DMA BD5 3
const unsigned int mem_dma_bd5_3 = 0x000a00ac;
// DMA BD5 4
const unsigned int mem_dma_bd5_4 = 0x000a00b0;
// DMA BD5 5
const unsigned int mem_dma_bd5_5 = 0x000a00b4;
// DMA BD5 6
const unsigned int mem_dma_bd5_6 = 0x000a00b8;
// DMA BD5 7
const unsigned int mem_dma_bd5_7 = 0x000a00bc;
// DMA BD6
const unsigned int mem_dma_bd6_0 = 0x000a00c0;
// DMA BD6 1
const unsigned int mem_dma_bd6_1 = 0x000a00c4;
// DMA BD6 2
const unsigned int mem_dma_bd6_2 = 0x000a00c8;
// DMA BD6 3
const unsigned int mem_dma_bd6_3 = 0x000a00cc;
// DMA BD6 4
const unsigned int mem_dma_bd6_4 = 0x000a00d0;
// DMA BD6 5
const unsigned int mem_dma_bd6_5 = 0x000a00d4;
// DMA BD6 6
const unsigned int mem_dma_bd6_6 = 0x000a00d8;
// DMA BD6 7
const unsigned int mem_dma_bd6_7 = 0x000a00dc;
// DMA BD7
const unsigned int mem_dma_bd7_0 = 0x000a00e0;
// DMA BD7 1
const unsigned int mem_dma_bd7_1 = 0x000a00e4;
// DMA BD7 2
const unsigned int mem_dma_bd7_2 = 0x000a00e8;
// DMA BD7 3
const unsigned int mem_dma_bd7_3 = 0x000a00ec;
// DMA BD7 4
const unsigned int mem_dma_bd7_4 = 0x000a00f0;
// DMA BD7 5
const unsigned int mem_dma_bd7_5 = 0x000a00f4;
// DMA BD7 6
const unsigned int mem_dma_bd7_6 = 0x000a00f8;
// DMA BD7 7
const unsigned int mem_dma_bd7_7 = 0x000a00fc;
// DMA BD8
const unsigned int mem_dma_bd8_0 = 0x000a0100;
// DMA BD8 1
const unsigned int mem_dma_bd8_1 = 0x000a0104;
// DMA BD8 2
const unsigned int mem_dma_bd8_2 = 0x000a0108;
// DMA BD8 3
const unsigned int mem_dma_bd8_3 = 0x000a010c;
// DMA BD8 4
const unsigned int mem_dma_bd8_4 = 0x000a0110;
// DMA BD8 5
const unsigned int mem_dma_bd8_5 = 0x000a0114;
// DMA BD8 6
const unsigned int mem_dma_bd8_6 = 0x000a0118;
// DMA BD8 7
const unsigned int mem_dma_bd8_7 = 0x000a011c;
// DMA BD9
const unsigned int mem_dma_bd9_0 = 0x000a0120;
// DMA BD9 1
const unsigned int mem_dma_bd9_1 = 0x000a0124;
// DMA BD9 2
const unsigned int mem_dma_bd9_2 = 0x000a0128;
// DMA BD9 3
const unsigned int mem_dma_bd9_3 = 0x000a012c;
// DMA BD9 4
const unsigned int mem_dma_bd9_4 = 0x000a0130;
// DMA BD9 5
const unsigned int mem_dma_bd9_5 = 0x000a0134;
// DMA BD9 6
const unsigned int mem_dma_bd9_6 = 0x000a0138;
// DMA BD9 7
const unsigned int mem_dma_bd9_7 = 0x000a013c;
// DMA BD1
const unsigned int mem_dma_bd10_0 = 0x000a0140;
// DMA BD10 1
const unsigned int mem_dma_bd10_1 = 0x000a0144;
// DMA BD10 2
const unsigned int mem_dma_bd10_2 = 0x000a0148;
// DMA BD10 3
const unsigned int mem_dma_bd10_3 = 0x000a014c;
// DMA BD10 4
const unsigned int mem_dma_bd10_4 = 0x000a0150;
// DMA BD10 5
const unsigned int mem_dma_bd10_5 = 0x000a0154;
// DMA BD10 6
const unsigned int mem_dma_bd10_6 = 0x000a0158;
// DMA BD10 7
const unsigned int mem_dma_bd10_7 = 0x000a015c;
// DMA BD11
const unsigned int mem_dma_bd11_0 = 0x000a0160;
// DMA BD11 1
const unsigned int mem_dma_bd11_1 = 0x000a0164;
// DMA BD11 2
const unsigned int mem_dma_bd11_2 = 0x000a0168;
// DMA BD11 3
const unsigned int mem_dma_bd11_3 = 0x000a016c;
// DMA BD11 4
const unsigned int mem_dma_bd11_4 = 0x000a0170;
// DMA BD11 5
const unsigned int mem_dma_bd11_5 = 0x000a0174;
// DMA BD11 6
const unsigned int mem_dma_bd11_6 = 0x000a0178;
// DMA BD11 7
const unsigned int mem_dma_bd11_7 = 0x000a017c;
// DMA BD12
const unsigned int mem_dma_bd12_0 = 0x000a0180;
// DMA BD12 1
const unsigned int mem_dma_bd12_1 = 0x000a0184;
// DMA BD12 2
const unsigned int mem_dma_bd12_2 = 0x000a0188;
// DMA BD12 3
const unsigned int mem_dma_bd12_3 = 0x000a018c;
// DMA BD12 4
const unsigned int mem_dma_bd12_4 = 0x000a0190;
// DMA BD12 5
const unsigned int mem_dma_bd12_5 = 0x000a0194;
// DMA BD12 6
const unsigned int mem_dma_bd12_6 = 0x000a0198;
// DMA BD12 7
const unsigned int mem_dma_bd12_7 = 0x000a019c;
// DMA BD13
const unsigned int mem_dma_bd13_0 = 0x000a01a0;
// DMA BD13 1
const unsigned int mem_dma_bd13_1 = 0x000a01a4;
// DMA BD13 2
const unsigned int mem_dma_bd13_2 = 0x000a01a8;
// DMA BD13 3
const unsigned int mem_dma_bd13_3 = 0x000a01ac;
// DMA BD13 4
const unsigned int mem_dma_bd13_4 = 0x000a01b0;
// DMA BD13 5
const unsigned int mem_dma_bd13_5 = 0x000a01b4;
// DMA BD13 6
const unsigned int mem_dma_bd13_6 = 0x000a01b8;
// DMA BD13 7
const unsigned int mem_dma_bd13_7 = 0x000a01bc;
// DMA BD14
const unsigned int mem_dma_bd14_0 = 0x000a01c0;
// DMA BD14 1
const unsigned int mem_dma_bd14_1 = 0x000a01c4;
// DMA BD14 2
const unsigned int mem_dma_bd14_2 = 0x000a01c8;
// DMA BD14 3
const unsigned int mem_dma_bd14_3 = 0x000a01cc;
// DMA BD14 4
const unsigned int mem_dma_bd14_4 = 0x000a01d0;
// DMA BD14 5
const unsigned int mem_dma_bd14_5 = 0x000a01d4;
// DMA BD14 6
const unsigned int mem_dma_bd14_6 = 0x000a01d8;
// DMA BD14 7
const unsigned int mem_dma_bd14_7 = 0x000a01dc;
// DMA BD15
const unsigned int mem_dma_bd15_0 = 0x000a01e0;
// DMA BD15 1
const unsigned int mem_dma_bd15_1 = 0x000a01e4;
// DMA BD15 2
const unsigned int mem_dma_bd15_2 = 0x000a01e8;
// DMA BD15 3
const unsigned int mem_dma_bd15_3 = 0x000a01ec;
// DMA BD15 4
const unsigned int mem_dma_bd15_4 = 0x000a01f0;
// DMA BD15 5
const unsigned int mem_dma_bd15_5 = 0x000a01f4;
// DMA BD15 6
const unsigned int mem_dma_bd15_6 = 0x000a01f8;
// DMA BD15 7
const unsigned int mem_dma_bd15_7 = 0x000a01fc;
// DMA BD16
const unsigned int mem_dma_bd16_0 = 0x000a0200;
// DMA BD16 1
const unsigned int mem_dma_bd16_1 = 0x000a0204;
// DMA BD16 2
const unsigned int mem_dma_bd16_2 = 0x000a0208;
// DMA BD16 3
const unsigned int mem_dma_bd16_3 = 0x000a020c;
// DMA BD16 4
const unsigned int mem_dma_bd16_4 = 0x000a0210;
// DMA BD16 5
const unsigned int mem_dma_bd16_5 = 0x000a0214;
// DMA BD16 6
const unsigned int mem_dma_bd16_6 = 0x000a0218;
// DMA BD16 7
const unsigned int mem_dma_bd16_7 = 0x000a021c;
// DMA BD17
const unsigned int mem_dma_bd17_0 = 0x000a0220;
// DMA BD17 1
const unsigned int mem_dma_bd17_1 = 0x000a0224;
// DMA BD17 2
const unsigned int mem_dma_bd17_2 = 0x000a0228;
// DMA BD17 3
const unsigned int mem_dma_bd17_3 = 0x000a022c;
// DMA BD17 4
const unsigned int mem_dma_bd17_4 = 0x000a0230;
// DMA BD17 5
const unsigned int mem_dma_bd17_5 = 0x000a0234;
// DMA BD17 6
const unsigned int mem_dma_bd17_6 = 0x000a0238;
// DMA BD17 7
const unsigned int mem_dma_bd17_7 = 0x000a023c;
// DMA BD15
const unsigned int mem_dma_bd18_0 = 0x000a0240;
// DMA BD18 1
const unsigned int mem_dma_bd18_1 = 0x000a0244;
// DMA BD18 2
const unsigned int mem_dma_bd18_2 = 0x000a0248;
// DMA BD18 3
const unsigned int mem_dma_bd18_3 = 0x000a024c;
// DMA BD18 4
const unsigned int mem_dma_bd18_4 = 0x000a0250;
// DMA BD18 5
const unsigned int mem_dma_bd18_5 = 0x000a0254;
// DMA BD18 6
const unsigned int mem_dma_bd18_6 = 0x000a0258;
// DMA BD18 7
const unsigned int mem_dma_bd18_7 = 0x000a025c;
// DMA BD19
const unsigned int mem_dma_bd19_0 = 0x000a0260;
// DMA BD19 1
const unsigned int mem_dma_bd19_1 = 0x000a0264;
// DMA BD19 2
const unsigned int mem_dma_bd19_2 = 0x000a0268;
// DMA BD19 3
const unsigned int mem_dma_bd19_3 = 0x000a026c;
// DMA BD19 4
const unsigned int mem_dma_bd19_4 = 0x000a0270;
// DMA BD19 5
const unsigned int mem_dma_bd19_5 = 0x000a0274;
// DMA BD19 6
const unsigned int mem_dma_bd19_6 = 0x000a0278;
// DMA BD19 7
const unsigned int mem_dma_bd19_7 = 0x000a027c;
// DMA BD2
const unsigned int mem_dma_bd20_0 = 0x000a0280;
// DMA BD20 1
const unsigned int mem_dma_bd20_1 = 0x000a0284;
// DMA BD20 2
const unsigned int mem_dma_bd20_2 = 0x000a0288;
// DMA BD20 3
const unsigned int mem_dma_bd20_3 = 0x000a028c;
// DMA BD20 4
const unsigned int mem_dma_bd20_4 = 0x000a0290;
// DMA BD20 5
const unsigned int mem_dma_bd20_5 = 0x000a0294;
// DMA BD20 6
const unsigned int mem_dma_bd20_6 = 0x000a0298;
// DMA BD20 7
const unsigned int mem_dma_bd20_7 = 0x000a029c;
// DMA BD21
const unsigned int mem_dma_bd21_0 = 0x000a02a0;
// DMA BD21 1
const unsigned int mem_dma_bd21_1 = 0x000a02a4;
// DMA BD21 2
const unsigned int mem_dma_bd21_2 = 0x000a02a8;
// DMA BD21 3
const unsigned int mem_dma_bd21_3 = 0x000a02ac;
// DMA BD21 4
const unsigned int mem_dma_bd21_4 = 0x000a02b0;
// DMA BD21 5
const unsigned int mem_dma_bd21_5 = 0x000a02b4;
// DMA BD21 6
const unsigned int mem_dma_bd21_6 = 0x000a02b8;
// DMA BD21 7
const unsigned int mem_dma_bd21_7 = 0x000a02bc;
// DMA BD22
const unsigned int mem_dma_bd22_0 = 0x000a02c0;
// DMA BD22 1
const unsigned int mem_dma_bd22_1 = 0x000a02c4;
// DMA BD22 2
const unsigned int mem_dma_bd22_2 = 0x000a02c8;
// DMA BD22 3
const unsigned int mem_dma_bd22_3 = 0x000a02cc;
// DMA BD22 4
const unsigned int mem_dma_bd22_4 = 0x000a02d0;
// DMA BD22 5
const unsigned int mem_dma_bd22_5 = 0x000a02d4;
// DMA BD22 6
const unsigned int mem_dma_bd22_6 = 0x000a02d8;
// DMA BD22 7
const unsigned int mem_dma_bd22_7 = 0x000a02dc;
// DMA BD23
const unsigned int mem_dma_bd23_0 = 0x000a02e0;
// DMA BD23 1
const unsigned int mem_dma_bd23_1 = 0x000a02e4;
// DMA BD23 2
const unsigned int mem_dma_bd23_2 = 0x000a02e8;
// DMA BD23 3
const unsigned int mem_dma_bd23_3 = 0x000a02ec;
// DMA BD23 4
const unsigned int mem_dma_bd23_4 = 0x000a02f0;
// DMA BD23 5
const unsigned int mem_dma_bd23_5 = 0x000a02f4;
// DMA BD23 6
const unsigned int mem_dma_bd23_6 = 0x000a02f8;
// DMA BD23 7
const unsigned int mem_dma_bd23_7 = 0x000a02fc;
// DMA BD24
const unsigned int mem_dma_bd24_0 = 0x000a0300;
// DMA BD24 1
const unsigned int mem_dma_bd24_1 = 0x000a0304;
// DMA BD24 2
const unsigned int mem_dma_bd24_2 = 0x000a0308;
// DMA BD24 3
const unsigned int mem_dma_bd24_3 = 0x000a030c;
// DMA BD24 4
const unsigned int mem_dma_bd24_4 = 0x000a0310;
// DMA BD24 5
const unsigned int mem_dma_bd24_5 = 0x000a0314;
// DMA BD24 6
const unsigned int mem_dma_bd24_6 = 0x000a0318;
// DMA BD24 7
const unsigned int mem_dma_bd24_7 = 0x000a031c;
// DMA BD25
const unsigned int mem_dma_bd25_0 = 0x000a0320;
// DMA BD25 1
const unsigned int mem_dma_bd25_1 = 0x000a0324;
// DMA BD25 2
const unsigned int mem_dma_bd25_2 = 0x000a0328;
// DMA BD25 3
const unsigned int mem_dma_bd25_3 = 0x000a032c;
// DMA BD25 4
const unsigned int mem_dma_bd25_4 = 0x000a0330;
// DMA BD25 5
const unsigned int mem_dma_bd25_5 = 0x000a0334;
// DMA BD25 6
const unsigned int mem_dma_bd25_6 = 0x000a0338;
// DMA BD25 7
const unsigned int mem_dma_bd25_7 = 0x000a033c;
// DMA BD26
const unsigned int mem_dma_bd26_0 = 0x000a0340;
// DMA BD26 1
const unsigned int mem_dma_bd26_1 = 0x000a0344;
// DMA BD26 2
const unsigned int mem_dma_bd26_2 = 0x000a0348;
// DMA BD26 3
const unsigned int mem_dma_bd26_3 = 0x000a034c;
// DMA BD26 4
const unsigned int mem_dma_bd26_4 = 0x000a0350;
// DMA BD26 5
const unsigned int mem_dma_bd26_5 = 0x000a0354;
// DMA BD26 6
const unsigned int mem_dma_bd26_6 = 0x000a0358;
// DMA BD26 7
const unsigned int mem_dma_bd26_7 = 0x000a035c;
// DMA BD27
const unsigned int mem_dma_bd27_0 = 0x000a0360;
// DMA BD27 1
const unsigned int mem_dma_bd27_1 = 0x000a0364;
// DMA BD27 2
const unsigned int mem_dma_bd27_2 = 0x000a0368;
// DMA BD27 3
const unsigned int mem_dma_bd27_3 = 0x000a036c;
// DMA BD27 4
const unsigned int mem_dma_bd27_4 = 0x000a0370;
// DMA BD27 5
const unsigned int mem_dma_bd27_5 = 0x000a0374;
// DMA BD27 6
const unsigned int mem_dma_bd27_6 = 0x000a0378;
// DMA BD27 7
const unsigned int mem_dma_bd27_7 = 0x000a037c;
// DMA BD25
const unsigned int mem_dma_bd28_0 = 0x000a0380;
// DMA BD28 1
const unsigned int mem_dma_bd28_1 = 0x000a0384;
// DMA BD28 2
const unsigned int mem_dma_bd28_2 = 0x000a0388;
// DMA BD28 3
const unsigned int mem_dma_bd28_3 = 0x000a038c;
// DMA BD28 4
const unsigned int mem_dma_bd28_4 = 0x000a0390;
// DMA BD28 5
const unsigned int mem_dma_bd28_5 = 0x000a0394;
// DMA BD28 6
const unsigned int mem_dma_bd28_6 = 0x000a0398;
// DMA BD28 7
const unsigned int mem_dma_bd28_7 = 0x000a039c;
// DMA BD29
const unsigned int mem_dma_bd29_0 = 0x000a03a0;
// DMA BD29 1
const unsigned int mem_dma_bd29_1 = 0x000a03a4;
// DMA BD29 2
const unsigned int mem_dma_bd29_2 = 0x000a03a8;
// DMA BD29 3
const unsigned int mem_dma_bd29_3 = 0x000a03ac;
// DMA BD29 4
const unsigned int mem_dma_bd29_4 = 0x000a03b0;
// DMA BD29 5
const unsigned int mem_dma_bd29_5 = 0x000a03b4;
// DMA BD29 6
const unsigned int mem_dma_bd29_6 = 0x000a03b8;
// DMA BD29 7
const unsigned int mem_dma_bd29_7 = 0x000a03bc;
// DMA BD30
const unsigned int mem_dma_bd30_0 = 0x000a03c0;
// DMA BD30 1
const unsigned int mem_dma_bd30_1 = 0x000a03c4;
// DMA BD30 2
const unsigned int mem_dma_bd30_2 = 0x000a03c8;
// DMA BD30 3
const unsigned int mem_dma_bd30_3 = 0x000a03cc;
// DMA BD30 4
const unsigned int mem_dma_bd30_4 = 0x000a03d0;
// DMA BD30 5
const unsigned int mem_dma_bd30_5 = 0x000a03d4;
// DMA BD30 6
const unsigned int mem_dma_bd30_6 = 0x000a03d8;
// DMA BD30 7
const unsigned int mem_dma_bd30_7 = 0x000a03dc;
// DMA BD31
const unsigned int mem_dma_bd31_0 = 0x000a03e0;
// DMA BD31 1
const unsigned int mem_dma_bd31_1 = 0x000a03e4;
// DMA BD31 2
const unsigned int mem_dma_bd31_2 = 0x000a03e8;
// DMA BD31 3
const unsigned int mem_dma_bd31_3 = 0x000a03ec;
// DMA BD31 4
const unsigned int mem_dma_bd31_4 = 0x000a03f0;
// DMA BD31 5
const unsigned int mem_dma_bd31_5 = 0x000a03f4;
// DMA BD31 6
const unsigned int mem_dma_bd31_6 = 0x000a03f8;
// DMA BD31 7
const unsigned int mem_dma_bd31_7 = 0x000a03fc;
// DMA BD32
const unsigned int mem_dma_bd32_0 = 0x000a0400;
// DMA BD32 1
const unsigned int mem_dma_bd32_1 = 0x000a0404;
// DMA BD32 2
const unsigned int mem_dma_bd32_2 = 0x000a0408;
// DMA BD32 3
const unsigned int mem_dma_bd32_3 = 0x000a040c;
// DMA BD32 4
const unsigned int mem_dma_bd32_4 = 0x000a0410;
// DMA BD32 5
const unsigned int mem_dma_bd32_5 = 0x000a0414;
// DMA BD32 6
const unsigned int mem_dma_bd32_6 = 0x000a0418;
// DMA BD32 7
const unsigned int mem_dma_bd32_7 = 0x000a041c;
// DMA BD33
const unsigned int mem_dma_bd33_0 = 0x000a0420;
// DMA BD33 1
const unsigned int mem_dma_bd33_1 = 0x000a0424;
// DMA BD33 2
const unsigned int mem_dma_bd33_2 = 0x000a0428;
// DMA BD33 3
const unsigned int mem_dma_bd33_3 = 0x000a042c;
// DMA BD33 4
const unsigned int mem_dma_bd33_4 = 0x000a0430;
// DMA BD33 5
const unsigned int mem_dma_bd33_5 = 0x000a0434;
// DMA BD33 6
const unsigned int mem_dma_bd33_6 = 0x000a0438;
// DMA BD33 7
const unsigned int mem_dma_bd33_7 = 0x000a043c;
// DMA BD34
const unsigned int mem_dma_bd34_0 = 0x000a0440;
// DMA BD34 1
const unsigned int mem_dma_bd34_1 = 0x000a0444;
// DMA BD34 2
const unsigned int mem_dma_bd34_2 = 0x000a0448;
// DMA BD34 3
const unsigned int mem_dma_bd34_3 = 0x000a044c;
// DMA BD34 4
const unsigned int mem_dma_bd34_4 = 0x000a0450;
// DMA BD34 5
const unsigned int mem_dma_bd34_5 = 0x000a0454;
// DMA BD34 6
const unsigned int mem_dma_bd34_6 = 0x000a0458;
// DMA BD34 7
const unsigned int mem_dma_bd34_7 = 0x000a045c;
// DMA BD35
const unsigned int mem_dma_bd35_0 = 0x000a0460;
// DMA BD35 1
const unsigned int mem_dma_bd35_1 = 0x000a0464;
// DMA BD35 2
const unsigned int mem_dma_bd35_2 = 0x000a0468;
// DMA BD35 3
const unsigned int mem_dma_bd35_3 = 0x000a046c;
// DMA BD35 4
const unsigned int mem_dma_bd35_4 = 0x000a0470;
// DMA BD35 5
const unsigned int mem_dma_bd35_5 = 0x000a0474;
// DMA BD35 6
const unsigned int mem_dma_bd35_6 = 0x000a0478;
// DMA BD35 7
const unsigned int mem_dma_bd35_7 = 0x000a047c;
// DMA BD36
const unsigned int mem_dma_bd36_0 = 0x000a0480;
// DMA BD36 1
const unsigned int mem_dma_bd36_1 = 0x000a0484;
// DMA BD36 2
const unsigned int mem_dma_bd36_2 = 0x000a0488;
// DMA BD36 3
const unsigned int mem_dma_bd36_3 = 0x000a048c;
// DMA BD36 4
const unsigned int mem_dma_bd36_4 = 0x000a0490;
// DMA BD36 5
const unsigned int mem_dma_bd36_5 = 0x000a0494;
// DMA BD36 6
const unsigned int mem_dma_bd36_6 = 0x000a0498;
// DMA BD36 7
const unsigned int mem_dma_bd36_7 = 0x000a049c;
// DMA BD37
const unsigned int mem_dma_bd37_0 = 0x000a04a0;
// DMA BD37 1
const unsigned int mem_dma_bd37_1 = 0x000a04a4;
// DMA BD37 2
const unsigned int mem_dma_bd37_2 = 0x000a04a8;
// DMA BD37 3
const unsigned int mem_dma_bd37_3 = 0x000a04ac;
// DMA BD37 4
const unsigned int mem_dma_bd37_4 = 0x000a04b0;
// DMA BD37 5
const unsigned int mem_dma_bd37_5 = 0x000a04b4;
// DMA BD37 6
const unsigned int mem_dma_bd37_6 = 0x000a04b8;
// DMA BD37 7
const unsigned int mem_dma_bd37_7 = 0x000a04bc;
// DMA BD35
const unsigned int mem_dma_bd38_0 = 0x000a04c0;
// DMA BD38 1
const unsigned int mem_dma_bd38_1 = 0x000a04c4;
// DMA BD38 2
const unsigned int mem_dma_bd38_2 = 0x000a04c8;
// DMA BD38 3
const unsigned int mem_dma_bd38_3 = 0x000a04cc;
// DMA BD38 4
const unsigned int mem_dma_bd38_4 = 0x000a04d0;
// DMA BD38 5
const unsigned int mem_dma_bd38_5 = 0x000a04d4;
// DMA BD38 6
const unsigned int mem_dma_bd38_6 = 0x000a04d8;
// DMA BD38 7
const unsigned int mem_dma_bd38_7 = 0x000a04dc;
// DMA BD39
const unsigned int mem_dma_bd39_0 = 0x000a04e0;
// DMA BD39 1
const unsigned int mem_dma_bd39_1 = 0x000a04e4;
// DMA BD39 2
const unsigned int mem_dma_bd39_2 = 0x000a04e8;
// DMA BD39 3
const unsigned int mem_dma_bd39_3 = 0x000a04ec;
// DMA BD39 4
const unsigned int mem_dma_bd39_4 = 0x000a04f0;
// DMA BD39 5
const unsigned int mem_dma_bd39_5 = 0x000a04f4;
// DMA BD39 6
const unsigned int mem_dma_bd39_6 = 0x000a04f8;
// DMA BD39 7
const unsigned int mem_dma_bd39_7 = 0x000a04fc;
// DMA BD40
const unsigned int mem_dma_bd40_0 = 0x000a0500;
// DMA BD40 1
const unsigned int mem_dma_bd40_1 = 0x000a0504;
// DMA BD40 2
const unsigned int mem_dma_bd40_2 = 0x000a0508;
// DMA BD40 3
const unsigned int mem_dma_bd40_3 = 0x000a050c;
// DMA BD40 4
const unsigned int mem_dma_bd40_4 = 0x000a0510;
// DMA BD40 5
const unsigned int mem_dma_bd40_5 = 0x000a0514;
// DMA BD40 6
const unsigned int mem_dma_bd40_6 = 0x000a0518;
// DMA BD40 7
const unsigned int mem_dma_bd40_7 = 0x000a051c;
// DMA BD41
const unsigned int mem_dma_bd41_0 = 0x000a0520;
// DMA BD41 1
const unsigned int mem_dma_bd41_1 = 0x000a0524;
// DMA BD41 2
const unsigned int mem_dma_bd41_2 = 0x000a0528;
// DMA BD41 3
const unsigned int mem_dma_bd41_3 = 0x000a052c;
// DMA BD41 4
const unsigned int mem_dma_bd41_4 = 0x000a0530;
// DMA BD41 5
const unsigned int mem_dma_bd41_5 = 0x000a0534;
// DMA BD41 6
const unsigned int mem_dma_bd41_6 = 0x000a0538;
// DMA BD41 7
const unsigned int mem_dma_bd41_7 = 0x000a053c;
// DMA BD42
const unsigned int mem_dma_bd42_0 = 0x000a0540;
// DMA BD42 1
const unsigned int mem_dma_bd42_1 = 0x000a0544;
// DMA BD42 2
const unsigned int mem_dma_bd42_2 = 0x000a0548;
// DMA BD42 3
const unsigned int mem_dma_bd42_3 = 0x000a054c;
// DMA BD42 4
const unsigned int mem_dma_bd42_4 = 0x000a0550;
// DMA BD42 5
const unsigned int mem_dma_bd42_5 = 0x000a0554;
// DMA BD42 6
const unsigned int mem_dma_bd42_6 = 0x000a0558;
// DMA BD42 7
const unsigned int mem_dma_bd42_7 = 0x000a055c;
// DMA BD43
const unsigned int mem_dma_bd43_0 = 0x000a0560;
// DMA BD43 1
const unsigned int mem_dma_bd43_1 = 0x000a0564;
// DMA BD43 2
const unsigned int mem_dma_bd43_2 = 0x000a0568;
// DMA BD43 3
const unsigned int mem_dma_bd43_3 = 0x000a056c;
// DMA BD43 4
const unsigned int mem_dma_bd43_4 = 0x000a0570;
// DMA BD43 5
const unsigned int mem_dma_bd43_5 = 0x000a0574;
// DMA BD43 6
const unsigned int mem_dma_bd43_6 = 0x000a0578;
// DMA BD43 7
const unsigned int mem_dma_bd43_7 = 0x000a057c;
// DMA BD44
const unsigned int mem_dma_bd44_0 = 0x000a0580;
// DMA BD44 1
const unsigned int mem_dma_bd44_1 = 0x000a0584;
// DMA BD44 2
const unsigned int mem_dma_bd44_2 = 0x000a0588;
// DMA BD44 3
const unsigned int mem_dma_bd44_3 = 0x000a058c;
// DMA BD44 4
const unsigned int mem_dma_bd44_4 = 0x000a0590;
// DMA BD44 5
const unsigned int mem_dma_bd44_5 = 0x000a0594;
// DMA BD44 6
const unsigned int mem_dma_bd44_6 = 0x000a0598;
// DMA BD44 7
const unsigned int mem_dma_bd44_7 = 0x000a059c;
// DMA BD45
const unsigned int mem_dma_bd45_0 = 0x000a05a0;
// DMA BD45 1
const unsigned int mem_dma_bd45_1 = 0x000a05a4;
// DMA BD45 2
const unsigned int mem_dma_bd45_2 = 0x000a05a8;
// DMA BD45 3
const unsigned int mem_dma_bd45_3 = 0x000a05ac;
// DMA BD45 4
const unsigned int mem_dma_bd45_4 = 0x000a05b0;
// DMA BD45 5
const unsigned int mem_dma_bd45_5 = 0x000a05b4;
// DMA BD45 6
const unsigned int mem_dma_bd45_6 = 0x000a05b8;
// DMA BD45 7
const unsigned int mem_dma_bd45_7 = 0x000a05bc;
// DMA BD46
const unsigned int mem_dma_bd46_0 = 0x000a05c0;
// DMA BD46 1
const unsigned int mem_dma_bd46_1 = 0x000a05c4;
// DMA BD46 2
const unsigned int mem_dma_bd46_2 = 0x000a05c8;
// DMA BD46 3
const unsigned int mem_dma_bd46_3 = 0x000a05cc;
// DMA BD46 4
const unsigned int mem_dma_bd46_4 = 0x000a05d0;
// DMA BD46 5
const unsigned int mem_dma_bd46_5 = 0x000a05d4;
// DMA BD46 6
const unsigned int mem_dma_bd46_6 = 0x000a05d8;
// DMA BD46 7
const unsigned int mem_dma_bd46_7 = 0x000a05dc;
// DMA BD47
const unsigned int mem_dma_bd47_0 = 0x000a05e0;
// DMA BD47 1
const unsigned int mem_dma_bd47_1 = 0x000a05e4;
// DMA BD47 2
const unsigned int mem_dma_bd47_2 = 0x000a05e8;
// DMA BD47 3
const unsigned int mem_dma_bd47_3 = 0x000a05ec;
// DMA BD47 4
const unsigned int mem_dma_bd47_4 = 0x000a05f0;
// DMA BD47 5
const unsigned int mem_dma_bd47_5 = 0x000a05f4;
// DMA BD47 6
const unsigned int mem_dma_bd47_6 = 0x000a05f8;
// DMA BD47 7
const unsigned int mem_dma_bd47_7 = 0x000a05fc;
// DMA Control Register S2MM Ch0
const unsigned int mem_dma_s2mm_0_ctrl = 0x000a0600;
// DMA Control Register S2MM Ch0 start BD
const unsigned int mem_dma_s2mm_0_start_queue = 0x000a0604;
// DMA Control Register S2MM Ch1
const unsigned int mem_dma_s2mm_1_ctrl = 0x000a0608;
// DMA Control Register S2MM Ch1 start BD
const unsigned int mem_dma_s2mm_1_start_queue = 0x000a060c;
// DMA Control Register S2MM Ch2
const unsigned int mem_dma_s2mm_2_ctrl = 0x000a0610;
// DMA Control Register S2MM Ch2 start BD
const unsigned int mem_dma_s2mm_2_start_queue = 0x000a0614;
// DMA Control Register S2MM Ch3
const unsigned int mem_dma_s2mm_3_ctrl = 0x000a0618;
// DMA Control Register S2MM Ch3 start BD
const unsigned int mem_dma_s2mm_3_start_queue = 0x000a061c;
// DMA Control Register S2MM Ch4
const unsigned int mem_dma_s2mm_4_ctrl = 0x000a0620;
// DMA Control Register S2MM Ch4 start BD
const unsigned int mem_dma_s2mm_4_start_queue = 0x000a0624;
// DMA Control Register S2MM Ch5
const unsigned int mem_dma_s2mm_5_ctrl = 0x000a0628;
// DMA Control Register S2MM Ch5 start BD
const unsigned int mem_dma_s2mm_5_start_queue = 0x000a062c;
// DMA Control Register MM2S Ch0
const unsigned int mem_dma_mm2s_0_ctrl = 0x000a0630;
// DMA Control Register MM2S Ch0 start BD
const unsigned int mem_dma_mm2s_0_start_queue = 0x000a0634;
// DMA Control Register MM2S Ch1
const unsigned int mem_dma_mm2s_1_ctrl = 0x000a0638;
// DMA Control Register MM2S Ch1 start BD
const unsigned int mem_dma_mm2s_1_start_queue = 0x000a063c;
// DMA Control Register MM2S Ch2
const unsigned int mem_dma_mm2s_2_ctrl = 0x000a0640;
// DMA Control Register MM2S Ch2 start BD
const unsigned int mem_dma_mm2s_2_start_queue = 0x000a0644;
// DMA Control Register MM2S Ch3
const unsigned int mem_dma_mm2s_3_ctrl = 0x000a0648;
// DMA Control Register MM2S Ch3 start BD
const unsigned int mem_dma_mm2s_3_start_queue = 0x000a064c;
// DMA Control Register MM2S Ch4
const unsigned int mem_dma_mm2s_4_ctrl = 0x000a0650;
// DMA Control Register MM2S Ch4 start BD
const unsigned int mem_dma_mm2s_4_start_queue = 0x000a0654;
// DMA Control Register MM2S Ch5
const unsigned int mem_dma_mm2s_5_ctrl = 0x000a0658;
// DMA Control Register MM2S Ch5 start BD
const unsigned int mem_dma_mm2s_5_start_queue = 0x000a065c;
// DMA S2MM Status Register Ch0
const unsigned int mem_dma_s2mm_status_0 = 0x000a0660;
// DMA S2MM Status Register Ch1
const unsigned int mem_dma_s2mm_status_1 = 0x000a0664;
// DMA S2MM Status Register Ch2
const unsigned int mem_dma_s2mm_status_2 = 0x000a0668;
// DMA S2MM Status Register Ch3
const unsigned int mem_dma_s2mm_status_3 = 0x000a066c;
// DMA S2MM Status Register Ch4
const unsigned int mem_dma_s2mm_status_4 = 0x000a0670;
// DMA S2MM Status Register Ch5
const unsigned int mem_dma_s2mm_status_5 = 0x000a0674;
// DMA S2MM Status Register Ch0
const unsigned int mem_dma_mm2s_status_0 = 0x000a0680;
// DMA S2MM Status Register Ch1
const unsigned int mem_dma_mm2s_status_1 = 0x000a0684;
// DMA S2MM Status Register Ch2
const unsigned int mem_dma_mm2s_status_2 = 0x000a0688;
// DMA S2MM Status Register Ch3
const unsigned int mem_dma_mm2s_status_3 = 0x000a068c;
// DMA S2MM Status Register Ch4
const unsigned int mem_dma_mm2s_status_4 = 0x000a0690;
// DMA S2MM Status Register Ch5
const unsigned int mem_dma_mm2s_status_5 = 0x000a0694;
// Selection of which DMA channels will generate events
const unsigned int mem_dma_event_channel_selection = 0x000a06a0;
// DMA S2MM Current Write Count Ch0
const unsigned int mem_dma_s2mm_current_write_count_0 = 0x000a06b0;
// DMA S2MM Current Write Count Ch1
const unsigned int mem_dma_s2mm_current_write_count_1 = 0x000a06b4;
// DMA S2MM Current Write Count Ch2
const unsigned int mem_dma_s2mm_current_write_count_2 = 0x000a06b8;
// DMA S2MM Current Write Count Ch3
const unsigned int mem_dma_s2mm_current_write_count_3 = 0x000a06bc;
// DMA S2MM Current Write Count Ch4
const unsigned int mem_dma_s2mm_current_write_count_4 = 0x000a06c0;
// DMA S2MM Current Write Count Ch5
const unsigned int mem_dma_s2mm_current_write_count_5 = 0x000a06c4;
// DMA S2MM FoT Count FIFO Pop Ch0
const unsigned int mem_dma_s2mm_fot_count_fifo_pop_0 = 0x000a06c8;
// DMA S2MM FoT Count FIFO Pop Ch1
const unsigned int mem_dma_s2mm_fot_count_fifo_pop_1 = 0x000a06cc;
// DMA S2MM FoT Count FIFO Pop Ch2
const unsigned int mem_dma_s2mm_fot_count_fifo_pop_2 = 0x000a06d0;
// DMA S2MM FoT Count FIFO Pop Ch3
const unsigned int mem_dma_s2mm_fot_count_fifo_pop_3 = 0x000a06d4;
// DMA S2MM FoT Count FIFO Pop Ch4
const unsigned int mem_dma_s2mm_fot_count_fifo_pop_4 = 0x000a06d8;
// DMA S2MM FoT Count FIFO Pop Ch5
const unsigned int mem_dma_s2mm_fot_count_fifo_pop_5 = 0x000a06dc;
// Stream Switch Master Configuration DMA 0
const unsigned int mem_stream_switch_master_config_dma0 = 0x000b0000;
// Stream Switch Master Configuration DMA 1
const unsigned int mem_stream_switch_master_config_dma1 = 0x000b0004;
// Stream Switch Master Configuration DMA 2
const unsigned int mem_stream_switch_master_config_dma2 = 0x000b0008;
// Stream Switch Master Configuration DMA 3
const unsigned int mem_stream_switch_master_config_dma3 = 0x000b000c;
// Stream Switch Master Configuration DMA 4
const unsigned int mem_stream_switch_master_config_dma4 = 0x000b0010;
// Stream Switch Master Configuration DMA 5
const unsigned int mem_stream_switch_master_config_dma5 = 0x000b0014;
// Stream Switch Master Configuration AI Engine Tile Ctrl
const unsigned int mem_stream_switch_master_config_tile_ctrl = 0x000b0018;
// Stream Switch Master Configuration South 0
const unsigned int mem_stream_switch_master_config_south0 = 0x000b001c;
// Stream Switch Master Configuration South 1
const unsigned int mem_stream_switch_master_config_south1 = 0x000b0020;
// Stream Switch Master Configuration South 2
const unsigned int mem_stream_switch_master_config_south2 = 0x000b0024;
// Stream Switch Master Configuration South 3
const unsigned int mem_stream_switch_master_config_south3 = 0x000b0028;
// Stream Switch Master Configuration North 0
const unsigned int mem_stream_switch_master_config_north0 = 0x000b002c;
// Stream Switch Master Configuration North 1
const unsigned int mem_stream_switch_master_config_north1 = 0x000b0030;
// Stream Switch Master Configuration North 2
const unsigned int mem_stream_switch_master_config_north2 = 0x000b0034;
// Stream Switch Master Configuration North 3
const unsigned int mem_stream_switch_master_config_north3 = 0x000b0038;
// Stream Switch Master Configuration North 4
const unsigned int mem_stream_switch_master_config_north4 = 0x000b003c;
// Stream Switch Master Configuration North 5
const unsigned int mem_stream_switch_master_config_north5 = 0x000b0040;
// Stream Switch Slave Configuration DMA 0
const unsigned int mem_stream_switch_slave_config_dma_0 = 0x000b0100;
// Stream Switch Slave Configuration DMA 1
const unsigned int mem_stream_switch_slave_config_dma_1 = 0x000b0104;
// Stream Switch Slave Configuration DMA 2
const unsigned int mem_stream_switch_slave_config_dma_2 = 0x000b0108;
// Stream Switch Slave Configuration DMA 3
const unsigned int mem_stream_switch_slave_config_dma_3 = 0x000b010c;
// Stream Switch Slave Configuration DMA 4
const unsigned int mem_stream_switch_slave_config_dma_4 = 0x000b0110;
// Stream Switch Slave Configuration DMA 5
const unsigned int mem_stream_switch_slave_config_dma_5 = 0x000b0114;
// Stream Switch Slave Configuration Tile Ctrl
const unsigned int mem_stream_switch_slave_config_tile_ctrl = 0x000b0118;
// Stream Switch Slave Configuration South 0
const unsigned int mem_stream_switch_slave_config_south_0 = 0x000b011c;
// Stream Switch Slave Configuration South 1
const unsigned int mem_stream_switch_slave_config_south_1 = 0x000b0120;
// Stream Switch Slave Configuration South 2
const unsigned int mem_stream_switch_slave_config_south_2 = 0x000b0124;
// Stream Switch Slave Configuration South 3
const unsigned int mem_stream_switch_slave_config_south_3 = 0x000b0128;
// Stream Switch Slave Configuration South 4
const unsigned int mem_stream_switch_slave_config_south_4 = 0x000b012c;
// Stream Switch Slave Configuration South 5
const unsigned int mem_stream_switch_slave_config_south_5 = 0x000b0130;
// Stream Switch Slave Configuration North 0
const unsigned int mem_stream_switch_slave_config_north_0 = 0x000b0134;
// Stream Switch Slave Configuration North 1
const unsigned int mem_stream_switch_slave_config_north_1 = 0x000b0138;
// Stream Switch Slave Configuration North 2
const unsigned int mem_stream_switch_slave_config_north_2 = 0x000b013c;
// Stream Switch Slave Configuration North 3
const unsigned int mem_stream_switch_slave_config_north_3 = 0x000b0140;
// Stream Switch Slave Configuration Trace
const unsigned int mem_stream_switch_slave_config_trace = 0x000b0144;
// Stream Switch Slave Configuration DMA 0
const unsigned int mem_stream_switch_slave_dma_0_slot0 = 0x000b0200;
// Stream Switch Slave Configuration DMA 0
const unsigned int mem_stream_switch_slave_dma_0_slot1 = 0x000b0204;
// Stream Switch Slave Configuration DMA 0
const unsigned int mem_stream_switch_slave_dma_0_slot2 = 0x000b0208;
// Stream Switch Slave Configuration DMA 0
const unsigned int mem_stream_switch_slave_dma_0_slot3 = 0x000b020c;
// Stream Switch Slave Configuration DMA 1
const unsigned int mem_stream_switch_slave_dma_1_slot0 = 0x000b0210;
// Stream Switch Slave Configuration DMA 1
const unsigned int mem_stream_switch_slave_dma_1_slot1 = 0x000b0214;
// Stream Switch Slave Configuration DMA 1
const unsigned int mem_stream_switch_slave_dma_1_slot2 = 0x000b0218;
// Stream Switch Slave Configuration DMA 1
const unsigned int mem_stream_switch_slave_dma_1_slot3 = 0x000b021c;
// Stream Switch Slave Configuration DMA 2
const unsigned int mem_stream_switch_slave_dma_2_slot0 = 0x000b0220;
// Stream Switch Slave Configuration DMA 2
const unsigned int mem_stream_switch_slave_dma_2_slot1 = 0x000b0224;
// Stream Switch Slave Configuration DMA 2
const unsigned int mem_stream_switch_slave_dma_2_slot2 = 0x000b0228;
// Stream Switch Slave Configuration DMA 2
const unsigned int mem_stream_switch_slave_dma_2_slot3 = 0x000b022c;
// Stream Switch Slave Configuration DMA 3
const unsigned int mem_stream_switch_slave_dma_3_slot0 = 0x000b0230;
// Stream Switch Slave Configuration DMA 3
const unsigned int mem_stream_switch_slave_dma_3_slot1 = 0x000b0234;
// Stream Switch Slave Configuration DMA 3
const unsigned int mem_stream_switch_slave_dma_3_slot2 = 0x000b0238;
// Stream Switch Slave Configuration DMA 3
const unsigned int mem_stream_switch_slave_dma_3_slot3 = 0x000b023c;
// Stream Switch Slave Configuration DMA 4
const unsigned int mem_stream_switch_slave_dma_4_slot0 = 0x000b0240;
// Stream Switch Slave Configuration DMA 4
const unsigned int mem_stream_switch_slave_dma_4_slot1 = 0x000b0244;
// Stream Switch Slave Configuration DMA 4
const unsigned int mem_stream_switch_slave_dma_4_slot2 = 0x000b0248;
// Stream Switch Slave Configuration DMA 4
const unsigned int mem_stream_switch_slave_dma_4_slot3 = 0x000b024c;
// Stream Switch Slave Configuration DMA 5
const unsigned int mem_stream_switch_slave_dma_5_slot0 = 0x000b0250;
// Stream Switch Slave Configuration DMA 5
const unsigned int mem_stream_switch_slave_dma_5_slot1 = 0x000b0254;
// Stream Switch Slave Configuration DMA 5
const unsigned int mem_stream_switch_slave_dma_5_slot2 = 0x000b0258;
// Stream Switch Slave Configuration DMA 5
const unsigned int mem_stream_switch_slave_dma_5_slot3 = 0x000b025c;
// Stream Switch Slave Configuration Tile Ctrl
const unsigned int mem_stream_switch_slave_tile_ctrl_slot0 = 0x000b0260;
// Stream Switch Slave Configuration Tile Ctrl
const unsigned int mem_stream_switch_slave_tile_ctrl_slot1 = 0x000b0264;
// Stream Switch Slave Configuration Tile Ctrl
const unsigned int mem_stream_switch_slave_tile_ctrl_slot2 = 0x000b0268;
// Stream Switch Slave Configuration Tile Ctrl
const unsigned int mem_stream_switch_slave_tile_ctrl_slot3 = 0x000b026c;
// Stream Switch Slave Configuration South 0
const unsigned int mem_stream_switch_slave_south_0_slot0 = 0x000b0270;
// Stream Switch Slave Configuration South 0
const unsigned int mem_stream_switch_slave_south_0_slot1 = 0x000b0274;
// Stream Switch Slave Configuration South 0
const unsigned int mem_stream_switch_slave_south_0_slot2 = 0x000b0278;
// Stream Switch Slave Configuration South 0
const unsigned int mem_stream_switch_slave_south_0_slot3 = 0x000b027c;
// Stream Switch Slave Configuration South 1
const unsigned int mem_stream_switch_slave_south_1_slot0 = 0x000b0280;
// Stream Switch Slave Configuration South 1
const unsigned int mem_stream_switch_slave_south_1_slot1 = 0x000b0284;
// Stream Switch Slave Configuration South 1
const unsigned int mem_stream_switch_slave_south_1_slot2 = 0x000b0288;
// Stream Switch Slave Configuration South 1
const unsigned int mem_stream_switch_slave_south_1_slot3 = 0x000b028c;
// Stream Switch Slave Configuration South 2
const unsigned int mem_stream_switch_slave_south_2_slot0 = 0x000b0290;
// Stream Switch Slave Configuration South 2
const unsigned int mem_stream_switch_slave_south_2_slot1 = 0x000b0294;
// Stream Switch Slave Configuration South 2
const unsigned int mem_stream_switch_slave_south_2_slot2 = 0x000b0298;
// Stream Switch Slave Configuration South 2
const unsigned int mem_stream_switch_slave_south_2_slot3 = 0x000b029c;
// Stream Switch Slave Configuration South 3
const unsigned int mem_stream_switch_slave_south_3_slot0 = 0x000b02a0;
// Stream Switch Slave Configuration South 3
const unsigned int mem_stream_switch_slave_south_3_slot1 = 0x000b02a4;
// Stream Switch Slave Configuration South 3
const unsigned int mem_stream_switch_slave_south_3_slot2 = 0x000b02a8;
// Stream Switch Slave Configuration South 3
const unsigned int mem_stream_switch_slave_south_3_slot3 = 0x000b02ac;
// Stream Switch Slave Configuration South 4
const unsigned int mem_stream_switch_slave_south_4_slot0 = 0x000b02b0;
// Stream Switch Slave Configuration South 4
const unsigned int mem_stream_switch_slave_south_4_slot1 = 0x000b02b4;
// Stream Switch Slave Configuration South 4
const unsigned int mem_stream_switch_slave_south_4_slot2 = 0x000b02b8;
// Stream Switch Slave Configuration South 4
const unsigned int mem_stream_switch_slave_south_4_slot3 = 0x000b02bc;
// Stream Switch Slave Configuration South 5
const unsigned int mem_stream_switch_slave_south_5_slot0 = 0x000b02c0;
// Stream Switch Slave Configuration South 5
const unsigned int mem_stream_switch_slave_south_5_slot1 = 0x000b02c4;
// Stream Switch Slave Configuration South 5
const unsigned int mem_stream_switch_slave_south_5_slot2 = 0x000b02c8;
// Stream Switch Slave Configuration South 5
const unsigned int mem_stream_switch_slave_south_5_slot3 = 0x000b02cc;
// Stream Switch Slave Configuration North 0
const unsigned int mem_stream_switch_slave_north_0_slot0 = 0x000b02d0;
// Stream Switch Slave Configuration North 0
const unsigned int mem_stream_switch_slave_north_0_slot1 = 0x000b02d4;
// Stream Switch Slave Configuration North 0
const unsigned int mem_stream_switch_slave_north_0_slot2 = 0x000b02d8;
// Stream Switch Slave Configuration North 0
const unsigned int mem_stream_switch_slave_north_0_slot3 = 0x000b02dc;
// Stream Switch Slave Configuration North 1
const unsigned int mem_stream_switch_slave_north_1_slot0 = 0x000b02e0;
// Stream Switch Slave Configuration North 1
const unsigned int mem_stream_switch_slave_north_1_slot1 = 0x000b02e4;
// Stream Switch Slave Configuration North 1
const unsigned int mem_stream_switch_slave_north_1_slot2 = 0x000b02e8;
// Stream Switch Slave Configuration North 1
const unsigned int mem_stream_switch_slave_north_1_slot3 = 0x000b02ec;
// Stream Switch Slave Configuration North 2
const unsigned int mem_stream_switch_slave_north_2_slot0 = 0x000b02f0;
// Stream Switch Slave Configuration North 2
const unsigned int mem_stream_switch_slave_north_2_slot1 = 0x000b02f4;
// Stream Switch Slave Configuration North 2
const unsigned int mem_stream_switch_slave_north_2_slot2 = 0x000b02f8;
// Stream Switch Slave Configuration North 2
const unsigned int mem_stream_switch_slave_north_2_slot3 = 0x000b02fc;
// Stream Switch Slave Configuration North 3
const unsigned int mem_stream_switch_slave_north_3_slot0 = 0x000b0300;
// Stream Switch Slave Configuration North 3
const unsigned int mem_stream_switch_slave_north_3_slot1 = 0x000b0304;
// Stream Switch Slave Configuration North 3
const unsigned int mem_stream_switch_slave_north_3_slot2 = 0x000b0308;
// Stream Switch Slave Configuration North 3
const unsigned int mem_stream_switch_slave_north_3_slot3 = 0x000b030c;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int mem_stream_switch_slave_trace_slot0 = 0x000b0310;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int mem_stream_switch_slave_trace_slot1 = 0x000b0314;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int mem_stream_switch_slave_trace_slot2 = 0x000b0318;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int mem_stream_switch_slave_trace_slot3 = 0x000b031c;
// Stream Switch Deterministic Merge Arbiter:0 Slave:0,1
const unsigned int mem_stream_switch_deterministic_merge_arb0_slave0_1 = 0x000b0800;
// Stream Switch Deterministic Merge Arbiter:0 Slave:2,3
const unsigned int mem_stream_switch_deterministic_merge_arb0_slave2_3 = 0x000b0804;
// Stream Switch Deterministic Merge Arbiter:0 Control
const unsigned int mem_stream_switch_deterministic_merge_arb0_ctrl = 0x000b0808;
// Stream Switch Deterministic Merge Arbiter:1 Slave:0,1
const unsigned int mem_stream_switch_deterministic_merge_arb1_slave0_1 = 0x000b0810;
// Stream Switch Deterministic Merge Arbiter:1 Slave:2,3
const unsigned int mem_stream_switch_deterministic_merge_arb1_slave2_3 = 0x000b0814;
// Stream Switch Deterministic Merge Arbiter:1 Control
const unsigned int mem_stream_switch_deterministic_merge_arb1_ctrl = 0x000b0818;
// Select Stream Switch Ports for event generation
const unsigned int mem_stream_switch_event_port_selection_0 = 0x000b0f00;
// Select Stream Switch Ports for event generation
const unsigned int mem_stream_switch_event_port_selection_1 = 0x000b0f04;
// Status bits for Parity errors on stream switch ports
const unsigned int mem_stream_switch_parity_status = 0x000b0f10;
// Injection of Parity errors on stream switch ports
const unsigned int mem_stream_switch_parity_injection = 0x000b0f20;
// Status of control packet handling
const unsigned int mem_tile_control_packet_handler_status = 0x000b0f30;
// Status of Stream Switch Adaptive Clock Gate
const unsigned int mem_stream_switch_adaptive_clock_gate_status = 0x000b0f34;
// Status of Stream Switch Adaptive Clock Gate Abort Period
const unsigned int mem_stream_switch_adaptive_clock_gate_abort_period = 0x000b0f38;
// Value of lock 0
const unsigned int mem_lock0_value = 0x000c0000;
// Value of lock 1
const unsigned int mem_lock1_value = 0x000c0010;
// Value of lock 2
const unsigned int mem_lock2_value = 0x000c0020;
// Value of lock 3
const unsigned int mem_lock3_value = 0x000c0030;
// Value of lock 4
const unsigned int mem_lock4_value = 0x000c0040;
// Value of lock 5
const unsigned int mem_lock5_value = 0x000c0050;
// Value of lock 6
const unsigned int mem_lock6_value = 0x000c0060;
// Value of lock 7
const unsigned int mem_lock7_value = 0x000c0070;
// Value of lock 8
const unsigned int mem_lock8_value = 0x000c0080;
// Value of lock 9
const unsigned int mem_lock9_value = 0x000c0090;
// Value of lock 10
const unsigned int mem_lock10_value = 0x000c00a0;
// Value of lock 11
const unsigned int mem_lock11_value = 0x000c00b0;
// Value of lock 12
const unsigned int mem_lock12_value = 0x000c00c0;
// Value of lock 13
const unsigned int mem_lock13_value = 0x000c00d0;
// Value of lock 14
const unsigned int mem_lock14_value = 0x000c00e0;
// Value of lock 15
const unsigned int mem_lock15_value = 0x000c00f0;
// Value of lock 16
const unsigned int mem_lock16_value = 0x000c0100;
// Value of lock 17
const unsigned int mem_lock17_value = 0x000c0110;
// Value of lock 18
const unsigned int mem_lock18_value = 0x000c0120;
// Value of lock 19
const unsigned int mem_lock19_value = 0x000c0130;
// Value of lock 20
const unsigned int mem_lock20_value = 0x000c0140;
// Value of lock 21
const unsigned int mem_lock21_value = 0x000c0150;
// Value of lock 22
const unsigned int mem_lock22_value = 0x000c0160;
// Value of lock 23
const unsigned int mem_lock23_value = 0x000c0170;
// Value of lock 24
const unsigned int mem_lock24_value = 0x000c0180;
// Value of lock 25
const unsigned int mem_lock25_value = 0x000c0190;
// Value of lock 26
const unsigned int mem_lock26_value = 0x000c01a0;
// Value of lock 27
const unsigned int mem_lock27_value = 0x000c01b0;
// Value of lock 28
const unsigned int mem_lock28_value = 0x000c01c0;
// Value of lock 29
const unsigned int mem_lock29_value = 0x000c01d0;
// Value of lock 30
const unsigned int mem_lock30_value = 0x000c01e0;
// Value of lock 31
const unsigned int mem_lock31_value = 0x000c01f0;
// Value of lock 32
const unsigned int mem_lock32_value = 0x000c0200;
// Value of lock 33
const unsigned int mem_lock33_value = 0x000c0210;
// Value of lock 34
const unsigned int mem_lock34_value = 0x000c0220;
// Value of lock 35
const unsigned int mem_lock35_value = 0x000c0230;
// Value of lock 36
const unsigned int mem_lock36_value = 0x000c0240;
// Value of lock 37
const unsigned int mem_lock37_value = 0x000c0250;
// Value of lock 38
const unsigned int mem_lock38_value = 0x000c0260;
// Value of lock 39
const unsigned int mem_lock39_value = 0x000c0270;
// Value of lock 40
const unsigned int mem_lock40_value = 0x000c0280;
// Value of lock 41
const unsigned int mem_lock41_value = 0x000c0290;
// Value of lock 42
const unsigned int mem_lock42_value = 0x000c02a0;
// Value of lock 43
const unsigned int mem_lock43_value = 0x000c02b0;
// Value of lock 44
const unsigned int mem_lock44_value = 0x000c02c0;
// Value of lock 45
const unsigned int mem_lock45_value = 0x000c02d0;
// Value of lock 46
const unsigned int mem_lock46_value = 0x000c02e0;
// Value of lock 47
const unsigned int mem_lock47_value = 0x000c02f0;
// Value of lock 48
const unsigned int mem_lock48_value = 0x000c0300;
// Value of lock 49
const unsigned int mem_lock49_value = 0x000c0310;
// Value of lock 50
const unsigned int mem_lock50_value = 0x000c0320;
// Value of lock 51
const unsigned int mem_lock51_value = 0x000c0330;
// Value of lock 52
const unsigned int mem_lock52_value = 0x000c0340;
// Value of lock 53
const unsigned int mem_lock53_value = 0x000c0350;
// Value of lock 54
const unsigned int mem_lock54_value = 0x000c0360;
// Value of lock 55
const unsigned int mem_lock55_value = 0x000c0370;
// Value of lock 56
const unsigned int mem_lock56_value = 0x000c0380;
// Value of lock 57
const unsigned int mem_lock57_value = 0x000c0390;
// Value of lock 58
const unsigned int mem_lock58_value = 0x000c03a0;
// Value of lock 59
const unsigned int mem_lock59_value = 0x000c03b0;
// Value of lock 60
const unsigned int mem_lock60_value = 0x000c03c0;
// Value of lock 61
const unsigned int mem_lock61_value = 0x000c03d0;
// Value of lock 62
const unsigned int mem_lock62_value = 0x000c03e0;
// Value of lock 63
const unsigned int mem_lock63_value = 0x000c03f0;
// Select lock for lock event generation 0
const unsigned int mem_locks_event_selection_0 = 0x000c0400;
// Select lock for lock event generation 1
const unsigned int mem_locks_event_selection_1 = 0x000c0404;
// Select lock for lock event generation 2
const unsigned int mem_locks_event_selection_2 = 0x000c0408;
// Select lock for lock event generation 3
const unsigned int mem_locks_event_selection_3 = 0x000c040c;
// Select lock for lock event generation 4
const unsigned int mem_locks_event_selection_4 = 0x000c0410;
// Select lock for lock event generation 5
const unsigned int mem_locks_event_selection_5 = 0x000c0414;
// Select lock for lock event generation 6
const unsigned int mem_locks_event_selection_6 = 0x000c0418;
// Select lock for lock event generation 7
const unsigned int mem_locks_event_selection_7 = 0x000c041c;
// Status bits for lock overflow, write to clear
const unsigned int mem_locks_overflow_0 = 0x000c0420;
// Status bits for lock overflow, write to clear
const unsigned int mem_locks_overflow_1 = 0x000c0424;
// Status bits for lock underflow, write to clear
const unsigned int mem_locks_underflow_0 = 0x000c0428;
// Status bits for lock underflow, write to clear
const unsigned int mem_locks_underflow_1 = 0x000c042c;
// Lock Request. 65kB address space: = 0xD0000 - = 0xDFFFC, Lock_Id [15:10], Acq_Rel (9), Change_Value [8:2]
const unsigned int mem_lock_request = 0x000d0000;
// Control clock gating of modules (privileged)
const unsigned int mem_module_clock_control = 0x000fff00;
// Reset of modules (privileged)
const unsigned int mem_module_reset_control = 0x000fff10;

// Register definitions for SHIM
// ###################################
// Step size between lock registers
const unsigned int shim_lock_step_size = 0x10;
// Step size between DMA BD register groups
const unsigned int shim_dma_bd_step_size = 0x20;
// Step size between DMA S2MM register groups
const unsigned int shim_dma_s2mm_step_size = 0x8;
// Value of lock 0
const unsigned int shim_lock0_value = 0x00014000;
// Value of lock 1
const unsigned int shim_lock1_value = 0x00014010;
// Value of lock 2
const unsigned int shim_lock2_value = 0x00014020;
// Value of lock 3
const unsigned int shim_lock3_value = 0x00014030;
// Value of lock 4
const unsigned int shim_lock4_value = 0x00014040;
// Value of lock 5
const unsigned int shim_lock5_value = 0x00014050;
// Value of lock 6
const unsigned int shim_lock6_value = 0x00014060;
// Value of lock 7
const unsigned int shim_lock7_value = 0x00014070;
// Value of lock 8
const unsigned int shim_lock8_value = 0x00014080;
// Value of lock 9
const unsigned int shim_lock9_value = 0x00014090;
// Value of lock 10
const unsigned int shim_lock10_value = 0x000140a0;
// Value of lock 11
const unsigned int shim_lock11_value = 0x000140b0;
// Value of lock 12
const unsigned int shim_lock12_value = 0x000140c0;
// Value of lock 13
const unsigned int shim_lock13_value = 0x000140d0;
// Value of lock 14
const unsigned int shim_lock14_value = 0x000140e0;
// Value of lock 15
const unsigned int shim_lock15_value = 0x000140f0;
// DMA BD0 Config Register 0
const unsigned int shim_dma_bd0_0 = 0x0001d000;
// DMA BD0 Config Register 1
const unsigned int shim_dma_bd0_1 = 0x0001d004;
// DMA BD0 Config Register 2
const unsigned int shim_dma_bd0_2 = 0x0001d008;
// DMA BD0 Config Register 3
const unsigned int shim_dma_bd0_3 = 0x0001d00c;
// DMA BD0 Config Register 4
const unsigned int shim_dma_bd0_4 = 0x0001d010;
// DMA BD0 Config Register 5
const unsigned int shim_dma_bd0_5 = 0x0001d014;
// DMA BD0 Config Register 6
const unsigned int shim_dma_bd0_6 = 0x0001d018;
// DMA BD0 Config Register 7
const unsigned int shim_dma_bd0_7 = 0x0001d01c;
// DMA BD1 Config Register 0
const unsigned int shim_dma_bd1_0 = 0x0001d020;
// DMA BD1 Config Register 1
const unsigned int shim_dma_bd1_1 = 0x0001d024;
// DMA BD1 Config Register 2
const unsigned int shim_dma_bd1_2 = 0x0001d028;
// DMA BD1 Config Register 3
const unsigned int shim_dma_bd1_3 = 0x0001d02c;
// DMA BD1 Config Register 4
const unsigned int shim_dma_bd1_4 = 0x0001d030;
// DMA BD1 Config Register 5
const unsigned int shim_dma_bd1_5 = 0x0001d034;
// DMA BD1 Config Register 6
const unsigned int shim_dma_bd1_6 = 0x0001d038;
// DMA BD1 Config Register 7
const unsigned int shim_dma_bd1_7 = 0x0001d03c;
// DMA BD2 Config Register 0
const unsigned int shim_dma_bd2_0 = 0x0001d040;
// DMA BD2 Config Register 1
const unsigned int shim_dma_bd2_1 = 0x0001d044;
// DMA BD2 Config Register 2
const unsigned int shim_dma_bd2_2 = 0x0001d048;
// DMA BD2 Config Register 3
const unsigned int shim_dma_bd2_3 = 0x0001d04c;
// DMA BD2 Config Register 4
const unsigned int shim_dma_bd2_4 = 0x0001d050;
// DMA BD2 Config Register 5
const unsigned int shim_dma_bd2_5 = 0x0001d054;
// DMA BD2 Config Register 6
const unsigned int shim_dma_bd2_6 = 0x0001d058;
// DMA BD2 Config Register 7
const unsigned int shim_dma_bd2_7 = 0x0001d05c;
// DMA BD3 Config Register 0
const unsigned int shim_dma_bd3_0 = 0x0001d060;
// DMA BD3 Config Register 1
const unsigned int shim_dma_bd3_1 = 0x0001d064;
// DMA BD3 Config Register 2
const unsigned int shim_dma_bd3_2 = 0x0001d068;
// DMA BD3 Config Register 3
const unsigned int shim_dma_bd3_3 = 0x0001d06c;
// DMA BD3 Config Register 4
const unsigned int shim_dma_bd3_4 = 0x0001d070;
// DMA BD3 Config Register 5
const unsigned int shim_dma_bd3_5 = 0x0001d074;
// DMA BD3 Config Register 6
const unsigned int shim_dma_bd3_6 = 0x0001d078;
// DMA BD3 Config Register 7
const unsigned int shim_dma_bd3_7 = 0x0001d07c;
// DMA BD4 Config Register 0
const unsigned int shim_dma_bd4_0 = 0x0001d080;
// DMA BD4 Config Register 1
const unsigned int shim_dma_bd4_1 = 0x0001d084;
// DMA BD4 Config Register 2
const unsigned int shim_dma_bd4_2 = 0x0001d088;
// DMA BD4 Config Register 3
const unsigned int shim_dma_bd4_3 = 0x0001d08c;
// DMA BD4 Config Register 4
const unsigned int shim_dma_bd4_4 = 0x0001d090;
// DMA BD4 Config Register 5
const unsigned int shim_dma_bd4_5 = 0x0001d094;
// DMA BD4 Config Register 6
const unsigned int shim_dma_bd4_6 = 0x0001d098;
// DMA BD4 Config Register 7
const unsigned int shim_dma_bd4_7 = 0x0001d09c;
// DMA BD5 Config Register 0
const unsigned int shim_dma_bd5_0 = 0x0001d0a0;
// DMA BD5 Config Register 1
const unsigned int shim_dma_bd5_1 = 0x0001d0a4;
// DMA BD5 Config Register 2
const unsigned int shim_dma_bd5_2 = 0x0001d0a8;
// DMA BD5 Config Register 3
const unsigned int shim_dma_bd5_3 = 0x0001d0ac;
// DMA BD5 Config Register 4
const unsigned int shim_dma_bd5_4 = 0x0001d0b0;
// DMA BD5 Config Register 5
const unsigned int shim_dma_bd5_5 = 0x0001d0b4;
// DMA BD5 Config Register 6
const unsigned int shim_dma_bd5_6 = 0x0001d0b8;
// DMA BD5 Config Register 7
const unsigned int shim_dma_bd5_7 = 0x0001d0bc;
// DMA BD6 Config Register 0
const unsigned int shim_dma_bd6_0 = 0x0001d0c0;
// DMA BD6 Config Register 1
const unsigned int shim_dma_bd6_1 = 0x0001d0c4;
// DMA BD6 Config Register 2
const unsigned int shim_dma_bd6_2 = 0x0001d0c8;
// DMA BD6 Config Register 3
const unsigned int shim_dma_bd6_3 = 0x0001d0cc;
// DMA BD6 Config Register 4
const unsigned int shim_dma_bd6_4 = 0x0001d0d0;
// DMA BD6 Config Register 5
const unsigned int shim_dma_bd6_5 = 0x0001d0d4;
// DMA BD6 Config Register 6
const unsigned int shim_dma_bd6_6 = 0x0001d0d8;
// DMA BD6 Config Register 7
const unsigned int shim_dma_bd6_7 = 0x0001d0dc;
// DMA BD7 Config Register 0
const unsigned int shim_dma_bd7_0 = 0x0001d0e0;
// DMA BD7 Config Register 1
const unsigned int shim_dma_bd7_1 = 0x0001d0e4;
// DMA BD7 Config Register 2
const unsigned int shim_dma_bd7_2 = 0x0001d0e8;
// DMA BD7 Config Register 3
const unsigned int shim_dma_bd7_3 = 0x0001d0ec;
// DMA BD7 Config Register 4
const unsigned int shim_dma_bd7_4 = 0x0001d0f0;
// DMA BD7 Config Register 5
const unsigned int shim_dma_bd7_5 = 0x0001d0f4;
// DMA BD7 Config Register 6
const unsigned int shim_dma_bd7_6 = 0x0001d0f8;
// DMA BD7 Config Register 7
const unsigned int shim_dma_bd7_7 = 0x0001d0fc;
// DMA BD8 Config Register 0
const unsigned int shim_dma_bd8_0 = 0x0001d100;
// DMA BD8 Config Register 1
const unsigned int shim_dma_bd8_1 = 0x0001d104;
// DMA BD8 Config Register 2
const unsigned int shim_dma_bd8_2 = 0x0001d108;
// DMA BD8 Config Register 3
const unsigned int shim_dma_bd8_3 = 0x0001d10c;
// DMA BD8 Config Register 4
const unsigned int shim_dma_bd8_4 = 0x0001d110;
// DMA BD8 Config Register 5
const unsigned int shim_dma_bd8_5 = 0x0001d114;
// DMA BD8 Config Register 6
const unsigned int shim_dma_bd8_6 = 0x0001d118;
// DMA BD8 Config Register 7
const unsigned int shim_dma_bd8_7 = 0x0001d11c;
// DMA BD9 Config Register 0
const unsigned int shim_dma_bd9_0 = 0x0001d120;
// DMA BD9 Config Register 1
const unsigned int shim_dma_bd9_1 = 0x0001d124;
// DMA BD9 Config Register 2
const unsigned int shim_dma_bd9_2 = 0x0001d128;
// DMA BD9 Config Register 3
const unsigned int shim_dma_bd9_3 = 0x0001d12c;
// DMA BD9 Config Register 4
const unsigned int shim_dma_bd9_4 = 0x0001d130;
// DMA BD9 Config Register 5
const unsigned int shim_dma_bd9_5 = 0x0001d134;
// DMA BD9 Config Register 6
const unsigned int shim_dma_bd9_6 = 0x0001d138;
// DMA BD9 Config Register 7
const unsigned int shim_dma_bd9_7 = 0x0001d13c;
// DMA BD10 Config Register 0
const unsigned int shim_dma_bd10_0 = 0x0001d140;
// DMA BD10 Config Register 1
const unsigned int shim_dma_bd10_1 = 0x0001d144;
// DMA BD10 Config Register 2
const unsigned int shim_dma_bd10_2 = 0x0001d148;
// DMA BD10 Config Register 3
const unsigned int shim_dma_bd10_3 = 0x0001d14c;
// DMA BD10 Config Register 4
const unsigned int shim_dma_bd10_4 = 0x0001d150;
// DMA BD10 Config Register 5
const unsigned int shim_dma_bd10_5 = 0x0001d154;
// DMA BD10 Config Register 6
const unsigned int shim_dma_bd10_6 = 0x0001d158;
// DMA BD10 Config Register 7
const unsigned int shim_dma_bd10_7 = 0x0001d15c;
// DMA BD11 Config Register 0
const unsigned int shim_dma_bd11_0 = 0x0001d160;
// DMA BD11 Config Register 1
const unsigned int shim_dma_bd11_1 = 0x0001d164;
// DMA BD11 Config Register 2
const unsigned int shim_dma_bd11_2 = 0x0001d168;
// DMA BD11 Config Register 3
const unsigned int shim_dma_bd11_3 = 0x0001d16c;
// DMA BD11 Config Register 4
const unsigned int shim_dma_bd11_4 = 0x0001d170;
// DMA BD11 Config Register 5
const unsigned int shim_dma_bd11_5 = 0x0001d174;
// DMA BD11 Config Register 6
const unsigned int shim_dma_bd11_6 = 0x0001d178;
// DMA BD11 Config Register 7
const unsigned int shim_dma_bd11_7 = 0x0001d17c;
// DMA BD12 Config Register 0
const unsigned int shim_dma_bd12_0 = 0x0001d180;
// DMA BD12 Config Register 1
const unsigned int shim_dma_bd12_1 = 0x0001d184;
// DMA BD12 Config Register 2
const unsigned int shim_dma_bd12_2 = 0x0001d188;
// DMA BD12 Config Register 3
const unsigned int shim_dma_bd12_3 = 0x0001d18c;
// DMA BD12 Config Register 4
const unsigned int shim_dma_bd12_4 = 0x0001d190;
// DMA BD12 Config Register 5
const unsigned int shim_dma_bd12_5 = 0x0001d194;
// DMA BD12 Config Register 6
const unsigned int shim_dma_bd12_6 = 0x0001d198;
// DMA BD12 Config Register 7
const unsigned int shim_dma_bd12_7 = 0x0001d19c;
// DMA BD13 Config Register 0
const unsigned int shim_dma_bd13_0 = 0x0001d1a0;
// DMA BD13 Config Register 1
const unsigned int shim_dma_bd13_1 = 0x0001d1a4;
// DMA BD13 Config Register 2
const unsigned int shim_dma_bd13_2 = 0x0001d1a8;
// DMA BD13 Config Register 3
const unsigned int shim_dma_bd13_3 = 0x0001d1ac;
// DMA BD13 Config Register 4
const unsigned int shim_dma_bd13_4 = 0x0001d1b0;
// DMA BD13 Config Register 5
const unsigned int shim_dma_bd13_5 = 0x0001d1b4;
// DMA BD13 Config Register 6
const unsigned int shim_dma_bd13_6 = 0x0001d1b8;
// DMA BD13 Config Register 7
const unsigned int shim_dma_bd13_7 = 0x0001d1bc;
// DMA BD14 Config Register 0
const unsigned int shim_dma_bd14_0 = 0x0001d1c0;
// DMA BD14 Config Register 1
const unsigned int shim_dma_bd14_1 = 0x0001d1c4;
// DMA BD14 Config Register 2
const unsigned int shim_dma_bd14_2 = 0x0001d1c8;
// DMA BD14 Config Register 3
const unsigned int shim_dma_bd14_3 = 0x0001d1cc;
// DMA BD14 Config Register 4
const unsigned int shim_dma_bd14_4 = 0x0001d1d0;
// DMA BD14 Config Register 5
const unsigned int shim_dma_bd14_5 = 0x0001d1d4;
// DMA BD14 Config Register 6
const unsigned int shim_dma_bd14_6 = 0x0001d1d8;
// DMA BD14 Config Register 7
const unsigned int shim_dma_bd14_7 = 0x0001d1dc;
// DMA BD15 Config Register 0
const unsigned int shim_dma_bd15_0 = 0x0001d1e0;
// DMA BD15 Config Register 1
const unsigned int shim_dma_bd15_1 = 0x0001d1e4;
// DMA BD15 Config Register 2
const unsigned int shim_dma_bd15_2 = 0x0001d1e8;
// DMA BD15 Config Register 3
const unsigned int shim_dma_bd15_3 = 0x0001d1ec;
// DMA BD15 Config Register 4
const unsigned int shim_dma_bd15_4 = 0x0001d1f0;
// DMA BD15 Config Register 5
const unsigned int shim_dma_bd15_5 = 0x0001d1f4;
// DMA BD15 Config Register 6
const unsigned int shim_dma_bd15_6 = 0x0001d1f8;
// DMA BD15 Config Register 7
const unsigned int shim_dma_bd15_7 = 0x0001d1fc;
// DMA S2MM Channel 0 Control
const unsigned int shim_dma_s2mm_0_ctrl = 0x0001d200;
// DMA S2MM Channel 0 Start Queue
const unsigned int shim_dma_s2mm_0_task_queue = 0x0001d204;
// DMA S2MM Channel 0 Status
const unsigned int shim_dma_s2mm_status_0 = 0x0001d220;
// DMA S2MM Channel 1 Status
const unsigned int shim_dma_s2mm_status_1 = 0x0001d224;
// DMA MM2S Channel 0 Status
const unsigned int shim_dma_mm2s_status_0 = 0x0001d228;
// DMA MM2S Channel 1 Status
const unsigned int shim_dma_mm2s_status_1 = 0x0001d22c;
// Performance Counters 1-0 Start and Stop Event
const unsigned int shim_performance_control0 = 0x00031000;
const unsigned int shim_performance_start_stop_0_1 = 0x00031000;
// Performance Counters Reset Events
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

} // namespace aie2

#endif /* AIE2_REGISTERS_H_ */
