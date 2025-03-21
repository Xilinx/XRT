// 67d7842dbbe25473c3c32b93c0da8047785f30d78e8a024de1b57352245f9689
// #######################################################################
// Copyright (c) 2022 AMD, Inc.  All rights reserved.
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

#ifndef AIE1_REGISTERS_H_
#define AIE1_REGISTERS_H_

namespace aie1 
{

// Register definitions for AIE1
// ###################################

// Register definitions for CM
// ###################################
// Combo event control
const unsigned int cm_combo_event_control = 0x00034404;
// Combo event inputs
const unsigned int cm_combo_event_inputs = 0x00034400;
// Core AMH0 Part1
const unsigned int cm_core_amh0_part1 = 0x00030660;
// Core AMH0 Part2
const unsigned int cm_core_amh0_part2 = 0x00030670;
// Core AMH0 Part3
const unsigned int cm_core_amh0_part3 = 0x00030680;
// Core AMH1 Part1
const unsigned int cm_core_amh1_part1 = 0x000306C0;
// Core AMH1 Part2
const unsigned int cm_core_amh1_part2 = 0x000306D0;
// Core AMH1 Part3
const unsigned int cm_core_amh1_part3 = 0x000306E0;
// Core AMH2 Part1
const unsigned int cm_core_amh2_part1 = 0x00030720;
// Core AMH2 Part2
const unsigned int cm_core_amh2_part2 = 0x00030730;
// Core AMH2 Part3
const unsigned int cm_core_amh2_part3 = 0x00030740;
// Core AMH3 Part1
const unsigned int cm_core_amh3_part1 = 0x00030780;
// Core AMH3 Part2
const unsigned int cm_core_amh3_part2 = 0x00030790;
// Core AMH3 Part3
const unsigned int cm_core_amh3_part3 = 0x000307A0;
// Core AML0 Part1
const unsigned int cm_core_aml0_part1 = 0x00030630;
// Core AML0 Part2
const unsigned int cm_core_aml0_part2 = 0x00030640;
// Core AML0 Part3
const unsigned int cm_core_aml0_part3 = 0x00030650;
// Core AML1 Part1
const unsigned int cm_core_aml1_part1 = 0x00030690;
// Core AML1 Part2
const unsigned int cm_core_aml1_part2 = 0x000306A0;
// Core AML1 Part3
const unsigned int cm_core_aml1_part3 = 0x000306B0;
// Core AML2 Part1
const unsigned int cm_core_aml2_part1 = 0x000306F0;
// Core AML2 Part2
const unsigned int cm_core_aml2_part2 = 0x00030700;
// Core AML2 Part3
const unsigned int cm_core_aml2_part3 = 0x00030710;
// Core AML3 Part1
const unsigned int cm_core_aml3_part1 = 0x00030750;
// Core AML3 Part2
const unsigned int cm_core_aml3_part2 = 0x00030760;
// Core AML3 Part3
const unsigned int cm_core_aml3_part3 = 0x00030770;
// Core CB0
const unsigned int cm_core_cb0 = 0x00030340;
// Core CB1
const unsigned int cm_core_cb1 = 0x00030350;
// Core CB2
const unsigned int cm_core_cb2 = 0x00030360;
// Core CB3
const unsigned int cm_core_cb3 = 0x00030370;
// Core CB4
const unsigned int cm_core_cb4 = 0x00030380;
// Core CB5
const unsigned int cm_core_cb5 = 0x00030390;
// Core CB6
const unsigned int cm_core_cb6 = 0x000303A0;
// Core CB7
const unsigned int cm_core_cb7 = 0x000303B0;
// Core CH0
const unsigned int cm_core_ch0 = 0x00030190;
// Core CH1
const unsigned int cm_core_ch1 = 0x000301B0;
// Core CH2
const unsigned int cm_core_ch2 = 0x000301D0;
// Core CH3
const unsigned int cm_core_ch3 = 0x000301F0;
// Core CH4
const unsigned int cm_core_ch4 = 0x00030210;
// Core CH5
const unsigned int cm_core_ch5 = 0x00030230;
// Core CH6
const unsigned int cm_core_ch6 = 0x00030250;
// Core CH7
const unsigned int cm_core_ch7 = 0x00030270;
// Core CL0
const unsigned int cm_core_cl0 = 0x00030180;
// Core CL1
const unsigned int cm_core_cl1 = 0x000301A0;
// Core CL2
const unsigned int cm_core_cl2 = 0x000301C0;
// Core CL3
const unsigned int cm_core_cl3 = 0x000301E0;
// Core CL4
const unsigned int cm_core_cl4 = 0x00030200;
// Core CL5
const unsigned int cm_core_cl5 = 0x00030220;
// Core CL6
const unsigned int cm_core_cl6 = 0x00030240;
// Core CL7
const unsigned int cm_core_cl7 = 0x00030260;
// Core Control
const unsigned int cm_core_control = 0x00032000;
// Core CS0
const unsigned int cm_core_cs0 = 0x000303C0;
// Core CS1
const unsigned int cm_core_cs1 = 0x000303D0;
// Core CS2
const unsigned int cm_core_cs2 = 0x000303E0;
// Core CS3
const unsigned int cm_core_cs3 = 0x000303F0;
// Core CS4
const unsigned int cm_core_cs4 = 0x00030400;
// Core CS5
const unsigned int cm_core_cs5 = 0x00030410;
// Core CS6
const unsigned int cm_core_cs6 = 0x00030420;
// Core CS7
const unsigned int cm_core_cs7 = 0x00030430;
// Core FC
const unsigned int cm_core_fc = 0x00030290;
// Core LC
const unsigned int cm_core_lc = 0x00030520;
// Core LE
const unsigned int cm_core_le = 0x00030510;
// Core LR
const unsigned int cm_core_lr = 0x000302B0;
// Core LS
const unsigned int cm_core_ls = 0x00030500;
// Core M0
const unsigned int cm_core_m0 = 0x000302C0;
// Core M1
const unsigned int cm_core_m1 = 0x000302D0;
// Core M2
const unsigned int cm_core_m2 = 0x000302E0;
// Core M3
const unsigned int cm_core_m3 = 0x000302F0;
// Core M4
const unsigned int cm_core_m4 = 0x00030300;
// Core M5
const unsigned int cm_core_m5 = 0x00030310;
// Core M6
const unsigned int cm_core_m6 = 0x00030320;
// Core M7
const unsigned int cm_core_m7 = 0x00030330;
// Core P0
const unsigned int cm_core_p0 = 0x00030100;
// Core P1
const unsigned int cm_core_p1 = 0x00030110;
// Core P2
const unsigned int cm_core_p2 = 0x00030120;
// Core P3
const unsigned int cm_core_p3 = 0x00030130;
// Core P4
const unsigned int cm_core_p4 = 0x00030140;
// Core P5
const unsigned int cm_core_p5 = 0x00030150;
// Core P6
const unsigned int cm_core_p6 = 0x00030160;
// Core P7
const unsigned int cm_core_p7 = 0x00030170;
// Core R0
const unsigned int cm_core_r0 = 0x00030000;
// Core R1
const unsigned int cm_core_r1 = 0x00030010;
// Core R10
const unsigned int cm_core_r10 = 0x000300A0;
// Core R11
const unsigned int cm_core_r11 = 0x000300B0;
// Core R12
const unsigned int cm_core_r12 = 0x000300C0;
// Core R13
const unsigned int cm_core_r13 = 0x000300D0;
// Core R14
const unsigned int cm_core_r14 = 0x000300E0;
// Core R15
const unsigned int cm_core_r15 = 0x000300F0;
// Core R2
const unsigned int cm_core_r2 = 0x00030020;
// Core R3
const unsigned int cm_core_r3 = 0x00030030;
// Core R4
const unsigned int cm_core_r4 = 0x00030040;
// Core R5
const unsigned int cm_core_r5 = 0x00030050;
// Core R6
const unsigned int cm_core_r6 = 0x00030060;
// Core R7
const unsigned int cm_core_r7 = 0x00030070;
// Core R8
const unsigned int cm_core_r8 = 0x00030080;
// Core R9
const unsigned int cm_core_r9 = 0x00030090;
// Core S0
const unsigned int cm_core_s0 = 0x00030480;
// Core S1
const unsigned int cm_core_s1 = 0x00030490;
// Core S2
const unsigned int cm_core_s2 = 0x000304A0;
// Core S3
const unsigned int cm_core_s3 = 0x000304B0;
// Core S4
const unsigned int cm_core_s4 = 0x000304C0;
// Core S5
const unsigned int cm_core_s5 = 0x000304D0;
// Core S6
const unsigned int cm_core_s6 = 0x000304E0;
// Core S7
const unsigned int cm_core_s7 = 0x000304F0;
// Core SP
const unsigned int cm_core_sp = 0x000302A0;
// Core Status
const unsigned int cm_core_status = 0x00032004;
// Core VCH0
const unsigned int cm_core_vch0 = 0x000305C0;
// Core VCH1
const unsigned int cm_core_vch1 = 0x000305E0;
// Core VCL0
const unsigned int cm_core_vcl0 = 0x000305B0;
// Core VCL1
const unsigned int cm_core_vcl1 = 0x000305D0;
// Core VDH0
const unsigned int cm_core_vdh0 = 0x00030600;
// Core VDH1
const unsigned int cm_core_vdh1 = 0x00030620;
// Core VDL0
const unsigned int cm_core_vdl0 = 0x000305F0;
// Core VDL1
const unsigned int cm_core_vdl1 = 0x00030610;
// Core VRH0
const unsigned int cm_core_vrh0 = 0x00030540;
// Core VRH1
const unsigned int cm_core_vrh1 = 0x00030560;
// Core VRH2
const unsigned int cm_core_vrh2 = 0x00030580;
// Core VRH3
const unsigned int cm_core_vrh3 = 0x000305A0;
// Core VRL0
const unsigned int cm_core_vrl0 = 0x00030530;
// Core VRL1
const unsigned int cm_core_vrl1 = 0x00030550;
// Core VRL2
const unsigned int cm_core_vrl2 = 0x00030570;
// Core VRL3
const unsigned int cm_core_vrl3 = 0x00030590;
// CSSD Trigger
const unsigned int cm_cssd_trigger = 0x00036044;
// Debug Control0
const unsigned int cm_debug_control0 = 0x00032010;
// Debug Control1
const unsigned int cm_debug_control1 = 0x00032014;
// Debug Control2
const unsigned int cm_debug_control2 = 0x00032018;
// Debug Status
const unsigned int cm_debug_status = 0x0003201C;
// ECC Control
const unsigned int cm_ecc_control = 0x00032100;
// ECC Failing Address
const unsigned int cm_ecc_failing_address = 0x00032120;
// ECC Scrubbing Event
const unsigned int cm_ecc_scrubbing_event = 0x00032110;
// Enable Events
const unsigned int cm_enable_events = 0x00032008;
// Error Halt Control
const unsigned int cm_error_halt_control = 0x00032030;
// Error Halt Event
const unsigned int cm_error_halt_event = 0x00032034;
// Event Broadcast0
const unsigned int cm_event_broadcast0 = 0x00034010;
// Event Broadcast1
const unsigned int cm_event_broadcast1 = 0x00034014;
// Event Broadcast10
const unsigned int cm_event_broadcast10 = 0x00034038;
// Event Broadcast11
const unsigned int cm_event_broadcast11 = 0x0003403C;
// Event Broadcast12
const unsigned int cm_event_broadcast12 = 0x00034040;
// Event Broadcast13
const unsigned int cm_event_broadcast13 = 0x00034044;
// Event Broadcast14
const unsigned int cm_event_broadcast14 = 0x00034048;
// Event Broadcast15
const unsigned int cm_event_broadcast15 = 0x0003404C;
// Event Broadcast2
const unsigned int cm_event_broadcast2 = 0x00034018;
// Event Broadcast3
const unsigned int cm_event_broadcast3 = 0x0003401C;
// Event Broadcast4
const unsigned int cm_event_broadcast4 = 0x00034020;
// Event Broadcast5
const unsigned int cm_event_broadcast5 = 0x00034024;
// Event Broadcast6
const unsigned int cm_event_broadcast6 = 0x00034028;
// Event Broadcast7
const unsigned int cm_event_broadcast7 = 0x0003402C;
// Event Broadcast8
const unsigned int cm_event_broadcast8 = 0x00034030;
// Event Broadcast9
const unsigned int cm_event_broadcast9 = 0x00034034;
// Event Broadcast Block East Clr
const unsigned int cm_event_broadcast_block_east_clr = 0x00034084;
// Event Broadcast Block East Set
const unsigned int cm_event_broadcast_block_east_set = 0x00034080;
// Event Broadcast Block East Value
const unsigned int cm_event_broadcast_block_east_value = 0x00034088;
// Event Broadcast Block North Clr
const unsigned int cm_event_broadcast_block_north_clr = 0x00034074;
// Event Broadcast Block North Set
const unsigned int cm_event_broadcast_block_north_set = 0x00034070;
// Event Broadcast Block North Value
const unsigned int cm_event_broadcast_block_north_value = 0x00034078;
// Event Broadcast Block South Clr
const unsigned int cm_event_broadcast_block_south_clr = 0x00034054;
// Event Broadcast Block South Set
const unsigned int cm_event_broadcast_block_south_set = 0x00034050;
// Event Broadcast Block South Value
const unsigned int cm_event_broadcast_block_south_value = 0x00034058;
// Event Broadcast Block West Clr
const unsigned int cm_event_broadcast_block_west_clr = 0x00034064;
// Event Broadcast Block West Set
const unsigned int cm_event_broadcast_block_west_set = 0x00034060;
// Event Broadcast Block West Value
const unsigned int cm_event_broadcast_block_west_value = 0x00034068;
// Event Generate
const unsigned int cm_event_generate = 0x00034008;
// Event Group 0 Enable
const unsigned int cm_event_group_0_enable = 0x00034500;
// Event Group Broadcast Enable
const unsigned int cm_event_group_broadcast_enable = 0x0003451C;
// Event Group Core Program Flow Enable
const unsigned int cm_event_group_core_program_flow_enable = 0x0003450C;
// Event Group Core Stall Enable
const unsigned int cm_event_group_core_stall_enable = 0x00034508;
// Event Group Errors0 Enable
const unsigned int cm_event_group_errors0_enable = 0x00034510;
// Event Group Errors1 Enable
const unsigned int cm_event_group_errors1_enable = 0x00034514;
// Event Group PC Enable
const unsigned int cm_event_group_pc_enable = 0x00034504;
// Event Group Stream Switch Enable
const unsigned int cm_event_group_stream_switch_enable = 0x00034518;
// Event Group User Event Enable
const unsigned int cm_event_group_user_event_enable = 0x00034520;
// Event Status0
const unsigned int cm_event_status0 = 0x00034200;
// Event Status1
const unsigned int cm_event_status1 = 0x00034204;
// Event Status2
const unsigned int cm_event_status2 = 0x00034208;
// Event Status3
const unsigned int cm_event_status3 = 0x0003420C;
// Core MC0
const unsigned int cm_mc0 = 0x00030460;
// Core MC1
const unsigned int cm_mc1 = 0x00030470;
// Core MD0
const unsigned int cm_md0 = 0x00030440;
// Core MD1
const unsigned int cm_md1 = 0x00030450;
// PC Event0
const unsigned int cm_pc_event0 = 0x00032020;
// PC Event1
const unsigned int cm_pc_event1 = 0x00032024;
// PC Event2
const unsigned int cm_pc_event2 = 0x00032028;
// PC Event3
const unsigned int cm_pc_event3 = 0x0003202C;
// Performance Counters 1-0 Start and Stop Event
const unsigned int cm_performance_control0 = 0x00031000;
// Performance Counters 3-2 Start and Stop Event
const unsigned int cm_performance_control1 = 0x00031004;
// Performance Counters Reset Events
const unsigned int cm_performance_control2 = 0x00031008;
// Performance Counter0
const unsigned int cm_performance_counter0 = 0x00031020;
// Performance Counter0 Event Value
const unsigned int cm_performance_counter0_event_value = 0x00031080;
// Performance Counter1
const unsigned int cm_performance_counter1 = 0x00031024;
// Performance Counter1 Event Value
const unsigned int cm_performance_counter1_event_value = 0x00031084;
// Performance Counter2
const unsigned int cm_performance_counter2 = 0x00031028;
// Performance Counter2 Event Value
const unsigned int cm_performance_counter2_event_value = 0x00031088;
// Performance Counter3
const unsigned int cm_performance_counter3 = 0x0003102C;
// Performance Counter3 Event Value
const unsigned int cm_performance_counter3_event_value = 0x0003108C;
// Program Counter
const unsigned int cm_program_counter = 0x00030280;
// Program Memory
const unsigned int cm_program_memory = 0x00020000;
// Program Memory Error Injection
const unsigned int cm_program_memory_error_injection = 0x00024000;
// Reserved0
const unsigned int cm_reserved0 = 0x00032130;
// Reserved1
const unsigned int cm_reserved1 = 0x00032134;
// Reserved2
const unsigned int cm_reserved2 = 0x00032138;
// Reserved3
const unsigned int cm_reserved3 = 0x0003213C;
// Reset Event
const unsigned int cm_reset_event = 0x0003200C;
// Spare Reg
const unsigned int cm_spare_reg = 0x00036050;
// Stream Switch Event Port Selection 0
const unsigned int cm_stream_switch_event_port_selection_0 = 0x0003FF00;
// Stream Switch Event Port Selection 1
const unsigned int cm_stream_switch_event_port_selection_1 = 0x0003FF04;
// Stream Switch Master Configuration AI Engine 0
const unsigned int cm_stream_switch_master_config_aie_core0 = 0x0003F000;
// Stream Switch Master Configuration AI Engine 1
const unsigned int cm_stream_switch_master_config_aie_core1 = 0x0003F004;
// Stream Switch Master Config DMA0
const unsigned int cm_stream_switch_master_config_dma0 = 0x0003F008;
// Stream Switch Master Config DMA1
const unsigned int cm_stream_switch_master_config_dma1 = 0x0003F00C;
// Stream Switch Master Config East0
const unsigned int cm_stream_switch_master_config_east0 = 0x0003F054;
// Stream Switch Master Config East1
const unsigned int cm_stream_switch_master_config_east1 = 0x0003F058;
// Stream Switch Master Config East2
const unsigned int cm_stream_switch_master_config_east2 = 0x0003F05C;
// Stream Switch Master Config East3
const unsigned int cm_stream_switch_master_config_east3 = 0x0003F060;
// Stream Switch Master Config FIFO0
const unsigned int cm_stream_switch_master_config_fifo0 = 0x0003F014;
// Stream Switch Master Config FIFO1
const unsigned int cm_stream_switch_master_config_fifo1 = 0x0003F018;
// Stream Switch Master Config North0
const unsigned int cm_stream_switch_master_config_north0 = 0x0003F03C;
// Stream Switch Master Config North1
const unsigned int cm_stream_switch_master_config_north1 = 0x0003F040;
// Stream Switch Master Config North2
const unsigned int cm_stream_switch_master_config_north2 = 0x0003F044;
// Stream Switch Master Config North3
const unsigned int cm_stream_switch_master_config_north3 = 0x0003F048;
// Stream Switch Master Config North4
const unsigned int cm_stream_switch_master_config_north4 = 0x0003F04C;
// Stream Switch Master Config North5
const unsigned int cm_stream_switch_master_config_north5 = 0x0003F050;
// Stream Switch Master Config South0
const unsigned int cm_stream_switch_master_config_south0 = 0x0003F01C;
// Stream Switch Master Config South1
const unsigned int cm_stream_switch_master_config_south1 = 0x0003F020;
// Stream Switch Master Config South2
const unsigned int cm_stream_switch_master_config_south2 = 0x0003F024;
// Stream Switch Master Config South3
const unsigned int cm_stream_switch_master_config_south3 = 0x0003F028;
// Stream Switch Master Config Tile Ctrl
const unsigned int cm_stream_switch_master_config_tile_ctrl = 0x0003F010;
// Stream Switch Master Config West0
const unsigned int cm_stream_switch_master_config_west0 = 0x0003F02C;
// Stream Switch Master Config West1
const unsigned int cm_stream_switch_master_config_west1 = 0x0003F030;
// Stream Switch Master Config West2
const unsigned int cm_stream_switch_master_config_west2 = 0x0003F034;
// Stream Switch Master Config West3
const unsigned int cm_stream_switch_master_config_west3 = 0x0003F038;
// Stream Switch Slave Configuration AI Engine 0
const unsigned int cm_stream_switch_slave_aie_core0_slot0 = 0x0003F200;
// Stream Switch Slave Configuration AI Engine 0
const unsigned int cm_stream_switch_slave_aie_core0_slot1 = 0x0003F204;
// Stream Switch Slave Configuration AI Engine 0
const unsigned int cm_stream_switch_slave_aie_core0_slot2 = 0x0003F208;
// Stream Switch Slave Configuration AI Engine 0
const unsigned int cm_stream_switch_slave_aie_core0_slot3 = 0x0003F20C;
// Stream Switch Slave Configuration AI Engine 1
const unsigned int cm_stream_switch_slave_aie_core1_slot0 = 0x0003F210;
// Stream Switch Slave Configuration AI Engine 1
const unsigned int cm_stream_switch_slave_aie_core1_slot1 = 0x0003F214;
// Stream Switch Slave Configuration AI Engine 1
const unsigned int cm_stream_switch_slave_aie_core1_slot2 = 0x0003F218;
// Stream Switch Slave Configuration AI Engine 1
const unsigned int cm_stream_switch_slave_aie_core1_slot3 = 0x0003F21C;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int cm_stream_switch_slave_aie_trace_slot0 = 0x0003F390;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int cm_stream_switch_slave_aie_trace_slot1 = 0x0003F394;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int cm_stream_switch_slave_aie_trace_slot2 = 0x0003F398;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int cm_stream_switch_slave_aie_trace_slot3 = 0x0003F39C;
// Stream Switch Slave Configuration AI Engine 0
const unsigned int cm_stream_switch_slave_config_aie_core0 = 0x0003F100;
// Stream Switch Slave Configuration AI Engine 1
const unsigned int cm_stream_switch_slave_config_aie_core1 = 0x0003F104;
// Stream Switch Slave Configuration AI Engine Trace
const unsigned int cm_stream_switch_slave_config_aie_trace = 0x0003F164;
// Stream Switch Slave Configuration DMA 0
const unsigned int cm_stream_switch_slave_config_dma_0 = 0x0003F108;
// Stream Switch Slave Configuration DMA 1
const unsigned int cm_stream_switch_slave_config_dma_1 = 0x0003F10C;
// Stream Switch Slave Configuration East 0
const unsigned int cm_stream_switch_slave_config_east_0 = 0x0003F154;
// Stream Switch Slave Configuration East 1
const unsigned int cm_stream_switch_slave_config_east_1 = 0x0003F158;
// Stream Switch Slave Configuration East 2
const unsigned int cm_stream_switch_slave_config_east_2 = 0x0003F15C;
// Stream Switch Slave Configuration East 3
const unsigned int cm_stream_switch_slave_config_east_3 = 0x0003F160;
// Stream Switch Slave Configuration FIFO 0
const unsigned int cm_stream_switch_slave_config_fifo_0 = 0x0003F114;
// Stream Switch Slave Configuration FIFO 1
const unsigned int cm_stream_switch_slave_config_fifo_1 = 0x0003F118;
// Stream Switch Slave Configuration Mem Trace
const unsigned int cm_stream_switch_slave_config_mem_trace = 0x0003F168;
// Stream Switch Slave Configuration North 0
const unsigned int cm_stream_switch_slave_config_north_0 = 0x0003F144;
// Stream Switch Slave Configuration North 1
const unsigned int cm_stream_switch_slave_config_north_1 = 0x0003F148;
// Stream Switch Slave Configuration North 2
const unsigned int cm_stream_switch_slave_config_north_2 = 0x0003F14C;
// Stream Switch Slave Configuration North 3
const unsigned int cm_stream_switch_slave_config_north_3 = 0x0003F150;
// Stream Switch Slave Configuration South 0
const unsigned int cm_stream_switch_slave_config_south_0 = 0x0003F11C;
// Stream Switch Slave Configuration South 1
const unsigned int cm_stream_switch_slave_config_south_1 = 0x0003F120;
// Stream Switch Slave Configuration South 2
const unsigned int cm_stream_switch_slave_config_south_2 = 0x0003F124;
// Stream Switch Slave Configuration South 3
const unsigned int cm_stream_switch_slave_config_south_3 = 0x0003F128;
// Stream Switch Slave Configuration South 4
const unsigned int cm_stream_switch_slave_config_south_4 = 0x0003F12C;
// Stream Switch Slave Configuration South 5
const unsigned int cm_stream_switch_slave_config_south_5 = 0x0003F130;
// Stream Switch Slave Configuration Tile Ctrl
const unsigned int cm_stream_switch_slave_config_tile_ctrl = 0x0003F110;
// Stream Switch Slave Configuration West 0
const unsigned int cm_stream_switch_slave_config_west_0 = 0x0003F134;
// Stream Switch Slave Configuration West 1
const unsigned int cm_stream_switch_slave_config_west_1 = 0x0003F138;
// Stream Switch Slave Configuration West 2
const unsigned int cm_stream_switch_slave_config_west_2 = 0x0003F13C;
// Stream Switch Slave Configuration West 3
const unsigned int cm_stream_switch_slave_config_west_3 = 0x0003F140;
// Stream Switch Slave DMA 0 Slot0
const unsigned int cm_stream_switch_slave_dma_0_slot0 = 0x0003F220;
// Stream Switch Slave DMA 0 Slot1
const unsigned int cm_stream_switch_slave_dma_0_slot1 = 0x0003F224;
// Stream Switch Slave DMA 0 Slot2
const unsigned int cm_stream_switch_slave_dma_0_slot2 = 0x0003F228;
// Stream Switch Slave DMA 0 Slot3
const unsigned int cm_stream_switch_slave_dma_0_slot3 = 0x0003F22C;
// Stream Switch Slave DMA 1 Slot0
const unsigned int cm_stream_switch_slave_dma_1_slot0 = 0x0003F230;
// Stream Switch Slave DMA 1 Slot1
const unsigned int cm_stream_switch_slave_dma_1_slot1 = 0x0003F234;
// Stream Switch Slave DMA 1 Slot2
const unsigned int cm_stream_switch_slave_dma_1_slot2 = 0x0003F238;
// Stream Switch Slave DMA 1 Slot3
const unsigned int cm_stream_switch_slave_dma_1_slot3 = 0x0003F23C;
// Stream Switch Slave East 0 Slot0
const unsigned int cm_stream_switch_slave_east_0_slot0 = 0x0003F350;
// Stream Switch Slave East 0 Slot1
const unsigned int cm_stream_switch_slave_east_0_slot1 = 0x0003F354;
// Stream Switch Slave East 0 Slot2
const unsigned int cm_stream_switch_slave_east_0_slot2 = 0x0003F358;
// Stream Switch Slave East 0 Slot3
const unsigned int cm_stream_switch_slave_east_0_slot3 = 0x0003F35C;
// Stream Switch Slave East 1 Slot0
const unsigned int cm_stream_switch_slave_east_1_slot0 = 0x0003F360;
// Stream Switch Slave East 1 Slot1
const unsigned int cm_stream_switch_slave_east_1_slot1 = 0x0003F364;
// Stream Switch Slave East 1 Slot2
const unsigned int cm_stream_switch_slave_east_1_slot2 = 0x0003F368;
// Stream Switch Slave East 1 Slot3
const unsigned int cm_stream_switch_slave_east_1_slot3 = 0x0003F36C;
// Stream Switch Slave East 2 Slot0
const unsigned int cm_stream_switch_slave_east_2_slot0 = 0x0003F370;
// Stream Switch Slave East 2 Slot1
const unsigned int cm_stream_switch_slave_east_2_slot1 = 0x0003F374;
// Stream Switch Slave East 2 Slot2
const unsigned int cm_stream_switch_slave_east_2_slot2 = 0x0003F378;
// Stream Switch Slave East 2 Slot3
const unsigned int cm_stream_switch_slave_east_2_slot3 = 0x0003F37C;
// Stream Switch Slave East 3 Slot0
const unsigned int cm_stream_switch_slave_east_3_slot0 = 0x0003F380;
// Stream Switch Slave East 3 Slot1
const unsigned int cm_stream_switch_slave_east_3_slot1 = 0x0003F384;
// Stream Switch Slave East 3 Slot2
const unsigned int cm_stream_switch_slave_east_3_slot2 = 0x0003F388;
// Stream Switch Slave East 3 Slot3
const unsigned int cm_stream_switch_slave_east_3_slot3 = 0x0003F38C;
// Stream Switch Slave FIFO 0 Slot0
const unsigned int cm_stream_switch_slave_fifo_0_slot0 = 0x0003F250;
// Stream Switch Slave FIFO 0 Slot1
const unsigned int cm_stream_switch_slave_fifo_0_slot1 = 0x0003F254;
// Stream Switch Slave FIFO 0 Slot2
const unsigned int cm_stream_switch_slave_fifo_0_slot2 = 0x0003F258;
// Stream Switch Slave FIFO 0 Slot3
const unsigned int cm_stream_switch_slave_fifo_0_slot3 = 0x0003F25C;
// Stream Switch Slave FIFO 1 Slot0
const unsigned int cm_stream_switch_slave_fifo_1_slot0 = 0x0003F260;
// Stream Switch Slave FIFO 1 Slot1
const unsigned int cm_stream_switch_slave_fifo_1_slot1 = 0x0003F264;
// Stream Switch Slave FIFO 1 Slot2
const unsigned int cm_stream_switch_slave_fifo_1_slot2 = 0x0003F268;
// Stream Switch Slave FIFO 1 Slot3
const unsigned int cm_stream_switch_slave_fifo_1_slot3 = 0x0003F26C;
// Stream Switch Slave Mem Trace Slot0
const unsigned int cm_stream_switch_slave_mem_trace_slot0 = 0x0003F3A0;
// Stream Switch Slave Mem Trace Slot1
const unsigned int cm_stream_switch_slave_mem_trace_slot1 = 0x0003F3A4;
// Stream Switch Slave Mem Trace Slot2
const unsigned int cm_stream_switch_slave_mem_trace_slot2 = 0x0003F3A8;
// Stream Switch Slave Mem Trace Slot3
const unsigned int cm_stream_switch_slave_mem_trace_slot3 = 0x0003F3AC;
// Stream Switch Slave North 0 Slot0
const unsigned int cm_stream_switch_slave_north_0_slot0 = 0x0003F310;
// Stream Switch Slave North 0 Slot1
const unsigned int cm_stream_switch_slave_north_0_slot1 = 0x0003F314;
// Stream Switch Slave North 0 Slot2
const unsigned int cm_stream_switch_slave_north_0_slot2 = 0x0003F318;
// Stream Switch Slave North 0 Slot3
const unsigned int cm_stream_switch_slave_north_0_slot3 = 0x0003F31C;
// Stream Switch Slave North 1 Slot0
const unsigned int cm_stream_switch_slave_north_1_slot0 = 0x0003F320;
// Stream Switch Slave North 1 Slot1
const unsigned int cm_stream_switch_slave_north_1_slot1 = 0x0003F324;
// Stream Switch Slave North 1 Slot2
const unsigned int cm_stream_switch_slave_north_1_slot2 = 0x0003F328;
// Stream Switch Slave North 1 Slot3
const unsigned int cm_stream_switch_slave_north_1_slot3 = 0x0003F32C;
// Stream Switch Slave North 2 Slot0
const unsigned int cm_stream_switch_slave_north_2_slot0 = 0x0003F330;
// Stream Switch Slave North 2 Slot1
const unsigned int cm_stream_switch_slave_north_2_slot1 = 0x0003F334;
// Stream Switch Slave North 2 Slot2
const unsigned int cm_stream_switch_slave_north_2_slot2 = 0x0003F338;
// Stream Switch Slave North 2 Slot3
const unsigned int cm_stream_switch_slave_north_2_slot3 = 0x0003F33C;
// Stream Switch Slave North 3 Slot0
const unsigned int cm_stream_switch_slave_north_3_slot0 = 0x0003F340;
// Stream Switch Slave North 3 Slot1
const unsigned int cm_stream_switch_slave_north_3_slot1 = 0x0003F344;
// Stream Switch Slave North 3 Slot2
const unsigned int cm_stream_switch_slave_north_3_slot2 = 0x0003F348;
// Stream Switch Slave North 3 Slot3
const unsigned int cm_stream_switch_slave_north_3_slot3 = 0x0003F34C;
// Stream Switch Slave South 0 Slot0
const unsigned int cm_stream_switch_slave_south_0_slot0 = 0x0003F270;
// Stream Switch Slave South 0 Slot1
const unsigned int cm_stream_switch_slave_south_0_slot1 = 0x0003F274;
// Stream Switch Slave South 0 Slot2
const unsigned int cm_stream_switch_slave_south_0_slot2 = 0x0003F278;
// Stream Switch Slave South 0 Slot3
const unsigned int cm_stream_switch_slave_south_0_slot3 = 0x0003F27C;
// Stream Switch Slave South 1 Slot0
const unsigned int cm_stream_switch_slave_south_1_slot0 = 0x0003F280;
// Stream Switch Slave South 1 Slot1
const unsigned int cm_stream_switch_slave_south_1_slot1 = 0x0003F284;
// Stream Switch Slave South 1 Slot2
const unsigned int cm_stream_switch_slave_south_1_slot2 = 0x0003F288;
// Stream Switch Slave South 1 Slot3
const unsigned int cm_stream_switch_slave_south_1_slot3 = 0x0003F28C;
// Stream Switch Slave South 2 Slot0
const unsigned int cm_stream_switch_slave_south_2_slot0 = 0x0003F290;
// Stream Switch Slave South 2 Slot1
const unsigned int cm_stream_switch_slave_south_2_slot1 = 0x0003F294;
// Stream Switch Slave South 2 Slot2
const unsigned int cm_stream_switch_slave_south_2_slot2 = 0x0003F298;
// Stream Switch Slave South 2 Slot3
const unsigned int cm_stream_switch_slave_south_2_slot3 = 0x0003F29C;
// Stream Switch Slave South 3 Slot0
const unsigned int cm_stream_switch_slave_south_3_slot0 = 0x0003F2A0;
// Stream Switch Slave South 3 Slot1
const unsigned int cm_stream_switch_slave_south_3_slot1 = 0x0003F2A4;
// Stream Switch Slave South 3 Slot2
const unsigned int cm_stream_switch_slave_south_3_slot2 = 0x0003F2A8;
// Stream Switch Slave South 3 Slot3
const unsigned int cm_stream_switch_slave_south_3_slot3 = 0x0003F2AC;
// Stream Switch Slave South 4 Slot0
const unsigned int cm_stream_switch_slave_south_4_slot0 = 0x0003F2B0;
// Stream Switch Slave South 4 Slot1
const unsigned int cm_stream_switch_slave_south_4_slot1 = 0x0003F2B4;
// Stream Switch Slave South 4 Slot2
const unsigned int cm_stream_switch_slave_south_4_slot2 = 0x0003F2B8;
// Stream Switch Slave South 4 Slot3
const unsigned int cm_stream_switch_slave_south_4_slot3 = 0x0003F2BC;
// Stream Switch Slave South 5 Slot0
const unsigned int cm_stream_switch_slave_south_5_slot0 = 0x0003F2C0;
// Stream Switch Slave South 5 Slot1
const unsigned int cm_stream_switch_slave_south_5_slot1 = 0x0003F2C4;
// Stream Switch Slave South 5 Slot2
const unsigned int cm_stream_switch_slave_south_5_slot2 = 0x0003F2C8;
// Stream Switch Slave South 5 Slot3
const unsigned int cm_stream_switch_slave_south_5_slot3 = 0x0003F2CC;
// Stream Switch Slave Tile Ctrl Slot0
const unsigned int cm_stream_switch_slave_tile_ctrl_slot0 = 0x0003F240;
// Stream Switch Slave Tile Ctrl Slot1
const unsigned int cm_stream_switch_slave_tile_ctrl_slot1 = 0x0003F244;
// Stream Switch Slave Tile Ctrl Slot2
const unsigned int cm_stream_switch_slave_tile_ctrl_slot2 = 0x0003F248;
// Stream Switch Slave Tile Ctrl Slot3
const unsigned int cm_stream_switch_slave_tile_ctrl_slot3 = 0x0003F24C;
// Stream Switch Slave West 0 Slot0
const unsigned int cm_stream_switch_slave_west_0_slot0 = 0x0003F2D0;
// Stream Switch Slave West 0 Slot1
const unsigned int cm_stream_switch_slave_west_0_slot1 = 0x0003F2D4;
// Stream Switch Slave West 0 Slot2
const unsigned int cm_stream_switch_slave_west_0_slot2 = 0x0003F2D8;
// Stream Switch Slave West 0 Slot3
const unsigned int cm_stream_switch_slave_west_0_slot3 = 0x0003F2DC;
// Stream Switch Slave West 1 Slot0
const unsigned int cm_stream_switch_slave_west_1_slot0 = 0x0003F2E0;
// Stream Switch Slave West 1 Slot1
const unsigned int cm_stream_switch_slave_west_1_slot1 = 0x0003F2E4;
// Stream Switch Slave West 1 Slot2
const unsigned int cm_stream_switch_slave_west_1_slot2 = 0x0003F2E8;
// Stream Switch Slave West 1 Slot3
const unsigned int cm_stream_switch_slave_west_1_slot3 = 0x0003F2EC;
// Stream Switch Slave West 2 Slot0
const unsigned int cm_stream_switch_slave_west_2_slot0 = 0x0003F2F0;
// Stream Switch Slave West 2 Slot1
const unsigned int cm_stream_switch_slave_west_2_slot1 = 0x0003F2F4;
// Stream Switch Slave West 2 Slot2
const unsigned int cm_stream_switch_slave_west_2_slot2 = 0x0003F2F8;
// Stream Switch Slave West 2 Slot3
const unsigned int cm_stream_switch_slave_west_2_slot3 = 0x0003F2FC;
// Stream Switch Slave West 3 Slot0
const unsigned int cm_stream_switch_slave_west_3_slot0 = 0x0003F300;
// Stream Switch Slave West 3 Slot1
const unsigned int cm_stream_switch_slave_west_3_slot1 = 0x0003F304;
// Stream Switch Slave West 3 Slot2
const unsigned int cm_stream_switch_slave_west_3_slot2 = 0x0003F308;
// Stream Switch Slave West 3 Slot3
const unsigned int cm_stream_switch_slave_west_3_slot3 = 0x0003F30C;
// Tile Clock Control
const unsigned int cm_tile_clock_control = 0x00036040;
// Tile Control
const unsigned int cm_tile_control = 0x00036030;
// Tile Control Packet Handler Status
const unsigned int cm_tile_control_packet_handler_status = 0x00036034;
// Timer Control
const unsigned int cm_timer_control = 0x00034000;
// Timer High
const unsigned int cm_timer_high = 0x000340FC;
// Timer Low
const unsigned int cm_timer_low = 0x000340F8;
// Timer Trig Event High Value
const unsigned int cm_timer_trig_event_high_value = 0x000340F4;
// Timer Trig Event Low Value
const unsigned int cm_timer_trig_event_low_value = 0x000340F0;
// Trace Control0
const unsigned int cm_trace_control0 = 0x000340D0;
// Trace Control1
const unsigned int cm_trace_control1 = 0x000340D4;
// Trace Event0
const unsigned int cm_trace_event0 = 0x000340E0;
// Trace Event1
const unsigned int cm_trace_event1 = 0x000340E4;
// Trace Status
const unsigned int cm_trace_status = 0x000340D8;

// Register definitions for MEM
// ###################################
// NOTE: these are dummy values needed by scripts but never used
// Performance Counter0
const unsigned int mem_performance_counter0 = 0x0;

// Register definitions for MM
// ###################################
// All Lock State Value
const unsigned int mm_all_lock_state_value = 0x0001EF00;
// Checkbit Error Generation
const unsigned int mm_checkbit_error_generation = 0x00012000;
// Combo event control
const unsigned int mm_combo_event_control = 0x00014404;
// Combo event inputs
const unsigned int mm_combo_event_inputs = 0x00014400;
// DMA BD0 2D X
const unsigned int mm_dma_bd0_2d_x = 0x0001D008;
// DMA BD0 2D Y
const unsigned int mm_dma_bd0_2d_y = 0x0001D00C;
// DMA BD0 Addr A
const unsigned int mm_dma_bd0_addr_a = 0x0001D000;
// DMA BD0 Addr B
const unsigned int mm_dma_bd0_addr_b = 0x0001D004;
// DMA BD0 Control
const unsigned int mm_dma_bd0_control = 0x0001D018;
// DMA BD0 Interleaved State
const unsigned int mm_dma_bd0_interleaved_state = 0x0001D014;
// DMA BD0 Packet
const unsigned int mm_dma_bd0_packet = 0x0001D010;
// DMA BD10 2D X
const unsigned int mm_dma_bd10_2d_x = 0x0001D148;
// DMA BD10 2D Y
const unsigned int mm_dma_bd10_2d_y = 0x0001D14C;
// DMA BD10 Addr A
const unsigned int mm_dma_bd10_addr_a = 0x0001D140;
// DMA BD10 Addr B
const unsigned int mm_dma_bd10_addr_b = 0x0001D144;
// DMA BD10 Control
const unsigned int mm_dma_bd10_control = 0x0001D158;
// DMA BD10 Interleaved State
const unsigned int mm_dma_bd10_interleaved_state = 0x0001D154;
// DMA BD10 Packet
const unsigned int mm_dma_bd10_packet = 0x0001D150;
// DMA BD11 2D X
const unsigned int mm_dma_bd11_2d_x = 0x0001D168;
// DMA BD11 2D Y
const unsigned int mm_dma_bd11_2d_y = 0x0001D16C;
// DMA BD11 Addr A
const unsigned int mm_dma_bd11_addr_a = 0x0001D160;
// DMA BD11 Addr B
const unsigned int mm_dma_bd11_addr_b = 0x0001D164;
// DMA BD11 Control
const unsigned int mm_dma_bd11_control = 0x0001D178;
// DMA BD11 Interleaved State
const unsigned int mm_dma_bd11_interleaved_state = 0x0001D174;
// DMA BD11 Packet
const unsigned int mm_dma_bd11_packet = 0x0001D170;
// DMA BD12 2D X
const unsigned int mm_dma_bd12_2d_x = 0x0001D188;
// DMA BD12 2D Y
const unsigned int mm_dma_bd12_2d_y = 0x0001D18C;
// DMA BD12 Addr A
const unsigned int mm_dma_bd12_addr_a = 0x0001D180;
// DMA BD12 Addr B
const unsigned int mm_dma_bd12_addr_b = 0x0001D184;
// DMA BD12 Control
const unsigned int mm_dma_bd12_control = 0x0001D198;
// DMA BD12 Interleaved State
const unsigned int mm_dma_bd12_interleaved_state = 0x0001D194;
// DMA BD12 Packet
const unsigned int mm_dma_bd12_packet = 0x0001D190;
// DMA BD13 2D X
const unsigned int mm_dma_bd13_2d_x = 0x0001D1A8;
// DMA BD13 2D Y
const unsigned int mm_dma_bd13_2d_y = 0x0001D1AC;
// DMA BD13 Addr A
const unsigned int mm_dma_bd13_addr_a = 0x0001D1A0;
// DMA BD13 Addr B
const unsigned int mm_dma_bd13_addr_b = 0x0001D1A4;
// DMA BD13 Control
const unsigned int mm_dma_bd13_control = 0x0001D1B8;
// DMA BD13 Interleaved State
const unsigned int mm_dma_bd13_interleaved_state = 0x0001D1B4;
// DMA BD13 Packet
const unsigned int mm_dma_bd13_packet = 0x0001D1B0;
// DMA BD14 2D X
const unsigned int mm_dma_bd14_2d_x = 0x0001D1C8;
// DMA BD14 2D Y
const unsigned int mm_dma_bd14_2d_y = 0x0001D1CC;
// DMA BD14 Addr A
const unsigned int mm_dma_bd14_addr_a = 0x0001D1C0;
// DMA BD14 Addr B
const unsigned int mm_dma_bd14_addr_b = 0x0001D1C4;
// DMA BD14 Control
const unsigned int mm_dma_bd14_control = 0x0001D1D8;
// DMA BD14 Interleaved State
const unsigned int mm_dma_bd14_interleaved_state = 0x0001D1D4;
// DMA BD14 Packet
const unsigned int mm_dma_bd14_packet = 0x0001D1D0;
// DMA BD15 2D X
const unsigned int mm_dma_bd15_2d_x = 0x0001D1E8;
// DMA BD15 2D Y
const unsigned int mm_dma_bd15_2d_y = 0x0001D1EC;
// DMA BD15 Addr A
const unsigned int mm_dma_bd15_addr_a = 0x0001D1E0;
// DMA BD15 Addr B
const unsigned int mm_dma_bd15_addr_b = 0x0001D1E4;
// DMA BD15 Control
const unsigned int mm_dma_bd15_control = 0x0001D1F8;
// DMA BD15 Interleaved State
const unsigned int mm_dma_bd15_interleaved_state = 0x0001D1F4;
// DMA BD15 Packet
const unsigned int mm_dma_bd15_packet = 0x0001D1F0;
// DMA BD1 2D X
const unsigned int mm_dma_bd1_2d_x = 0x0001D028;
// DMA BD1 2D Y
const unsigned int mm_dma_bd1_2d_y = 0x0001D02C;
// DMA BD1 Addr A
const unsigned int mm_dma_bd1_addr_a = 0x0001D020;
// DMA BD1 Addr B
const unsigned int mm_dma_bd1_addr_b = 0x0001D024;
// DMA BD1 Control
const unsigned int mm_dma_bd1_control = 0x0001D038;
// DMA BD1 Interleaved State
const unsigned int mm_dma_bd1_interleaved_state = 0x0001D034;
// DMA BD1 Packet
const unsigned int mm_dma_bd1_packet = 0x0001D030;
// DMA BD2 2D X
const unsigned int mm_dma_bd2_2d_x = 0x0001D048;
// DMA BD2 2D Y
const unsigned int mm_dma_bd2_2d_y = 0x0001D04C;
// DMA BD2 Addr A
const unsigned int mm_dma_bd2_addr_a = 0x0001D040;
// DMA BD2 Addr B
const unsigned int mm_dma_bd2_addr_b = 0x0001D044;
// DMA BD2 Control
const unsigned int mm_dma_bd2_control = 0x0001D058;
// DMA BD2 Interleaved State
const unsigned int mm_dma_bd2_interleaved_state = 0x0001D054;
// DMA BD2 Packet
const unsigned int mm_dma_bd2_packet = 0x0001D050;
// DMA BD3 2D X
const unsigned int mm_dma_bd3_2d_x = 0x0001D068;
// DMA BD3 2D Y
const unsigned int mm_dma_bd3_2d_y = 0x0001D06C;
// DMA BD3 Addr A
const unsigned int mm_dma_bd3_addr_a = 0x0001D060;
// DMA BD3 Addr B
const unsigned int mm_dma_bd3_addr_b = 0x0001D064;
// DMA BD3 Control
const unsigned int mm_dma_bd3_control = 0x0001D078;
// DMA BD3 Interleaved State
const unsigned int mm_dma_bd3_interleaved_state = 0x0001D074;
// DMA BD3 Packet
const unsigned int mm_dma_bd3_packet = 0x0001D070;
// DMA BD4 2D X
const unsigned int mm_dma_bd4_2d_x = 0x0001D088;
// DMA BD4 2D Y
const unsigned int mm_dma_bd4_2d_y = 0x0001D08C;
// DMA BD4 Addr A
const unsigned int mm_dma_bd4_addr_a = 0x0001D080;
// DMA BD4 Addr B
const unsigned int mm_dma_bd4_addr_b = 0x0001D084;
// DMA BD4 Control
const unsigned int mm_dma_bd4_control = 0x0001D098;
// DMA BD4 Interleaved State
const unsigned int mm_dma_bd4_interleaved_state = 0x0001D094;
// DMA BD4 Packet
const unsigned int mm_dma_bd4_packet = 0x0001D090;
// DMA BD5 2D X
const unsigned int mm_dma_bd5_2d_x = 0x0001D0A8;
// DMA BD5 2D Y
const unsigned int mm_dma_bd5_2d_y = 0x0001D0AC;
// DMA BD5 Addr A
const unsigned int mm_dma_bd5_addr_a = 0x0001D0A0;
// DMA BD5 Addr B
const unsigned int mm_dma_bd5_addr_b = 0x0001D0A4;
// DMA BD5 Control
const unsigned int mm_dma_bd5_control = 0x0001D0B8;
// DMA BD5 Interleaved State
const unsigned int mm_dma_bd5_interleaved_state = 0x0001D0B4;
// DMA BD5 Packet
const unsigned int mm_dma_bd5_packet = 0x0001D0B0;
// DMA BD6 2D X
const unsigned int mm_dma_bd6_2d_x = 0x0001D0C8;
// DMA BD6 2D Y
const unsigned int mm_dma_bd6_2d_y = 0x0001D0CC;
// DMA BD6 Addr A
const unsigned int mm_dma_bd6_addr_a = 0x0001D0C0;
// DMA BD6 Addr B
const unsigned int mm_dma_bd6_addr_b = 0x0001D0C4;
// DMA BD6 Control
const unsigned int mm_dma_bd6_control = 0x0001D0D8;
// DMA BD6 Interleaved State
const unsigned int mm_dma_bd6_interleaved_state = 0x0001D0D4;
// DMA BD6 Packet
const unsigned int mm_dma_bd6_packet = 0x0001D0D0;
// DMA BD7 2D X
const unsigned int mm_dma_bd7_2d_x = 0x0001D0E8;
// DMA BD7 2D Y
const unsigned int mm_dma_bd7_2d_y = 0x0001D0EC;
// DMA BD7 Addr A
const unsigned int mm_dma_bd7_addr_a = 0x0001D0E0;
// DMA BD7 Addr B
const unsigned int mm_dma_bd7_addr_b = 0x0001D0E4;
// DMA BD7 Control
const unsigned int mm_dma_bd7_control = 0x0001D0F8;
// DMA BD7 Interleaved State
const unsigned int mm_dma_bd7_interleaved_state = 0x0001D0F4;
// DMA BD7 Packet
const unsigned int mm_dma_bd7_packet = 0x0001D0F0;
// DMA BD8 2D X
const unsigned int mm_dma_bd8_2d_x = 0x0001D108;
// DMA BD8 2D Y
const unsigned int mm_dma_bd8_2d_y = 0x0001D10C;
// DMA BD8 Addr A
const unsigned int mm_dma_bd8_addr_a = 0x0001D100;
// DMA BD8 Addr B
const unsigned int mm_dma_bd8_addr_b = 0x0001D104;
// DMA BD8 Control
const unsigned int mm_dma_bd8_control = 0x0001D118;
// DMA BD8 Interleaved State
const unsigned int mm_dma_bd8_interleaved_state = 0x0001D114;
// DMA BD8 Packet
const unsigned int mm_dma_bd8_packet = 0x0001D110;
// DMA BD9 2D X
const unsigned int mm_dma_bd9_2d_x = 0x0001D128;
// DMA BD9 2D Y
const unsigned int mm_dma_bd9_2d_y = 0x0001D12C;
// DMA BD9 Addr A
const unsigned int mm_dma_bd9_addr_a = 0x0001D120;
// DMA BD9 Addr B
const unsigned int mm_dma_bd9_addr_b = 0x0001D124;
// DMA BD9 Control
const unsigned int mm_dma_bd9_control = 0x0001D138;
// DMA BD9 Interleaved State
const unsigned int mm_dma_bd9_interleaved_state = 0x0001D134;
// DMA BD9 Packet
const unsigned int mm_dma_bd9_packet = 0x0001D130;
// DMA FIFO Counter
const unsigned int mm_dma_fifo_counter = 0x0001DF20;
// DMA MM2S 0 Ctrl
const unsigned int mm_dma_mm2s_0_ctrl = 0x0001DE10;
// DMA MM2S 0 Start Queue
const unsigned int mm_dma_mm2s_0_start_queue = 0x0001DE14;
// DMA MM2S 1 Ctrl
const unsigned int mm_dma_mm2s_1_ctrl = 0x0001DE18;
// DMA MM2S 1 Start Queue
const unsigned int mm_dma_mm2s_1_start_queue = 0x0001DE1C;
// DMA MM2S Status
const unsigned int mm_dma_mm2s_status = 0x0001DF10;
// DMA S2MM 0 Ctrl
const unsigned int mm_dma_s2mm_0_ctrl = 0x0001DE00;
// DMA S2MM 0 Start Queue
const unsigned int mm_dma_s2mm_0_start_queue = 0x0001DE04;
// DMA S2MM 1 Ctrl
const unsigned int mm_dma_s2mm_1_ctrl = 0x0001DE08;
// DMA S2MM 1 Start Queue
const unsigned int mm_dma_s2mm_1_start_queue = 0x0001DE0C;
// DMA S2MM Status
const unsigned int mm_dma_s2mm_status = 0x0001DF00;
// ECC Failing Address
const unsigned int mm_ecc_failing_address = 0x00012120;
// ECC Scrubbing Event
const unsigned int mm_ecc_scrubbing_event = 0x00012110;
// Event Broadcast0
const unsigned int mm_event_broadcast0 = 0x00014010;
// Event Broadcast1
const unsigned int mm_event_broadcast1 = 0x00014014;
// Event Broadcast10
const unsigned int mm_event_broadcast10 = 0x00014038;
// Event Broadcast11
const unsigned int mm_event_broadcast11 = 0x0001403C;
// Event Broadcast12
const unsigned int mm_event_broadcast12 = 0x00014040;
// Event Broadcast13
const unsigned int mm_event_broadcast13 = 0x00014044;
// Event Broadcast14
const unsigned int mm_event_broadcast14 = 0x00014048;
// Event Broadcast15
const unsigned int mm_event_broadcast15 = 0x0001404C;
// Event Broadcast2
const unsigned int mm_event_broadcast2 = 0x00014018;
// Event Broadcast3
const unsigned int mm_event_broadcast3 = 0x0001401C;
// Event Broadcast4
const unsigned int mm_event_broadcast4 = 0x00014020;
// Event Broadcast5
const unsigned int mm_event_broadcast5 = 0x00014024;
// Event Broadcast6
const unsigned int mm_event_broadcast6 = 0x00014028;
// Event Broadcast7
const unsigned int mm_event_broadcast7 = 0x0001402C;
// Event Broadcast8
const unsigned int mm_event_broadcast8 = 0x00014030;
// Event Broadcast9
const unsigned int mm_event_broadcast9 = 0x00014034;
// Event Broadcast Block East Clr
const unsigned int mm_event_broadcast_block_east_clr = 0x00014084;
// Event Broadcast Block East Set
const unsigned int mm_event_broadcast_block_east_set = 0x00014080;
// Event Broadcast Block East Value
const unsigned int mm_event_broadcast_block_east_value = 0x00014088;
// Event Broadcast Block North Clr
const unsigned int mm_event_broadcast_block_north_clr = 0x00014074;
// Event Broadcast Block North Set
const unsigned int mm_event_broadcast_block_north_set = 0x00014070;
// Event Broadcast Block North Value
const unsigned int mm_event_broadcast_block_north_value = 0x00014078;
// Event Broadcast Block South Clr
const unsigned int mm_event_broadcast_block_south_clr = 0x00014054;
// Event Broadcast Block South Set
const unsigned int mm_event_broadcast_block_south_set = 0x00014050;
// Event Broadcast Block South Value
const unsigned int mm_event_broadcast_block_south_value = 0x00014058;
// Event Broadcast Block West Clr
const unsigned int mm_event_broadcast_block_west_clr = 0x00014064;
// Event Broadcast Block West Set
const unsigned int mm_event_broadcast_block_west_set = 0x00014060;
// Event Broadcast Block West Value
const unsigned int mm_event_broadcast_block_west_value = 0x00014068;
// Event Generate
const unsigned int mm_event_generate = 0x00014008;
// Event Group 0 Enable
const unsigned int mm_event_group_0_enable = 0x00014500;
// Event Group Broadcast Enable
const unsigned int mm_event_group_broadcast_enable = 0x00014518;
// Event Group DMA Enable
const unsigned int mm_event_group_dma_enable = 0x00014508;
// Event Group Error Enable
const unsigned int mm_event_group_error_enable = 0x00014514;
// Event Group Lock Enable
const unsigned int mm_event_group_lock_enable = 0x0001450C;
// Event Group Memory Conflict Enable
const unsigned int mm_event_group_memory_conflict_enable = 0x00014510;
// Event Group User Event Enable
const unsigned int mm_event_group_user_event_enable = 0x0001451C;
// Event Group Watchpoint Enable
const unsigned int mm_event_group_watchpoint_enable = 0x00014504;
// Event Status0
const unsigned int mm_event_status0 = 0x00014200;
// Event Status1
const unsigned int mm_event_status1 = 0x00014204;
// Event Status2
const unsigned int mm_event_status2 = 0x00014208;
// Event Status3
const unsigned int mm_event_status3 = 0x0001420C;
// Lock0 Acquire NV
const unsigned int mm_lock0_acquire_nv = 0x0001E040;
// Lock0 Acquire V0
const unsigned int mm_lock0_acquire_v0 = 0x0001E060;
// Lock0 Acquire V1
const unsigned int mm_lock0_acquire_v1 = 0x0001E070;
// Lock0 Release NV
const unsigned int mm_lock0_release_nv = 0x0001E000;
// Lock0 Release V0
const unsigned int mm_lock0_release_v0 = 0x0001E020;
// Lock0 Release V1
const unsigned int mm_lock0_release_v1 = 0x0001E030;
// Lock10 Acquire NV
const unsigned int mm_lock10_acquire_nv = 0x0001E540;
// Lock10 Acquire V0
const unsigned int mm_lock10_acquire_v0 = 0x0001E560;
// Lock10 Acquire V1
const unsigned int mm_lock10_acquire_v1 = 0x0001E570;
// Lock10 Release NV
const unsigned int mm_lock10_release_nv = 0x0001E500;
// Lock10 Release V0
const unsigned int mm_lock10_release_v0 = 0x0001E520;
// Lock10 Release V1
const unsigned int mm_lock10_release_v1 = 0x0001E530;
// Lock11 Acquire NV
const unsigned int mm_lock11_acquire_nv = 0x0001E5C0;
// Lock11 Acquire V0
const unsigned int mm_lock11_acquire_v0 = 0x0001E5E0;
// Lock11 Acquire V1
const unsigned int mm_lock11_acquire_v1 = 0x0001E5F0;
// Lock11 Release NV
const unsigned int mm_lock11_release_nv = 0x0001E580;
// Lock11 Release V0
const unsigned int mm_lock11_release_v0 = 0x0001E5A0;
// Lock11 Release V1
const unsigned int mm_lock11_release_v1 = 0x0001E5B0;
// Lock12 Acquire NV
const unsigned int mm_lock12_acquire_nv = 0x0001E640;
// Lock12 Acquire V0
const unsigned int mm_lock12_acquire_v0 = 0x0001E660;
// Lock12 Acquire V1
const unsigned int mm_lock12_acquire_v1 = 0x0001E670;
// Lock12 Release NV
const unsigned int mm_lock12_release_nv = 0x0001E600;
// Lock12 Release V0
const unsigned int mm_lock12_release_v0 = 0x0001E620;
// Lock12 Release V1
const unsigned int mm_lock12_release_v1 = 0x0001E630;
// Lock13 Acquire NV
const unsigned int mm_lock13_acquire_nv = 0x0001E6C0;
// Lock13 Acquire V0
const unsigned int mm_lock13_acquire_v0 = 0x0001E6E0;
// Lock13 Acquire V1
const unsigned int mm_lock13_acquire_v1 = 0x0001E6F0;
// Lock13 Release NV
const unsigned int mm_lock13_release_nv = 0x0001E680;
// Lock13 Release V0
const unsigned int mm_lock13_release_v0 = 0x0001E6A0;
// Lock13 Release V1
const unsigned int mm_lock13_release_v1 = 0x0001E6B0;
// Lock14 Acquire NV
const unsigned int mm_lock14_acquire_nv = 0x0001E740;
// Lock14 Acquire V0
const unsigned int mm_lock14_acquire_v0 = 0x0001E760;
// Lock14 Acquire V1
const unsigned int mm_lock14_acquire_v1 = 0x0001E770;
// Lock14 Release NV
const unsigned int mm_lock14_release_nv = 0x0001E700;
// Lock14 Release V0
const unsigned int mm_lock14_release_v0 = 0x0001E720;
// Lock14 Release V1
const unsigned int mm_lock14_release_v1 = 0x0001E730;
// Lock15 Acquire NV
const unsigned int mm_lock15_acquire_nv = 0x0001E7C0;
// Lock15 Acquire V0
const unsigned int mm_lock15_acquire_v0 = 0x0001E7E0;
// Lock15 Acquire V1
const unsigned int mm_lock15_acquire_v1 = 0x0001E7F0;
// Lock15 Release NV
const unsigned int mm_lock15_release_nv = 0x0001E780;
// Lock15 Release V0
const unsigned int mm_lock15_release_v0 = 0x0001E7A0;
// Lock15 Release V1
const unsigned int mm_lock15_release_v1 = 0x0001E7B0;
// Lock1 Acquire NV
const unsigned int mm_lock1_acquire_nv = 0x0001E0C0;
// Lock1 Acquire V0
const unsigned int mm_lock1_acquire_v0 = 0x0001E0E0;
// Lock1 Acquire V1
const unsigned int mm_lock1_acquire_v1 = 0x0001E0F0;
// Lock1 Release NV
const unsigned int mm_lock1_release_nv = 0x0001E080;
// Lock1 Release V0
const unsigned int mm_lock1_release_v0 = 0x0001E0A0;
// Lock1 Release V1
const unsigned int mm_lock1_release_v1 = 0x0001E0B0;
// Lock2 Acquire NV
const unsigned int mm_lock2_acquire_nv = 0x0001E140;
// Lock2 Acquire V0
const unsigned int mm_lock2_acquire_v0 = 0x0001E160;
// Lock2 Acquire V1
const unsigned int mm_lock2_acquire_v1 = 0x0001E170;
// Lock2 Release NV
const unsigned int mm_lock2_release_nv = 0x0001E100;
// Lock2 Release V0
const unsigned int mm_lock2_release_v0 = 0x0001E120;
// Lock2 Release V1
const unsigned int mm_lock2_release_v1 = 0x0001E130;
// Lock3 Acquire NV
const unsigned int mm_lock3_acquire_nv = 0x0001E1C0;
// Lock3 Acquire V0
const unsigned int mm_lock3_acquire_v0 = 0x0001E1E0;
// Lock3 Acquire V1
const unsigned int mm_lock3_acquire_v1 = 0x0001E1F0;
// Lock3 Release NV
const unsigned int mm_lock3_release_nv = 0x0001E180;
// Lock3 Release V0
const unsigned int mm_lock3_release_v0 = 0x0001E1A0;
// Lock3 Release V1
const unsigned int mm_lock3_release_v1 = 0x0001E1B0;
// Lock4 Acquire NV
const unsigned int mm_lock4_acquire_nv = 0x0001E240;
// Lock4 Acquire V0
const unsigned int mm_lock4_acquire_v0 = 0x0001E260;
// Lock4 Acquire V1
const unsigned int mm_lock4_acquire_v1 = 0x0001E270;
// Lock4 Release NV
const unsigned int mm_lock4_release_nv = 0x0001E200;
// Lock4 Release V0
const unsigned int mm_lock4_release_v0 = 0x0001E220;
// Lock4 Release V1
const unsigned int mm_lock4_release_v1 = 0x0001E230;
// Lock5 Acquire NV
const unsigned int mm_lock5_acquire_nv = 0x0001E2C0;
// Lock5 Acquire V0
const unsigned int mm_lock5_acquire_v0 = 0x0001E2E0;
// Lock5 Acquire V1
const unsigned int mm_lock5_acquire_v1 = 0x0001E2F0;
// Lock5 Release NV
const unsigned int mm_lock5_release_nv = 0x0001E280;
// Lock5 Release V0
const unsigned int mm_lock5_release_v0 = 0x0001E2A0;
// Lock5 Release V1
const unsigned int mm_lock5_release_v1 = 0x0001E2B0;
// Lock6 Acquire NV
const unsigned int mm_lock6_acquire_nv = 0x0001E340;
// Lock6 Acquire V0
const unsigned int mm_lock6_acquire_v0 = 0x0001E360;
// Lock6 Acquire V1
const unsigned int mm_lock6_acquire_v1 = 0x0001E370;
// Lock6 Release NV
const unsigned int mm_lock6_release_nv = 0x0001E300;
// Lock6 Release V0
const unsigned int mm_lock6_release_v0 = 0x0001E320;
// Lock6 Release V1
const unsigned int mm_lock6_release_v1 = 0x0001E330;
// Lock7 Acquire NV
const unsigned int mm_lock7_acquire_nv = 0x0001E3C0;
// Lock7 Acquire V0
const unsigned int mm_lock7_acquire_v0 = 0x0001E3E0;
// Lock7 Acquire V1
const unsigned int mm_lock7_acquire_v1 = 0x0001E3F0;
// Lock7 Release NV
const unsigned int mm_lock7_release_nv = 0x0001E380;
// Lock7 Release V0
const unsigned int mm_lock7_release_v0 = 0x0001E3A0;
// Lock7 Release V1
const unsigned int mm_lock7_release_v1 = 0x0001E3B0;
// Lock8 Acquire NV
const unsigned int mm_lock8_acquire_nv = 0x0001E440;
// Lock8 Acquire V0
const unsigned int mm_lock8_acquire_v0 = 0x0001E460;
// Lock8 Acquire V1
const unsigned int mm_lock8_acquire_v1 = 0x0001E470;
// Lock8 Release NV
const unsigned int mm_lock8_release_nv = 0x0001E400;
// Lock8 Release V0
const unsigned int mm_lock8_release_v0 = 0x0001E420;
// Lock8 Release V1
const unsigned int mm_lock8_release_v1 = 0x0001E430;
// Lock9 Acquire NV
const unsigned int mm_lock9_acquire_nv = 0x0001E4C0;
// Lock9 Acquire V0
const unsigned int mm_lock9_acquire_v0 = 0x0001E4E0;
// Lock9 Acquire V1
const unsigned int mm_lock9_acquire_v1 = 0x0001E4F0;
// Lock9 Release NV
const unsigned int mm_lock9_release_nv = 0x0001E480;
// Lock9 Release V0
const unsigned int mm_lock9_release_v0 = 0x0001E4A0;
// Lock9 Release V1
const unsigned int mm_lock9_release_v1 = 0x0001E4B0;
// Lock Event Value Control 0
const unsigned int mm_lock_event_value_control_0 = 0x0001EF20;
// Lock Event Value Control 1
const unsigned int mm_lock_event_value_control_1 = 0x0001EF24;
// Parity Failing Address
const unsigned int mm_parity_failing_address = 0x00012124;
// Performance Control0
const unsigned int mm_performance_control0 = 0x00011000;
// Performance Control1
const unsigned int mm_performance_control1 = 0x00011008;
// Performance Counter0
const unsigned int mm_performance_counter0 = 0x00011020;
// Performance Counter0 Event Value
const unsigned int mm_performance_counter0_event_value = 0x00011080;
// Performance Counter1
const unsigned int mm_performance_counter1 = 0x00011024;
// Performance Counter1 Event Value
const unsigned int mm_performance_counter1_event_value = 0x00011084;
// Reserved0
const unsigned int mm_reserved0 = 0x00014210;
// Reserved1
const unsigned int mm_reserved1 = 0x00014214;
// Reserved2
const unsigned int mm_reserved2 = 0x00014218;
// Reserved3
const unsigned int mm_reserved3 = 0x0001421C;
// Reset Control
const unsigned int mm_reset_control = 0x00013000;
// Spare Reg
const unsigned int mm_spare_reg = 0x00016000;
// Timer Control
const unsigned int mm_timer_control = 0x00014000;
// Timer High
const unsigned int mm_timer_high = 0x000140FC;
// Timer Low
const unsigned int mm_timer_low = 0x000140F8;
// Timer Trig Event High Value
const unsigned int mm_timer_trig_event_high_value = 0x000140F4;
// Timer Trig Event Low Value
const unsigned int mm_timer_trig_event_low_value = 0x000140F0;
// Trace Control0
const unsigned int mm_trace_control0 = 0x000140D0;
// Trace Control1
const unsigned int mm_trace_control1 = 0x000140D4;
// Trace Event0
const unsigned int mm_trace_event0 = 0x000140E0;
// Trace Event1
const unsigned int mm_trace_event1 = 0x000140E4;
// Trace Status
const unsigned int mm_trace_status = 0x000140D8;
// WatchPoint0
const unsigned int mm_watchpoint0 = 0x00014100;
// WatchPoint1
const unsigned int mm_watchpoint1 = 0x00014104;

// Register definitions for SHIM
// ###################################
// All Lock State Value
const unsigned int shim_all_lock_state_value = 0x00014F00;
// BISR cache ctrl
const unsigned int shim_bisr_cache_ctrl = 0x00036000;
// BISR cache data0
const unsigned int shim_bisr_cache_data0 = 0x00036010;
// BISR cache data1
const unsigned int shim_bisr_cache_data1 = 0x00036014;
// BISR cache data2
const unsigned int shim_bisr_cache_data2 = 0x00036018;
// BISR cache data3
const unsigned int shim_bisr_cache_data3 = 0x0003601C;
// BISR cache status
const unsigned int shim_bisr_cache_status = 0x00036008;
// BISR test data0
const unsigned int shim_bisr_test_data0 = 0x00036020;
// BISR test data1
const unsigned int shim_bisr_test_data1 = 0x00036024;
// BISR test data2
const unsigned int shim_bisr_test_data2 = 0x00036028;
// BISR test data3
const unsigned int shim_bisr_test_data3 = 0x0003602C;
// Combo event control
const unsigned int shim_combo_event_control = 0x00034404;
// Combo event inputs
const unsigned int shim_combo_event_inputs = 0x00034400;
// Control Packet Handler Status
const unsigned int shim_control_packet_handler_status = 0x00036034;
// CSSD Trigger
const unsigned int shim_cssd_trigger = 0x00036044;
// Demux Config
const unsigned int shim_demux_config = 0x0001F004;
// DMA BD0 Addr Low
const unsigned int shim_dma_bd0_addr_low = 0x0001D000;
// DMA BD0 AXI Config
const unsigned int shim_dma_bd0_axi_config = 0x0001D00C;
// DMA BD0 Buffer Length
const unsigned int shim_dma_bd0_buffer_length = 0x0001D004;
// DMA BD0 Control
const unsigned int shim_dma_bd0_control = 0x0001D008;
// DMA BD0 Packet
const unsigned int shim_dma_bd0_packet = 0x0001D010;
// DMA BD10 Addr Low
const unsigned int shim_dma_bd10_addr_low = 0x0001D0C8;
// DMA BD10 AXI Config
const unsigned int shim_dma_bd10_axi_config = 0x0001D0D4;
// DMA BD10 Buffer Control
const unsigned int shim_dma_bd10_buffer_control = 0x0001D0D0;
// DMA BD10 Buffer Length
const unsigned int shim_dma_bd10_buffer_length = 0x0001D0CC;
// DMA BD10 Packet
const unsigned int shim_dma_bd10_packet = 0x0001D0D8;
// DMA BD11 Addr Low
const unsigned int shim_dma_bd11_addr_low = 0x0001D0DC;
// DMA BD11 AXI Config
const unsigned int shim_dma_bd11_axi_config = 0x0001D0E8;
// DMA BD11 Buffer Control
const unsigned int shim_dma_bd11_buffer_control = 0x0001D0E4;
// DMA BD11 Buffer Length
const unsigned int shim_dma_bd11_buffer_length = 0x0001D0E0;
// DMA BD11 Packet
const unsigned int shim_dma_bd11_packet = 0x0001D0EC;
// DMA BD12 Addr Low
const unsigned int shim_dma_bd12_addr_low = 0x0001D0F0;
// DMA BD12 AXI Config
const unsigned int shim_dma_bd12_axi_config = 0x0001D0FC;
// DMA BD12 Buffer Control
const unsigned int shim_dma_bd12_buffer_control = 0x0001D0F8;
// DMA BD12 Buffer Length
const unsigned int shim_dma_bd12_buffer_length = 0x0001D0F4;
// DMA BD12 Packet
const unsigned int shim_dma_bd12_packet = 0x0001D100;
// DMA BD13 Addr Low
const unsigned int shim_dma_bd13_addr_low = 0x0001D104;
// DMA BD13 AXI Config
const unsigned int shim_dma_bd13_axi_config = 0x0001D110;
// DMA BD13 Buffer Control
const unsigned int shim_dma_bd13_buffer_control = 0x0001D10C;
// DMA BD13 Buffer Length
const unsigned int shim_dma_bd13_buffer_length = 0x0001D108;
// DMA BD13 Packet
const unsigned int shim_dma_bd13_packet = 0x0001D114;
// DMA BD14 Addr Low
const unsigned int shim_dma_bd14_addr_low = 0x0001D118;
// DMA BD14 AXI Config
const unsigned int shim_dma_bd14_axi_config = 0x0001D124;
// DMA BD14 Buffer Control
const unsigned int shim_dma_bd14_buffer_control = 0x0001D120;
// DMA BD14 Buffer Length
const unsigned int shim_dma_bd14_buffer_length = 0x0001D11C;
// DMA BD14 Packet
const unsigned int shim_dma_bd14_packet = 0x0001D128;
// DMA BD15 Addr Low
const unsigned int shim_dma_bd15_addr_low = 0x0001D12C;
// DMA BD15 AXI Config
const unsigned int shim_dma_bd15_axi_config = 0x0001D138;
// DMA BD15 Buffer Control
const unsigned int shim_dma_bd15_buffer_control = 0x0001D134;
// DMA BD15 Buffer Length
const unsigned int shim_dma_bd15_buffer_length = 0x0001D130;
// DMA BD15 Packet
const unsigned int shim_dma_bd15_packet = 0x0001D13C;
// DMA BD1 Addr Low
const unsigned int shim_dma_bd1_addr_low = 0x0001D014;
// DMA BD1 AXI Config
const unsigned int shim_dma_bd1_axi_config = 0x0001D020;
// DMA BD1 Buffer Control
const unsigned int shim_dma_bd1_buffer_control = 0x0001D01C;
// DMA BD1 Buffer Length
const unsigned int shim_dma_bd1_buffer_length = 0x0001D018;
// DMA BD1 Packet
const unsigned int shim_dma_bd1_packet = 0x0001D024;
// DMA BD2 Addr Low
const unsigned int shim_dma_bd2_addr_low = 0x0001D028;
// DMA BD2 AXI Config
const unsigned int shim_dma_bd2_axi_config = 0x0001D034;
// DMA BD2 Buffer Control
const unsigned int shim_dma_bd2_buffer_control = 0x0001D030;
// DMA BD2 Buffer Length
const unsigned int shim_dma_bd2_buffer_length = 0x0001D02C;
// DMA BD2 Packet
const unsigned int shim_dma_bd2_packet = 0x0001D038;
// DMA BD3 Addr Low
const unsigned int shim_dma_bd3_addr_low = 0x0001D03C;
// DMA BD3 AXI Config
const unsigned int shim_dma_bd3_axi_config = 0x0001D048;
// DMA BD3 Buffer Control
const unsigned int shim_dma_bd3_buffer_control = 0x0001D044;
// DMA BD3 Buffer Length
const unsigned int shim_dma_bd3_buffer_length = 0x0001D040;
// DMA BD3 Packet
const unsigned int shim_dma_bd3_packet = 0x0001D04C;
// DMA BD4 Addr Low
const unsigned int shim_dma_bd4_addr_low = 0x0001D050;
// DMA BD4 AXI Config
const unsigned int shim_dma_bd4_axi_config = 0x0001D05C;
// DMA BD4 Buffer Control
const unsigned int shim_dma_bd4_buffer_control = 0x0001D058;
// DMA BD4 Buffer Length
const unsigned int shim_dma_bd4_buffer_length = 0x0001D054;
// DMA BD4 Packet
const unsigned int shim_dma_bd4_packet = 0x0001D060;
// DMA BD5 Addr Low
const unsigned int shim_dma_bd5_addr_low = 0x0001D064;
// DMA BD5 AXI Config
const unsigned int shim_dma_bd5_axi_config = 0x0001D070;
// DMA BD5 Buffer Control
const unsigned int shim_dma_bd5_buffer_control = 0x0001D06C;
// DMA BD5 Buffer Length
const unsigned int shim_dma_bd5_buffer_length = 0x0001D068;
// DMA BD5 Packet
const unsigned int shim_dma_bd5_packet = 0x0001D074;
// DMA BD6 Addr Low
const unsigned int shim_dma_bd6_addr_low = 0x0001D078;
// DMA BD6 AXI Config
const unsigned int shim_dma_bd6_axi_config = 0x0001D084;
// DMA BD6 Buffer Control
const unsigned int shim_dma_bd6_buffer_control = 0x0001D080;
// DMA BD6 Buffer Length
const unsigned int shim_dma_bd6_buffer_length = 0x0001D07C;
// DMA BD6 Packet
const unsigned int shim_dma_bd6_packet = 0x0001D088;
// DMA BD7 Addr Low
const unsigned int shim_dma_bd7_addr_low = 0x0001D08C;
// DMA BD7 AXI Config
const unsigned int shim_dma_bd7_axi_config = 0x0001D098;
// DMA BD7 Buffer Control
const unsigned int shim_dma_bd7_buffer_control = 0x0001D094;
// DMA BD7 Buffer Length
const unsigned int shim_dma_bd7_buffer_length = 0x0001D090;
// DMA BD7 Packet
const unsigned int shim_dma_bd7_packet = 0x0001D09C;
// DMA BD8 Addr Low
const unsigned int shim_dma_bd8_addr_low = 0x0001D0A0;
// DMA BD8 AXI Config
const unsigned int shim_dma_bd8_axi_config = 0x0001D0AC;
// DMA BD8 Buffer Control
const unsigned int shim_dma_bd8_buffer_control = 0x0001D0A8;
// DMA BD8 Buffer Length
const unsigned int shim_dma_bd8_buffer_length = 0x0001D0A4;
// DMA BD8 Packet
const unsigned int shim_dma_bd8_packet = 0x0001D0B0;
// DMA BD9 Addr Low
const unsigned int shim_dma_bd9_addr_low = 0x0001D0B4;
// DMA BD9 AXI Config
const unsigned int shim_dma_bd9_axi_config = 0x0001D0C0;
// DMA BD9 Buffer Control
const unsigned int shim_dma_bd9_buffer_control = 0x0001D0BC;
// DMA BD9 Buffer Length
const unsigned int shim_dma_bd9_buffer_length = 0x0001D0B8;
// DMA BD9 Packet
const unsigned int shim_dma_bd9_packet = 0x0001D0C4;
// Step size between DMA BD register groups
const unsigned int shim_dma_bd_step_size = 0x14;
// DMA MM2S 0 Ctrl
const unsigned int shim_dma_mm2s_0_ctrl = 0x0001D150;
// DMA MM2S 0 Start Queue
const unsigned int shim_dma_mm2s_0_start_queue = 0x0001D154;
// DMA MM2S 1 Ctrl
const unsigned int shim_dma_mm2s_1_ctrl = 0x0001D158;
// DMA MM2S 1 Start Queue
const unsigned int shim_dma_mm2s_1_start_queue = 0x0001D15C;
// DMA MM2S Status
const unsigned int shim_dma_mm2s_status = 0x0001D164;
// DMA S2MM 0 Ctrl
const unsigned int shim_dma_s2mm_0_ctrl = 0x0001D140;
// DMA S2MM 0 Start Queue
const unsigned int shim_dma_s2mm_0_start_queue = 0x0001D144;
// DMA S2MM 1 Ctrl
const unsigned int shim_dma_s2mm_1_ctrl = 0x0001D148;
// DMA S2MM 1 Start Queue
const unsigned int shim_dma_s2mm_1_start_queue = 0x0001D14C;
// DMA S2MM Status
const unsigned int shim_dma_s2mm_status = 0x0001D160;
// Step size between DMA S2MM register groups
const unsigned int shim_dma_s2mm_step_size = 0x8;
// Control of which Internal Event to Broadcast0
const unsigned int shim_event_broadcast_a_0 = 0x00034010;
// Control of which Internal Event to Broadcast1
const unsigned int shim_event_broadcast_a_1 = 0x00034014;
// Control of which Internal Event to Broadcast10
const unsigned int shim_event_broadcast_a_10 = 0x00034038;
// Control of which Internal Event to Broadcast11
const unsigned int shim_event_broadcast_a_11 = 0x0003403C;
// Control of which Internal Event to Broadcast12
const unsigned int shim_event_broadcast_a_12 = 0x00034040;
// Control of which Internal Event to Broadcast13
const unsigned int shim_event_broadcast_a_13 = 0x00034044;
// Control of which Internal Event to Broadcast14
const unsigned int shim_event_broadcast_a_14 = 0x00034048;
// Control of which Internal Event to Broadcast15
const unsigned int shim_event_broadcast_a_15 = 0x0003404C;
// Control of which Internal Event to Broadcast2
const unsigned int shim_event_broadcast_a_2 = 0x00034018;
// Control of which Internal Event to Broadcast3
const unsigned int shim_event_broadcast_a_3 = 0x0003401C;
// Control of which Internal Event to Broadcast4
const unsigned int shim_event_broadcast_a_4 = 0x00034020;
// Control of which Internal Event to Broadcast5
const unsigned int shim_event_broadcast_a_5 = 0x00034024;
// Control of which Internal Event to Broadcast6
const unsigned int shim_event_broadcast_a_6 = 0x00034028;
// Control of which Internal Event to Broadcast7
const unsigned int shim_event_broadcast_a_7 = 0x0003402C;
// Control of which Internal Event to Broadcast8
const unsigned int shim_event_broadcast_a_8 = 0x00034030;
// Control of which Internal Event to Broadcast9
const unsigned int shim_event_broadcast_a_9 = 0x00034034;
// Event Broadcast A Block East Clr
const unsigned int shim_event_broadcast_a_block_east_clr = 0x00034084;
// Event Broadcast A Block East Set
const unsigned int shim_event_broadcast_a_block_east_set = 0x00034080;
// Event Broadcast A Block East Value
const unsigned int shim_event_broadcast_a_block_east_value = 0x00034088;
// Event Broadcast A Block North Clr
const unsigned int shim_event_broadcast_a_block_north_clr = 0x00034074;
// Event Broadcast A Block North Set
const unsigned int shim_event_broadcast_a_block_north_set = 0x00034070;
// Event Broadcast A Block North Value
const unsigned int shim_event_broadcast_a_block_north_value = 0x00034078;
// Event Broadcast A Block South Clr
const unsigned int shim_event_broadcast_a_block_south_clr = 0x00034054;
// Event Broadcast A Block South Set
const unsigned int shim_event_broadcast_a_block_south_set = 0x00034050;
// Event Broadcast A Block South Value
const unsigned int shim_event_broadcast_a_block_south_value = 0x00034058;
// Event Broadcast A Block West Clr
const unsigned int shim_event_broadcast_a_block_west_clr = 0x00034064;
// Event Broadcast A Block West Set
const unsigned int shim_event_broadcast_a_block_west_set = 0x00034060;
// Event Broadcast A Block West Value
const unsigned int shim_event_broadcast_a_block_west_value = 0x00034068;
// Event Broadcast B Block East Clr
const unsigned int shim_event_broadcast_b_block_east_clr = 0x000340C4;
// Event Broadcast B Block East Set
const unsigned int shim_event_broadcast_b_block_east_set = 0x000340C0;
// Event Broadcast B Block East Value
const unsigned int shim_event_broadcast_b_block_east_value = 0x000340C8;
// Event Broadcast B Block North Clr
const unsigned int shim_event_broadcast_b_block_north_clr = 0x000340B4;
// Event Broadcast B Block North Set
const unsigned int shim_event_broadcast_b_block_north_set = 0x000340B0;
// Event Broadcast B Block North Value
const unsigned int shim_event_broadcast_b_block_north_value = 0x000340B8;
// Event Broadcast B Block South Clr
const unsigned int shim_event_broadcast_b_block_south_clr = 0x00034094;
// Event Broadcast B Block South Set
const unsigned int shim_event_broadcast_b_block_south_set = 0x00034090;
// Event Broadcast B Block South Value
const unsigned int shim_event_broadcast_b_block_south_value = 0x00034098;
// Event Broadcast B Block West Clr
const unsigned int shim_event_broadcast_b_block_west_clr = 0x000340A4;
// Event Broadcast B Block West Set
const unsigned int shim_event_broadcast_b_block_west_set = 0x000340A0;
// Event Broadcast B Block West Value
const unsigned int shim_event_broadcast_b_block_west_value = 0x000340A8;
// Event Generate
const unsigned int shim_event_generate = 0x00034008;
// Event Group 0 Enable
const unsigned int shim_event_group_0_enable = 0x00034500;
// Event Group Broadcast A Enable
const unsigned int shim_event_group_broadcast_a_enable = 0x00034514;
// Event enable for DMA Group
const unsigned int shim_event_group_dma_enable = 0x00034504;
// Event Group Errors Enable
const unsigned int shim_event_group_errors_enable = 0x0003450C;
// Event Group Lock Enable
const unsigned int shim_event_group_lock_enable = 0x00034508;
// Event Group Stream Switch Enable
const unsigned int shim_event_group_stream_switch_enable = 0x00034510;
// Event Group User Enable
const unsigned int shim_event_group_user_enable = 0x00034518;
// Event Status0
const unsigned int shim_event_status0 = 0x00034200;
// Event Status1
const unsigned int shim_event_status1 = 0x00034204;
// Event Status2
const unsigned int shim_event_status2 = 0x00034208;
// Event Status3
const unsigned int shim_event_status3 = 0x0003420C;
// Interrupt controller 1st level block north in A clear
const unsigned int shim_interrupt_controller_1st_level_block_north_in_a_clear = 0x0003501C;
// Interrupt controller 1st level block north in A set
const unsigned int shim_interrupt_controller_1st_level_block_north_in_a_set = 0x00035018;
// Interrupt controller 1st level block north in A value
const unsigned int shim_interrupt_controller_1st_level_block_north_in_a_value = 0x00035020;
// Interrupt controller 1st level block north in B clear
const unsigned int shim_interrupt_controller_1st_level_block_north_in_b_clear = 0x0003504C;
// Interrupt controller 1st level block north in B set
const unsigned int shim_interrupt_controller_1st_level_block_north_in_b_set = 0x00035048;
// Interrupt controller 1st level block north in B value
const unsigned int shim_interrupt_controller_1st_level_block_north_in_b_value = 0x00035050;
// Interrupt controller 1st level disable A
const unsigned int shim_interrupt_controller_1st_level_disable_a = 0x00035008;
// Interrupt controller 1st level disable B
const unsigned int shim_interrupt_controller_1st_level_disable_b = 0x00035038;
// Interrupt controller 1st level enable A
const unsigned int shim_interrupt_controller_1st_level_enable_a = 0x00035004;
// Interrupt controller 1st level enable B
const unsigned int shim_interrupt_controller_1st_level_enable_b = 0x00035034;
// Interrupt controller 1st level irq event A
const unsigned int shim_interrupt_controller_1st_level_irq_event_a = 0x00035014;
// Interrupt controller 1st level irq event B
const unsigned int shim_interrupt_controller_1st_level_irq_event_b = 0x00035044;
// Interrupt controller 1st level irq no A
const unsigned int shim_interrupt_controller_1st_level_irq_no_a = 0x00035010;
// Interrupt controller 1st level irq no B
const unsigned int shim_interrupt_controller_1st_level_irq_no_b = 0x00035040;
// Interrupt controller 1st level mask A
const unsigned int shim_interrupt_controller_1st_level_mask_a = 0x00035000;
// Interrupt controller 1st level mask B
const unsigned int shim_interrupt_controller_1st_level_mask_b = 0x00035030;
// Interrupt controller 1st level status A
const unsigned int shim_interrupt_controller_1st_level_status_a = 0x0003500C;
// Interrupt controller 1st level status B
const unsigned int shim_interrupt_controller_1st_level_status_b = 0x0003503C;
// Interrupt controller 2nd level disable
const unsigned int shim_interrupt_controller_2nd_level_disable = 0x00015008;
// Interrupt controller 2nd level enable
const unsigned int shim_interrupt_controller_2nd_level_enable = 0x00015004;
// Interrupt controller 2nd level interrupt
const unsigned int shim_interrupt_controller_2nd_level_interrupt = 0x00015010;
// Interrupt controller 2nd level mask
const unsigned int shim_interrupt_controller_2nd_level_mask = 0x00015000;
// Interrupt controller 2nd level status
const unsigned int shim_interrupt_controller_2nd_level_status = 0x0001500C;
// Lock0 Acquire NV
const unsigned int shim_lock0_acquire_nv = 0x00014040;
// Lock0 Acquire V0
const unsigned int shim_lock0_acquire_v0 = 0x00014060;
// Lock0 Acquire V1
const unsigned int shim_lock0_acquire_v1 = 0x00014070;
// Lock0 Release NV
const unsigned int shim_lock0_release_nv = 0x00014000;
// Lock0 Release V0
const unsigned int shim_lock0_release_v0 = 0x00014020;
// Lock0 Release V1
const unsigned int shim_lock0_release_v1 = 0x00014030;
// Lock10 Acquire NV
const unsigned int shim_lock10_acquire_nv = 0x00014540;
// Lock10 Acquire V0
const unsigned int shim_lock10_acquire_v0 = 0x00014560;
// Lock10 Acquire V1
const unsigned int shim_lock10_acquire_v1 = 0x00014570;
// Lock10 Release NV
const unsigned int shim_lock10_release_nv = 0x00014500;
// Lock10 Release V0
const unsigned int shim_lock10_release_v0 = 0x00014520;
// Lock10 Release V1
const unsigned int shim_lock10_release_v1 = 0x00014530;
// Lock11 Acquire NV
const unsigned int shim_lock11_acquire_nv = 0x000145C0;
// Lock11 Acquire V0
const unsigned int shim_lock11_acquire_v0 = 0x000145E0;
// Lock11 Acquire V1
const unsigned int shim_lock11_acquire_v1 = 0x000145F0;
// Lock11 Release NV
const unsigned int shim_lock11_release_nv = 0x00014580;
// Lock11 Release V0
const unsigned int shim_lock11_release_v0 = 0x000145A0;
// Lock11 Release V1
const unsigned int shim_lock11_release_v1 = 0x000145B0;
// Lock12 Acquire NV
const unsigned int shim_lock12_acquire_nv = 0x00014640;
// Lock12 Acquire V0
const unsigned int shim_lock12_acquire_v0 = 0x00014660;
// Lock12 Acquire V1
const unsigned int shim_lock12_acquire_v1 = 0x00014670;
// Lock12 Release NV
const unsigned int shim_lock12_release_nv = 0x00014600;
// Lock12 Release V0
const unsigned int shim_lock12_release_v0 = 0x00014620;
// Lock12 Release V1
const unsigned int shim_lock12_release_v1 = 0x00014630;
// Lock13 Acquire NV
const unsigned int shim_lock13_acquire_nv = 0x000146C0;
// Lock13 Acquire V0
const unsigned int shim_lock13_acquire_v0 = 0x000146E0;
// Lock13 Acquire V1
const unsigned int shim_lock13_acquire_v1 = 0x000146F0;
// Lock13 Release NV
const unsigned int shim_lock13_release_nv = 0x00014680;
// Lock13 Release V0
const unsigned int shim_lock13_release_v0 = 0x000146A0;
// Lock13 Release V1
const unsigned int shim_lock13_release_v1 = 0x000146B0;
// Lock14 Acquire NV
const unsigned int shim_lock14_acquire_nv = 0x00014740;
// Lock14 Acquire V0
const unsigned int shim_lock14_acquire_v0 = 0x00014760;
// Lock14 Acquire V1
const unsigned int shim_lock14_acquire_v1 = 0x00014770;
// Lock14 Release NV
const unsigned int shim_lock14_release_nv = 0x00014700;
// Lock14 Release V0
const unsigned int shim_lock14_release_v0 = 0x00014720;
// Lock14 Release V1
const unsigned int shim_lock14_release_v1 = 0x00014730;
// Lock15 Acquire NV
const unsigned int shim_lock15_acquire_nv = 0x000147C0;
// Lock15 Acquire V0
const unsigned int shim_lock15_acquire_v0 = 0x000147E0;
// Lock15 Acquire V1
const unsigned int shim_lock15_acquire_v1 = 0x000147F0;
// Lock15 Release NV
const unsigned int shim_lock15_release_nv = 0x00014780;
// Lock15 Release V0
const unsigned int shim_lock15_release_v0 = 0x000147A0;
// Lock15 Release V1
const unsigned int shim_lock15_release_v1 = 0x000147B0;
// Lock1 Acquire NV
const unsigned int shim_lock1_acquire_nv = 0x000140C0;
// Lock1 Acquire V0
const unsigned int shim_lock1_acquire_v0 = 0x000140E0;
// Lock1 Acquire V1
const unsigned int shim_lock1_acquire_v1 = 0x000140F0;
// Lock1 Release NV
const unsigned int shim_lock1_release_nv = 0x00014080;
// Lock1 Release V0
const unsigned int shim_lock1_release_v0 = 0x000140A0;
// Lock1 Release V1
const unsigned int shim_lock1_release_v1 = 0x000140B0;
// Lock2 Acquire NV
const unsigned int shim_lock2_acquire_nv = 0x00014140;
// Lock2 Acquire V0
const unsigned int shim_lock2_acquire_v0 = 0x00014160;
// Lock2 Acquire V1
const unsigned int shim_lock2_acquire_v1 = 0x00014170;
// Lock2 Release NV
const unsigned int shim_lock2_release_nv = 0x00014100;
// Lock2 Release V0
const unsigned int shim_lock2_release_v0 = 0x00014120;
// Lock2 Release V1
const unsigned int shim_lock2_release_v1 = 0x00014130;
// Lock3 Acquire NV
const unsigned int shim_lock3_acquire_nv = 0x000141C0;
// Lock3 Acquire V0
const unsigned int shim_lock3_acquire_v0 = 0x000141E0;
// Lock3 Acquire V1
const unsigned int shim_lock3_acquire_v1 = 0x000141F0;
// Lock3 Release NV
const unsigned int shim_lock3_release_nv = 0x00014180;
// Lock3 Release V0
const unsigned int shim_lock3_release_v0 = 0x000141A0;
// Lock3 Release V1
const unsigned int shim_lock3_release_v1 = 0x000141B0;
// Lock4 Acquire NV
const unsigned int shim_lock4_acquire_nv = 0x00014240;
// Lock4 Acquire V0
const unsigned int shim_lock4_acquire_v0 = 0x00014260;
// Lock4 Acquire V1
const unsigned int shim_lock4_acquire_v1 = 0x00014270;
// Lock4 Release NV
const unsigned int shim_lock4_release_nv = 0x00014200;
// Lock4 Release V0
const unsigned int shim_lock4_release_v0 = 0x00014220;
// Lock4 Release V1
const unsigned int shim_lock4_release_v1 = 0x00014230;
// Lock5 Acquire NV
const unsigned int shim_lock5_acquire_nv = 0x000142C0;
// Lock5 Acquire V0
const unsigned int shim_lock5_acquire_v0 = 0x000142E0;
// Lock5 Acquire V1
const unsigned int shim_lock5_acquire_v1 = 0x000142F0;
// Lock5 Release NV
const unsigned int shim_lock5_release_nv = 0x00014280;
// Lock5 Release V0
const unsigned int shim_lock5_release_v0 = 0x000142A0;
// Lock5 Release V1
const unsigned int shim_lock5_release_v1 = 0x000142B0;
// Lock6 Acquire NV
const unsigned int shim_lock6_acquire_nv = 0x00014340;
// Lock6 Acquire V0
const unsigned int shim_lock6_acquire_v0 = 0x00014360;
// Lock6 Acquire V1
const unsigned int shim_lock6_acquire_v1 = 0x00014370;
// Lock6 Release NV
const unsigned int shim_lock6_release_nv = 0x00014300;
// Lock6 Release V0
const unsigned int shim_lock6_release_v0 = 0x00014320;
// Lock6 Release V1
const unsigned int shim_lock6_release_v1 = 0x00014330;
// Lock7 Acquire NV
const unsigned int shim_lock7_acquire_nv = 0x000143C0;
// Lock7 Acquire V0
const unsigned int shim_lock7_acquire_v0 = 0x000143E0;
// Lock7 Acquire V1
const unsigned int shim_lock7_acquire_v1 = 0x000143F0;
// Lock7 Release NV
const unsigned int shim_lock7_release_nv = 0x00014380;
// Lock7 Release V0
const unsigned int shim_lock7_release_v0 = 0x000143A0;
// Lock7 Release V1
const unsigned int shim_lock7_release_v1 = 0x000143B0;
// Lock8 Acquire NV
const unsigned int shim_lock8_acquire_nv = 0x00014440;
// Lock8 Acquire V0
const unsigned int shim_lock8_acquire_v0 = 0x00014460;
// Lock8 Acquire V1
const unsigned int shim_lock8_acquire_v1 = 0x00014470;
// Lock8 Release NV
const unsigned int shim_lock8_release_nv = 0x00014400;
// Lock8 Release V0
const unsigned int shim_lock8_release_v0 = 0x00014420;
// Lock8 Release V1
const unsigned int shim_lock8_release_v1 = 0x00014430;
// Lock9 Acquire NV
const unsigned int shim_lock9_acquire_nv = 0x000144C0;
// Lock9 Acquire V0
const unsigned int shim_lock9_acquire_v0 = 0x000144E0;
// Lock9 Acquire V1
const unsigned int shim_lock9_acquire_v1 = 0x000144F0;
// Lock9 Release NV
const unsigned int shim_lock9_release_nv = 0x00014480;
// Lock9 Release V0
const unsigned int shim_lock9_release_v0 = 0x000144A0;
// Lock9 Release V1
const unsigned int shim_lock9_release_v1 = 0x000144B0;
// Lock Event Value Control 0
const unsigned int shim_lock_event_value_control_0 = 0x00014F20;
// Lock Event Value Control 1
const unsigned int shim_lock_event_value_control_1 = 0x00014F24;
// ME AXIMM Config
const unsigned int shim_me_aximm_config = 0x0001E020;
// ME Shim Reset Enable
const unsigned int shim_me_shim_reset_enable = 0x0003604C;
// ME Tile Column Reset
const unsigned int shim_me_tile_column_reset = 0x00036048;
// Mux Config
const unsigned int shim_mux_config = 0x0001F000;
// NoC Interface ME to NoC South2
const unsigned int shim_noc_interface_me_to_noc_south2 = 0x0001E008;
// NoC Interface ME to NoC South3
const unsigned int shim_noc_interface_me_to_noc_south3 = 0x0001E00C;
// NoC Interface ME to NoC South4
const unsigned int shim_noc_interface_me_to_noc_south4 = 0x0001E010;
// NoC Interface ME to NoC South5
const unsigned int shim_noc_interface_me_to_noc_south5 = 0x0001E014;
// Performance Counters 1-0 Start and Stop Events
const unsigned int shim_performance_control0 = 0x00031000;
const unsigned int shim_performance_start_stop_0_1 = 0x00031000;
// Performance Counters 1-0 Reset Events
const unsigned int shim_performance_control1 = 0x00031008;
const unsigned int shim_performance_reset_0_1 = 0x00031008;
// Performance Counter0
const unsigned int shim_performance_counter0 = 0x00031020;
// Performance Counter0 Event Value
const unsigned int shim_performance_counter0_event_value = 0x00031080;
// Performance Counter1
const unsigned int shim_performance_counter1 = 0x00031024;
// Performance Counter1 Event Value
const unsigned int shim_performance_counter1_event_value = 0x00031084;
// PL Interface Downsizer Bypass
const unsigned int shim_pl_interface_downsizer_bypass = 0x0003300C;
// PL Interface Downsizer Config
const unsigned int shim_pl_interface_downsizer_config = 0x00033004;
// PL Interface Downsizer Enable
const unsigned int shim_pl_interface_downsizer_enable = 0x00033008;
// PL Interface Upsizer Config
const unsigned int shim_pl_interface_upsizer_config = 0x00033000;
// Reserved0
const unsigned int shim_reserved0 = 0x00034210;
// Reserved1
const unsigned int shim_reserved1 = 0x00034214;
// Reserved2
const unsigned int shim_reserved2 = 0x00034218;
// Reserved3
const unsigned int shim_reserved3 = 0x0003421C;
// Spare Reg
const unsigned int shim_spare_reg = 0x00036050;
// Stream Switch Event Port Selection 0
const unsigned int shim_stream_switch_event_port_selection_0 = 0x0003FF00;
// Stream Switch Event Port Selection 1
const unsigned int shim_stream_switch_event_port_selection_1 = 0x0003FF04;
// Stream Switch Master Config East0
const unsigned int shim_stream_switch_master_config_east0 = 0x0003F04C;
// Stream Switch Master Config East1
const unsigned int shim_stream_switch_master_config_east1 = 0x0003F050;
// Stream Switch Master Config East2
const unsigned int shim_stream_switch_master_config_east2 = 0x0003F054;
// Stream Switch Master Config East3
const unsigned int shim_stream_switch_master_config_east3 = 0x0003F058;
// Stream Switch Master Config FIFO0
const unsigned int shim_stream_switch_master_config_fifo0 = 0x0003F004;
// Stream Switch Master Config FIFO1
const unsigned int shim_stream_switch_master_config_fifo1 = 0x0003F008;
// Stream Switch Master Config North0
const unsigned int shim_stream_switch_master_config_north0 = 0x0003F034;
// Stream Switch Master Config North1
const unsigned int shim_stream_switch_master_config_north1 = 0x0003F038;
// Stream Switch Master Config North2
const unsigned int shim_stream_switch_master_config_north2 = 0x0003F03C;
// Stream Switch Master Config North3
const unsigned int shim_stream_switch_master_config_north3 = 0x0003F040;
// Stream Switch Master Config North4
const unsigned int shim_stream_switch_master_config_north4 = 0x0003F044;
// Stream Switch Master Config North5
const unsigned int shim_stream_switch_master_config_north5 = 0x0003F048;
// Stream Switch Master Config South0
const unsigned int shim_stream_switch_master_config_south0 = 0x0003F00C;
// Stream Switch Master Config South1
const unsigned int shim_stream_switch_master_config_south1 = 0x0003F010;
// Stream Switch Master Config South2
const unsigned int shim_stream_switch_master_config_south2 = 0x0003F014;
// Stream Switch Master Config South3
const unsigned int shim_stream_switch_master_config_south3 = 0x0003F018;
// Stream Switch Master Config South4
const unsigned int shim_stream_switch_master_config_south4 = 0x0003F01C;
// Stream Switch Master Config South5
const unsigned int shim_stream_switch_master_config_south5 = 0x0003F020;
// Stream Switch Master Config Tile Ctrl
const unsigned int shim_stream_switch_master_config_tile_ctrl = 0x0003F000;
// Stream Switch Master Config West0
const unsigned int shim_stream_switch_master_config_west0 = 0x0003F024;
// Stream Switch Master Config West1
const unsigned int shim_stream_switch_master_config_west1 = 0x0003F028;
// Stream Switch Master Config West2
const unsigned int shim_stream_switch_master_config_west2 = 0x0003F02C;
// Stream Switch Master Config West3
const unsigned int shim_stream_switch_master_config_west3 = 0x0003F030;
// Stream Switch Slave East 0 Config
const unsigned int shim_stream_switch_slave_east_0_config = 0x0003F14C;
// Stream Switch Slave East 0 Slot0
const unsigned int shim_stream_switch_slave_east_0_slot0 = 0x0003F330;
// Stream Switch Slave East 0 Slot1
const unsigned int shim_stream_switch_slave_east_0_slot1 = 0x0003F334;
// Stream Switch Slave East 0 Slot2
const unsigned int shim_stream_switch_slave_east_0_slot2 = 0x0003F338;
// Stream Switch Slave East 0 Slot3
const unsigned int shim_stream_switch_slave_east_0_slot3 = 0x0003F33C;
// Stream Switch Slave East 1 Config
const unsigned int shim_stream_switch_slave_east_1_config = 0x0003F150;
// Stream Switch Slave East 1 Slot0
const unsigned int shim_stream_switch_slave_east_1_slot0 = 0x0003F340;
// Stream Switch Slave East 1 Slot1
const unsigned int shim_stream_switch_slave_east_1_slot1 = 0x0003F344;
// Stream Switch Slave East 1 Slot2
const unsigned int shim_stream_switch_slave_east_1_slot2 = 0x0003F348;
// Stream Switch Slave East 1 Slot3
const unsigned int shim_stream_switch_slave_east_1_slot3 = 0x0003F34C;
// Stream Switch Slave East 2 Config
const unsigned int shim_stream_switch_slave_east_2_config = 0x0003F154;
// Stream Switch Slave East 2 Slot0
const unsigned int shim_stream_switch_slave_east_2_slot0 = 0x0003F350;
// Stream Switch Slave East 2 Slot1
const unsigned int shim_stream_switch_slave_east_2_slot1 = 0x0003F354;
// Stream Switch Slave East 2 Slot2
const unsigned int shim_stream_switch_slave_east_2_slot2 = 0x0003F358;
// Stream Switch Slave East 2 Slot3
const unsigned int shim_stream_switch_slave_east_2_slot3 = 0x0003F35C;
// Stream Switch Slave East 3 Config
const unsigned int shim_stream_switch_slave_east_3_config = 0x0003F158;
// Stream Switch Slave East 3 Slot0
const unsigned int shim_stream_switch_slave_east_3_slot0 = 0x0003F360;
// Stream Switch Slave East 3 Slot1
const unsigned int shim_stream_switch_slave_east_3_slot1 = 0x0003F364;
// Stream Switch Slave East 3 Slot2
const unsigned int shim_stream_switch_slave_east_3_slot2 = 0x0003F368;
// Stream Switch Slave East 3 Slot3
const unsigned int shim_stream_switch_slave_east_3_slot3 = 0x0003F36C;
// Stream Switch Slave FIFO 0 Config
const unsigned int shim_stream_switch_slave_fifo_0_config = 0x0003F104;
// Stream Switch Slave FIFO 0 Slot0
const unsigned int shim_stream_switch_slave_fifo_0_slot0 = 0x0003F210;
// Stream Switch Slave FIFO 0 Slot1
const unsigned int shim_stream_switch_slave_fifo_0_slot1 = 0x0003F214;
// Stream Switch Slave FIFO 0 Slot2
const unsigned int shim_stream_switch_slave_fifo_0_slot2 = 0x0003F218;
// Stream Switch Slave FIFO 0 Slot3
const unsigned int shim_stream_switch_slave_fifo_0_slot3 = 0x0003F21C;
// Stream Switch Slave FIFO 1 Config
const unsigned int shim_stream_switch_slave_fifo_1_config = 0x0003F108;
// Stream Switch Slave FIFO 1 Slot0
const unsigned int shim_stream_switch_slave_fifo_1_slot0 = 0x0003F220;
// Stream Switch Slave FIFO 1 Slot1
const unsigned int shim_stream_switch_slave_fifo_1_slot1 = 0x0003F224;
// Stream Switch Slave FIFO 1 Slot2
const unsigned int shim_stream_switch_slave_fifo_1_slot2 = 0x0003F228;
// Stream Switch Slave FIFO 1 Slot3
const unsigned int shim_stream_switch_slave_fifo_1_slot3 = 0x0003F22C;
// Stream Switch Slave North 0 Config
const unsigned int shim_stream_switch_slave_north_0_config = 0x0003F13C;
// Stream Switch Slave North 0 Slot0
const unsigned int shim_stream_switch_slave_north_0_slot0 = 0x0003F2F0;
// Stream Switch Slave North 0 Slot1
const unsigned int shim_stream_switch_slave_north_0_slot1 = 0x0003F2F4;
// Stream Switch Slave North 0 Slot2
const unsigned int shim_stream_switch_slave_north_0_slot2 = 0x0003F2F8;
// Stream Switch Slave North 0 Slot3
const unsigned int shim_stream_switch_slave_north_0_slot3 = 0x0003F2FC;
// Stream Switch Slave North 1 Config
const unsigned int shim_stream_switch_slave_north_1_config = 0x0003F140;
// Stream Switch Slave North 1 Slot0
const unsigned int shim_stream_switch_slave_north_1_slot0 = 0x0003F300;
// Stream Switch Slave North 1 Slot1
const unsigned int shim_stream_switch_slave_north_1_slot1 = 0x0003F304;
// Stream Switch Slave North 1 Slot2
const unsigned int shim_stream_switch_slave_north_1_slot2 = 0x0003F308;
// Stream Switch Slave North 1 Slot3
const unsigned int shim_stream_switch_slave_north_1_slot3 = 0x0003F30C;
// Stream Switch Slave North 2 Config
const unsigned int shim_stream_switch_slave_north_2_config = 0x0003F144;
// Stream Switch Slave North 2 Slot0
const unsigned int shim_stream_switch_slave_north_2_slot0 = 0x0003F310;
// Stream Switch Slave North 2 Slot1
const unsigned int shim_stream_switch_slave_north_2_slot1 = 0x0003F314;
// Stream Switch Slave North 2 Slot2
const unsigned int shim_stream_switch_slave_north_2_slot2 = 0x0003F318;
// Stream Switch Slave North 2 Slot3
const unsigned int shim_stream_switch_slave_north_2_slot3 = 0x0003F31C;
// Stream Switch Slave North 3 Config
const unsigned int shim_stream_switch_slave_north_3_config = 0x0003F148;
// Stream Switch Slave North 3 Slot0
const unsigned int shim_stream_switch_slave_north_3_slot0 = 0x0003F320;
// Stream Switch Slave North 3 Slot1
const unsigned int shim_stream_switch_slave_north_3_slot1 = 0x0003F324;
// Stream Switch Slave North 3 Slot2
const unsigned int shim_stream_switch_slave_north_3_slot2 = 0x0003F328;
// Stream Switch Slave North 3 Slot3
const unsigned int shim_stream_switch_slave_north_3_slot3 = 0x0003F32C;
// Stream Switch Slave South 0 Config
const unsigned int shim_stream_switch_slave_south_0_config = 0x0003F10C;
// Stream Switch Slave South 0 Slot0
const unsigned int shim_stream_switch_slave_south_0_slot0 = 0x0003F230;
// Stream Switch Slave South 0 Slot1
const unsigned int shim_stream_switch_slave_south_0_slot1 = 0x0003F234;
// Stream Switch Slave South 0 Slot2
const unsigned int shim_stream_switch_slave_south_0_slot2 = 0x0003F238;
// Stream Switch Slave South 0 Slot3
const unsigned int shim_stream_switch_slave_south_0_slot3 = 0x0003F23C;
// Stream Switch Slave South 1 Config
const unsigned int shim_stream_switch_slave_south_1_config = 0x0003F110;
// Stream Switch Slave South 1 Slot0
const unsigned int shim_stream_switch_slave_south_1_slot0 = 0x0003F240;
// Stream Switch Slave South 1 Slot1
const unsigned int shim_stream_switch_slave_south_1_slot1 = 0x0003F244;
// Stream Switch Slave South 1 Slot2
const unsigned int shim_stream_switch_slave_south_1_slot2 = 0x0003F248;
// Stream Switch Slave South 1 Slot3
const unsigned int shim_stream_switch_slave_south_1_slot3 = 0x0003F24C;
// Stream Switch Slave South 2 Config
const unsigned int shim_stream_switch_slave_south_2_config = 0x0003F114;
// Stream Switch Slave South 2 Slot0
const unsigned int shim_stream_switch_slave_south_2_slot0 = 0x0003F250;
// Stream Switch Slave South 2 Slot1
const unsigned int shim_stream_switch_slave_south_2_slot1 = 0x0003F254;
// Stream Switch Slave South 2 Slot2
const unsigned int shim_stream_switch_slave_south_2_slot2 = 0x0003F258;
// Stream Switch Slave South 2 Slot3
const unsigned int shim_stream_switch_slave_south_2_slot3 = 0x0003F25C;
// Stream Switch Slave South 3 Config
const unsigned int shim_stream_switch_slave_south_3_config = 0x0003F118;
// Stream Switch Slave South 3 Slot0
const unsigned int shim_stream_switch_slave_south_3_slot0 = 0x0003F260;
// Stream Switch Slave South 3 Slot1
const unsigned int shim_stream_switch_slave_south_3_slot1 = 0x0003F264;
// Stream Switch Slave South 3 Slot2
const unsigned int shim_stream_switch_slave_south_3_slot2 = 0x0003F268;
// Stream Switch Slave South 3 Slot3
const unsigned int shim_stream_switch_slave_south_3_slot3 = 0x0003F26C;
// Stream Switch Slave South 4 Config
const unsigned int shim_stream_switch_slave_south_4_config = 0x0003F11C;
// Stream Switch Slave South 4 Slot0
const unsigned int shim_stream_switch_slave_south_4_slot0 = 0x0003F270;
// Stream Switch Slave South 4 Slot1
const unsigned int shim_stream_switch_slave_south_4_slot1 = 0x0003F274;
// Stream Switch Slave South 4 Slot2
const unsigned int shim_stream_switch_slave_south_4_slot2 = 0x0003F278;
// Stream Switch Slave South 4 Slot3
const unsigned int shim_stream_switch_slave_south_4_slot3 = 0x0003F27C;
// Stream Switch Slave South 5 Config
const unsigned int shim_stream_switch_slave_south_5_config = 0x0003F120;
// Stream Switch Slave South 5 Slot0
const unsigned int shim_stream_switch_slave_south_5_slot0 = 0x0003F280;
// Stream Switch Slave South 5 Slot1
const unsigned int shim_stream_switch_slave_south_5_slot1 = 0x0003F284;
// Stream Switch Slave South 5 Slot2
const unsigned int shim_stream_switch_slave_south_5_slot2 = 0x0003F288;
// Stream Switch Slave South 5 Slot3
const unsigned int shim_stream_switch_slave_south_5_slot3 = 0x0003F28C;
// Stream Switch Slave South 6 Config
const unsigned int shim_stream_switch_slave_south_6_config = 0x0003F124;
// Stream Switch Slave South 6 Slot0
const unsigned int shim_stream_switch_slave_south_6_slot0 = 0x0003F290;
// Stream Switch Slave South 6 Slot1
const unsigned int shim_stream_switch_slave_south_6_slot1 = 0x0003F294;
// Stream Switch Slave South 6 Slot2
const unsigned int shim_stream_switch_slave_south_6_slot2 = 0x0003F298;
// Stream Switch Slave South 6 Slot3
const unsigned int shim_stream_switch_slave_south_6_slot3 = 0x0003F29C;
// Stream Switch Slave South 7 Config
const unsigned int shim_stream_switch_slave_south_7_config = 0x0003F128;
// Stream Switch Slave South 7 Slot0
const unsigned int shim_stream_switch_slave_south_7_slot0 = 0x0003F2A0;
// Stream Switch Slave South 7 Slot1
const unsigned int shim_stream_switch_slave_south_7_slot1 = 0x0003F2A4;
// Stream Switch Slave South 7 Slot2
const unsigned int shim_stream_switch_slave_south_7_slot2 = 0x0003F2A8;
// Stream Switch Slave South 7 Slot3
const unsigned int shim_stream_switch_slave_south_7_slot3 = 0x0003F2AC;
// Stream Switch Slave Tile Ctrl Config
const unsigned int shim_stream_switch_slave_tile_ctrl_config = 0x0003F100;
// Stream Switch Slave Tile Ctrl Slot0
const unsigned int shim_stream_switch_slave_tile_ctrl_slot0 = 0x0003F200;
// Stream Switch Slave Tile Ctrl Slot1
const unsigned int shim_stream_switch_slave_tile_ctrl_slot1 = 0x0003F204;
// Stream Switch Slave Tile Ctrl Slot2
const unsigned int shim_stream_switch_slave_tile_ctrl_slot2 = 0x0003F208;
// Stream Switch Slave Tile Ctrl Slot3
const unsigned int shim_stream_switch_slave_tile_ctrl_slot3 = 0x0003F20C;
// Stream Switch Slave Trace Config
const unsigned int shim_stream_switch_slave_trace_config = 0x0003F15C;
// Stream Switch Slave Trace Slot0
const unsigned int shim_stream_switch_slave_trace_slot0 = 0x0003F370;
// Stream Switch Slave Trace Slot1
const unsigned int shim_stream_switch_slave_trace_slot1 = 0x0003F374;
// Stream Switch Slave Trace Slot2
const unsigned int shim_stream_switch_slave_trace_slot2 = 0x0003F378;
// Stream Switch Slave Trace Slot3
const unsigned int shim_stream_switch_slave_trace_slot3 = 0x0003F37C;
// Stream Switch Slave West 0 Config
const unsigned int shim_stream_switch_slave_west_0_config = 0x0003F12C;
// Stream Switch Slave West 0 Slot0
const unsigned int shim_stream_switch_slave_west_0_slot0 = 0x0003F2B0;
// Stream Switch Slave West 0 Slot1
const unsigned int shim_stream_switch_slave_west_0_slot1 = 0x0003F2B4;
// Stream Switch Slave West 0 Slot2
const unsigned int shim_stream_switch_slave_west_0_slot2 = 0x0003F2B8;
// Stream Switch Slave West 0 Slot3
const unsigned int shim_stream_switch_slave_west_0_slot3 = 0x0003F2BC;
// Stream Switch Slave West 1 Config
const unsigned int shim_stream_switch_slave_west_1_config = 0x0003F130;
// Stream Switch Slave West 1 Slot0
const unsigned int shim_stream_switch_slave_west_1_slot0 = 0x0003F2C0;
// Stream Switch Slave West 1 Slot1
const unsigned int shim_stream_switch_slave_west_1_slot1 = 0x0003F2C4;
// Stream Switch Slave West 1 Slot2
const unsigned int shim_stream_switch_slave_west_1_slot2 = 0x0003F2C8;
// Stream Switch Slave West 1 Slot3
const unsigned int shim_stream_switch_slave_west_1_slot3 = 0x0003F2CC;
// Stream Switch Slave West 2 Config
const unsigned int shim_stream_switch_slave_west_2_config = 0x0003F134;
// Stream Switch Slave West 2 Slot0
const unsigned int shim_stream_switch_slave_west_2_slot0 = 0x0003F2D0;
// Stream Switch Slave West 2 Slot1
const unsigned int shim_stream_switch_slave_west_2_slot1 = 0x0003F2D4;
// Stream Switch Slave West 2 Slot2
const unsigned int shim_stream_switch_slave_west_2_slot2 = 0x0003F2D8;
// Stream Switch Slave West 2 Slot3
const unsigned int shim_stream_switch_slave_west_2_slot3 = 0x0003F2DC;
// Stream Switch Slave West 3 Config
const unsigned int shim_stream_switch_slave_west_3_config = 0x0003F138;
// Stream Switch Slave West 3 Slot0
const unsigned int shim_stream_switch_slave_west_3_slot0 = 0x0003F2E0;
// Stream Switch Slave West 3 Slot1
const unsigned int shim_stream_switch_slave_west_3_slot1 = 0x0003F2E4;
// Stream Switch Slave West 3 Slot2
const unsigned int shim_stream_switch_slave_west_3_slot2 = 0x0003F2E8;
// Stream Switch Slave West 3 Slot3
const unsigned int shim_stream_switch_slave_west_3_slot3 = 0x0003F2EC;
// Tile Clock Control
const unsigned int shim_tile_clock_control = 0x00036040;
// Tile Control
const unsigned int shim_tile_control = 0x00036030;
// Timer Control
const unsigned int shim_timer_control = 0x00034000;
// Timer High
const unsigned int shim_timer_high = 0x000340FC;
// Timer Low
const unsigned int shim_timer_low = 0x000340F8;
// Timer Trig Event High Value
const unsigned int shim_timer_trig_event_high_value = 0x000340F4;
// Timer Trig Event Low Value
const unsigned int shim_timer_trig_event_low_value = 0x000340F0;
// Trace Control0
const unsigned int shim_trace_control0 = 0x000340D0;
// Trace Control1
const unsigned int shim_trace_control1 = 0x000340D4;
// Trace Event0
const unsigned int shim_trace_event0 = 0x000340E0;
// Trace Event1
const unsigned int shim_trace_event1 = 0x000340E4;
// Trace Status
const unsigned int shim_trace_status = 0x000340D8;

} // namespace aie1

#endif /* AIE1_REGISTERS_H_ */
