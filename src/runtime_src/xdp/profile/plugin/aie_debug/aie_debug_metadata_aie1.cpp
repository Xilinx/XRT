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

#define XDP_PLUGIN_SOURCE

#include "xdp/profile/plugin/aie_debug/generations/aie1_registers.h"
#include "xdp/profile/plugin/aie_debug/aie_debug_metadata.h"


namespace xdp {
void AIE1UsedRegisters::populateProfileRegisters() {
    // Core modules
    core_addresses.emplace(aie1::cm_performance_control0);
    core_addresses.emplace(aie1::cm_performance_control1);
    core_addresses.emplace(aie1::cm_performance_control2);
    core_addresses.emplace(aie1::cm_performance_counter0);
    core_addresses.emplace(aie1::cm_performance_counter1);
    core_addresses.emplace(aie1::cm_performance_counter2);
    core_addresses.emplace(aie1::cm_performance_counter3);
    core_addresses.emplace(aie1::cm_performance_counter0_event_value);
    core_addresses.emplace(aie1::cm_performance_counter1_event_value);
    core_addresses.emplace(aie1::cm_performance_counter2_event_value);
    core_addresses.emplace(aie1::cm_performance_counter3_event_value);

    // Memory modules
    memory_addresses.emplace(aie1::mm_performance_control0);
    memory_addresses.emplace(aie1::mm_performance_control1);
    memory_addresses.emplace(aie1::mm_performance_counter0);
    memory_addresses.emplace(aie1::mm_performance_counter1);
    memory_addresses.emplace(aie1::mm_performance_counter0_event_value);
    memory_addresses.emplace(aie1::mm_performance_counter1_event_value);

    // Interface tiles
    interface_addresses.emplace(aie1::shim_performance_control0);
    interface_addresses.emplace(aie1::shim_performance_control1);
    interface_addresses.emplace(aie1::shim_performance_counter0);
    interface_addresses.emplace(aie1::shim_performance_counter1);
    interface_addresses.emplace(aie1::shim_performance_counter0_event_value);
    interface_addresses.emplace(aie1::shim_performance_counter1_event_value);

    // Memory tiles
    // NOTE, not available on AIE1
  }

  void AIE1UsedRegisters::populateTraceRegisters() {
    // Core modules
    core_addresses.emplace(aie1::cm_core_status);
    core_addresses.emplace(aie1::cm_trace_control0);
    core_addresses.emplace(aie1::cm_trace_control1);
    core_addresses.emplace(aie1::cm_trace_status);
    core_addresses.emplace(aie1::cm_trace_event0);
    core_addresses.emplace(aie1::cm_trace_event1);
    core_addresses.emplace(aie1::cm_event_status0);
    core_addresses.emplace(aie1::cm_event_status1);
    core_addresses.emplace(aie1::cm_event_status2);
    core_addresses.emplace(aie1::cm_event_status3);
    core_addresses.emplace(aie1::cm_event_broadcast0);
    core_addresses.emplace(aie1::cm_event_broadcast1);
    core_addresses.emplace(aie1::cm_event_broadcast2);
    core_addresses.emplace(aie1::cm_event_broadcast3);
    core_addresses.emplace(aie1::cm_event_broadcast4);
    core_addresses.emplace(aie1::cm_event_broadcast5);
    core_addresses.emplace(aie1::cm_event_broadcast6);
    core_addresses.emplace(aie1::cm_event_broadcast7);
    core_addresses.emplace(aie1::cm_event_broadcast8);
    core_addresses.emplace(aie1::cm_event_broadcast9);
    core_addresses.emplace(aie1::cm_event_broadcast10);
    core_addresses.emplace(aie1::cm_event_broadcast11);
    core_addresses.emplace(aie1::cm_event_broadcast12);
    core_addresses.emplace(aie1::cm_event_broadcast13);
    core_addresses.emplace(aie1::cm_event_broadcast14);
    core_addresses.emplace(aie1::cm_event_broadcast15);
    core_addresses.emplace(aie1::cm_timer_trig_event_low_value);
    core_addresses.emplace(aie1::cm_timer_trig_event_high_value);
    core_addresses.emplace(aie1::cm_timer_low);
    core_addresses.emplace(aie1::cm_timer_high);
    core_addresses.emplace(aie1::cm_stream_switch_event_port_selection_0);
    core_addresses.emplace(aie1::cm_stream_switch_event_port_selection_1);

    // Memory modules
    memory_addresses.emplace(aie1::mm_trace_control0);
    memory_addresses.emplace(aie1::mm_trace_control1);
    memory_addresses.emplace(aie1::mm_trace_status);
    memory_addresses.emplace(aie1::mm_trace_event0);
    memory_addresses.emplace(aie1::mm_trace_event1);
    memory_addresses.emplace(aie1::mm_event_status0);
    memory_addresses.emplace(aie1::mm_event_status1);
    memory_addresses.emplace(aie1::mm_event_status2);
    memory_addresses.emplace(aie1::mm_event_status3);
    memory_addresses.emplace(aie1::mm_event_broadcast0);
    memory_addresses.emplace(aie1::mm_event_broadcast1);
    memory_addresses.emplace(aie1::mm_event_broadcast2);
    memory_addresses.emplace(aie1::mm_event_broadcast3);
    memory_addresses.emplace(aie1::mm_event_broadcast4);
    memory_addresses.emplace(aie1::mm_event_broadcast5);
    memory_addresses.emplace(aie1::mm_event_broadcast6);
    memory_addresses.emplace(aie1::mm_event_broadcast7);
    memory_addresses.emplace(aie1::mm_event_broadcast8);
    memory_addresses.emplace(aie1::mm_event_broadcast9);
    memory_addresses.emplace(aie1::mm_event_broadcast10);
    memory_addresses.emplace(aie1::mm_event_broadcast11);
    memory_addresses.emplace(aie1::mm_event_broadcast12);
    memory_addresses.emplace(aie1::mm_event_broadcast13);
    memory_addresses.emplace(aie1::mm_event_broadcast14);
    memory_addresses.emplace(aie1::mm_event_broadcast15);

    // Interface tiles
    interface_addresses.emplace(aie1::shim_trace_control0);
    interface_addresses.emplace(aie1::shim_trace_control1);
    interface_addresses.emplace(aie1::shim_trace_status);
    interface_addresses.emplace(aie1::shim_trace_event0);
    interface_addresses.emplace(aie1::shim_trace_event1);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_0);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_1);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_2);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_3);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_4);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_5);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_6);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_7);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_8);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_9);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_10);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_11);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_12);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_13);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_14);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_15);
    interface_addresses.emplace(aie1::shim_event_status0);
    interface_addresses.emplace(aie1::shim_event_status1);
    interface_addresses.emplace(aie1::shim_event_status2);
    interface_addresses.emplace(aie1::shim_event_status3);
    interface_addresses.emplace(aie1::shim_stream_switch_event_port_selection_0);
    interface_addresses.emplace(aie1::shim_stream_switch_event_port_selection_1);

    // Memory tiles
    // NOTE, not available on AIE1
  }

/*************************************************************************************
 * AIE1 Registers
 *************************************************************************************/
void AIE1UsedRegisters::populateRegNameToValueMap() {
   regNameToValue["cm_program_memory"] = aie1::cm_program_memory;
   regNameToValue["cm_program_memory_error_injection"] = aie1::cm_program_memory_error_injection;
   regNameToValue["cm_core_r0"] = aie1::cm_core_r0;
   regNameToValue["cm_core_r1"] = aie1::cm_core_r1;
   regNameToValue["cm_core_r2"] = aie1::cm_core_r2;
   regNameToValue["cm_core_r3"] = aie1::cm_core_r3;
   regNameToValue["cm_core_r4"] = aie1::cm_core_r4;
   regNameToValue["cm_core_r5"] = aie1::cm_core_r5;
   regNameToValue["cm_core_r6"] = aie1::cm_core_r6;
   regNameToValue["cm_core_r7"] = aie1::cm_core_r7;
   regNameToValue["cm_core_r8"] = aie1::cm_core_r8;
   regNameToValue["cm_core_r9"] = aie1::cm_core_r9;
   regNameToValue["cm_core_r10"] = aie1::cm_core_r10;
   regNameToValue["cm_core_r11"] = aie1::cm_core_r11;
   regNameToValue["cm_core_r12"] = aie1::cm_core_r12;
   regNameToValue["cm_core_r13"] = aie1::cm_core_r13;
   regNameToValue["cm_core_r14"] = aie1::cm_core_r14;
   regNameToValue["cm_core_r15"] = aie1::cm_core_r15;
   regNameToValue["cm_core_p0"] = aie1::cm_core_p0;
   regNameToValue["cm_core_p1"] = aie1::cm_core_p1;
   regNameToValue["cm_core_p2"] = aie1::cm_core_p2;
   regNameToValue["cm_core_p3"] = aie1::cm_core_p3;
   regNameToValue["cm_core_p4"] = aie1::cm_core_p4;
   regNameToValue["cm_core_p5"] = aie1::cm_core_p5;
   regNameToValue["cm_core_p6"] = aie1::cm_core_p6;
   regNameToValue["cm_core_p7"] = aie1::cm_core_p7;
   regNameToValue["cm_core_cl0"] = aie1::cm_core_cl0;
   regNameToValue["cm_core_ch0"] = aie1::cm_core_ch0;
   regNameToValue["cm_core_cl1"] = aie1::cm_core_cl1;
   regNameToValue["cm_core_ch1"] = aie1::cm_core_ch1;
   regNameToValue["cm_core_cl2"] = aie1::cm_core_cl2;
   regNameToValue["cm_core_ch2"] = aie1::cm_core_ch2;
   regNameToValue["cm_core_cl3"] = aie1::cm_core_cl3;
   regNameToValue["cm_core_ch3"] = aie1::cm_core_ch3;
   regNameToValue["cm_core_cl4"] = aie1::cm_core_cl4;
   regNameToValue["cm_core_ch4"] = aie1::cm_core_ch4;
   regNameToValue["cm_core_cl5"] = aie1::cm_core_cl5;
   regNameToValue["cm_core_ch5"] = aie1::cm_core_ch5;
   regNameToValue["cm_core_cl6"] = aie1::cm_core_cl6;
   regNameToValue["cm_core_ch6"] = aie1::cm_core_ch6;
   regNameToValue["cm_core_cl7"] = aie1::cm_core_cl7;
   regNameToValue["cm_core_ch7"] = aie1::cm_core_ch7;
   regNameToValue["cm_program_counter"] = aie1::cm_program_counter;
   regNameToValue["cm_core_fc"] = aie1::cm_core_fc;
   regNameToValue["cm_core_sp"] = aie1::cm_core_sp;
   regNameToValue["cm_core_lr"] = aie1::cm_core_lr;
   regNameToValue["cm_core_m0"] = aie1::cm_core_m0;
   regNameToValue["cm_core_m1"] = aie1::cm_core_m1;
   regNameToValue["cm_core_m2"] = aie1::cm_core_m2;
   regNameToValue["cm_core_m3"] = aie1::cm_core_m3;
   regNameToValue["cm_core_m4"] = aie1::cm_core_m4;
   regNameToValue["cm_core_m5"] = aie1::cm_core_m5;
   regNameToValue["cm_core_m6"] = aie1::cm_core_m6;
   regNameToValue["cm_core_m7"] = aie1::cm_core_m7;
   regNameToValue["cm_core_cb0"] = aie1::cm_core_cb0;
   regNameToValue["cm_core_cb1"] = aie1::cm_core_cb1;
   regNameToValue["cm_core_cb2"] = aie1::cm_core_cb2;
   regNameToValue["cm_core_cb3"] = aie1::cm_core_cb3;
   regNameToValue["cm_core_cb4"] = aie1::cm_core_cb4;
   regNameToValue["cm_core_cb5"] = aie1::cm_core_cb5;
   regNameToValue["cm_core_cb6"] = aie1::cm_core_cb6;
   regNameToValue["cm_core_cb7"] = aie1::cm_core_cb7;
   regNameToValue["cm_core_cs0"] = aie1::cm_core_cs0;
   regNameToValue["cm_core_cs1"] = aie1::cm_core_cs1;
   regNameToValue["cm_core_cs2"] = aie1::cm_core_cs2;
   regNameToValue["cm_core_cs3"] = aie1::cm_core_cs3;
   regNameToValue["cm_core_cs4"] = aie1::cm_core_cs4;
   regNameToValue["cm_core_cs5"] = aie1::cm_core_cs5;
   regNameToValue["cm_core_cs6"] = aie1::cm_core_cs6;
   regNameToValue["cm_core_cs7"] = aie1::cm_core_cs7;
   regNameToValue["cm_md0"] = aie1::cm_md0;
   regNameToValue["cm_md1"] = aie1::cm_md1;
   regNameToValue["cm_mc0"] = aie1::cm_mc0;
   regNameToValue["cm_mc1"] = aie1::cm_mc1;
   regNameToValue["cm_core_s0"] = aie1::cm_core_s0;
   regNameToValue["cm_core_s1"] = aie1::cm_core_s1;
   regNameToValue["cm_core_s2"] = aie1::cm_core_s2;
   regNameToValue["cm_core_s3"] = aie1::cm_core_s3;
   regNameToValue["cm_core_s4"] = aie1::cm_core_s4;
   regNameToValue["cm_core_s5"] = aie1::cm_core_s5;
   regNameToValue["cm_core_s6"] = aie1::cm_core_s6;
   regNameToValue["cm_core_s7"] = aie1::cm_core_s7;
   regNameToValue["cm_core_ls"] = aie1::cm_core_ls;
   regNameToValue["cm_core_le"] = aie1::cm_core_le;
   regNameToValue["cm_core_lc"] = aie1::cm_core_lc;
   regNameToValue["cm_core_vrl0"] = aie1::cm_core_vrl0;
   regNameToValue["cm_core_vrh0"] = aie1::cm_core_vrh0;
   regNameToValue["cm_core_vrl1"] = aie1::cm_core_vrl1;
   regNameToValue["cm_core_vrh1"] = aie1::cm_core_vrh1;
   regNameToValue["cm_core_vrl2"] = aie1::cm_core_vrl2;
   regNameToValue["cm_core_vrh2"] = aie1::cm_core_vrh2;
   regNameToValue["cm_core_vrl3"] = aie1::cm_core_vrl3;
   regNameToValue["cm_core_vrh3"] = aie1::cm_core_vrh3;
   regNameToValue["cm_core_vcl0"] = aie1::cm_core_vcl0;
   regNameToValue["cm_core_vch0"] = aie1::cm_core_vch0;
   regNameToValue["cm_core_vcl1"] = aie1::cm_core_vcl1;
   regNameToValue["cm_core_vch1"] = aie1::cm_core_vch1;
   regNameToValue["cm_core_vdl0"] = aie1::cm_core_vdl0;
   regNameToValue["cm_core_vdh0"] = aie1::cm_core_vdh0;
   regNameToValue["cm_core_vdl1"] = aie1::cm_core_vdl1;
   regNameToValue["cm_core_vdh1"] = aie1::cm_core_vdh1;
   regNameToValue["cm_core_aml0_part1"] = aie1::cm_core_aml0_part1;
   regNameToValue["cm_core_aml0_part2"] = aie1::cm_core_aml0_part2;
   regNameToValue["cm_core_aml0_part3"] = aie1::cm_core_aml0_part3;
   regNameToValue["cm_core_amh0_part1"] = aie1::cm_core_amh0_part1;
   regNameToValue["cm_core_amh0_part2"] = aie1::cm_core_amh0_part2;
   regNameToValue["cm_core_amh0_part3"] = aie1::cm_core_amh0_part3;
   regNameToValue["cm_core_aml1_part1"] = aie1::cm_core_aml1_part1;
   regNameToValue["cm_core_aml1_part2"] = aie1::cm_core_aml1_part2;
   regNameToValue["cm_core_aml1_part3"] = aie1::cm_core_aml1_part3;
   regNameToValue["cm_core_amh1_part1"] = aie1::cm_core_amh1_part1;
   regNameToValue["cm_core_amh1_part2"] = aie1::cm_core_amh1_part2;
   regNameToValue["cm_core_amh1_part3"] = aie1::cm_core_amh1_part3;
   regNameToValue["cm_core_aml2_part1"] = aie1::cm_core_aml2_part1;
   regNameToValue["cm_core_aml2_part2"] = aie1::cm_core_aml2_part2;
   regNameToValue["cm_core_aml2_part3"] = aie1::cm_core_aml2_part3;
   regNameToValue["cm_core_amh2_part1"] = aie1::cm_core_amh2_part1;
   regNameToValue["cm_core_amh2_part2"] = aie1::cm_core_amh2_part2;
   regNameToValue["cm_core_amh2_part3"] = aie1::cm_core_amh2_part3;
   regNameToValue["cm_core_aml3_part1"] = aie1::cm_core_aml3_part1;
   regNameToValue["cm_core_aml3_part2"] = aie1::cm_core_aml3_part2;
   regNameToValue["cm_core_aml3_part3"] = aie1::cm_core_aml3_part3;
   regNameToValue["cm_core_amh3_part1"] = aie1::cm_core_amh3_part1;
   regNameToValue["cm_core_amh3_part2"] = aie1::cm_core_amh3_part2;
   regNameToValue["cm_core_amh3_part3"] = aie1::cm_core_amh3_part3;
   regNameToValue["cm_performance_control0"] = aie1::cm_performance_control0;
   regNameToValue["cm_performance_control1"] = aie1::cm_performance_control1;
   regNameToValue["cm_performance_control2"] = aie1::cm_performance_control2;
   regNameToValue["cm_performance_counter0"] = aie1::cm_performance_counter0;
   regNameToValue["cm_performance_counter1"] = aie1::cm_performance_counter1;
   regNameToValue["cm_performance_counter2"] = aie1::cm_performance_counter2;
   regNameToValue["cm_performance_counter3"] = aie1::cm_performance_counter3;
   regNameToValue["cm_performance_counter0_event_value"] = aie1::cm_performance_counter0_event_value;
   regNameToValue["cm_performance_counter1_event_value"] = aie1::cm_performance_counter1_event_value;
   regNameToValue["cm_performance_counter2_event_value"] = aie1::cm_performance_counter2_event_value;
   regNameToValue["cm_performance_counter3_event_value"] = aie1::cm_performance_counter3_event_value;
   regNameToValue["cm_core_control"] = aie1::cm_core_control;
   regNameToValue["cm_core_status"] = aie1::cm_core_status;
   regNameToValue["cm_enable_events"] = aie1::cm_enable_events;
   regNameToValue["cm_reset_event"] = aie1::cm_reset_event;
   regNameToValue["cm_debug_control0"] = aie1::cm_debug_control0;
   regNameToValue["cm_debug_control1"] = aie1::cm_debug_control1;
   regNameToValue["cm_debug_control2"] = aie1::cm_debug_control2;
   regNameToValue["cm_debug_status"] = aie1::cm_debug_status;
   regNameToValue["cm_pc_event0"] = aie1::cm_pc_event0;
   regNameToValue["cm_pc_event1"] = aie1::cm_pc_event1;
   regNameToValue["cm_pc_event2"] = aie1::cm_pc_event2;
   regNameToValue["cm_pc_event3"] = aie1::cm_pc_event3;
   regNameToValue["cm_error_halt_control"] = aie1::cm_error_halt_control;
   regNameToValue["cm_error_halt_event"] = aie1::cm_error_halt_event;
   regNameToValue["cm_ecc_control"] = aie1::cm_ecc_control;
   regNameToValue["cm_ecc_scrubbing_event"] = aie1::cm_ecc_scrubbing_event;
   regNameToValue["cm_ecc_failing_address"] = aie1::cm_ecc_failing_address;
   regNameToValue["cm_reserved0"] = aie1::cm_reserved0;
   regNameToValue["cm_reserved1"] = aie1::cm_reserved1;
   regNameToValue["cm_reserved2"] = aie1::cm_reserved2;
   regNameToValue["cm_reserved3"] = aie1::cm_reserved3;
   regNameToValue["cm_timer_control"] = aie1::cm_timer_control;
   regNameToValue["cm_event_generate"] = aie1::cm_event_generate;
   regNameToValue["cm_event_broadcast0"] = aie1::cm_event_broadcast0;
   regNameToValue["cm_event_broadcast1"] = aie1::cm_event_broadcast1;
   regNameToValue["cm_event_broadcast2"] = aie1::cm_event_broadcast2;
   regNameToValue["cm_event_broadcast3"] = aie1::cm_event_broadcast3;
   regNameToValue["cm_event_broadcast4"] = aie1::cm_event_broadcast4;
   regNameToValue["cm_event_broadcast5"] = aie1::cm_event_broadcast5;
   regNameToValue["cm_event_broadcast6"] = aie1::cm_event_broadcast6;
   regNameToValue["cm_event_broadcast7"] = aie1::cm_event_broadcast7;
   regNameToValue["cm_event_broadcast8"] = aie1::cm_event_broadcast8;
   regNameToValue["cm_event_broadcast9"] = aie1::cm_event_broadcast9;
   regNameToValue["cm_event_broadcast10"] = aie1::cm_event_broadcast10;
   regNameToValue["cm_event_broadcast11"] = aie1::cm_event_broadcast11;
   regNameToValue["cm_event_broadcast12"] = aie1::cm_event_broadcast12;
   regNameToValue["cm_event_broadcast13"] = aie1::cm_event_broadcast13;
   regNameToValue["cm_event_broadcast14"] = aie1::cm_event_broadcast14;
   regNameToValue["cm_event_broadcast15"] = aie1::cm_event_broadcast15;
   regNameToValue["cm_event_broadcast_block_south_set"] = aie1::cm_event_broadcast_block_south_set;
   regNameToValue["cm_event_broadcast_block_south_clr"] = aie1::cm_event_broadcast_block_south_clr;
   regNameToValue["cm_event_broadcast_block_south_value"] = aie1::cm_event_broadcast_block_south_value;
   regNameToValue["cm_event_broadcast_block_west_set"] = aie1::cm_event_broadcast_block_west_set;
   regNameToValue["cm_event_broadcast_block_west_clr"] = aie1::cm_event_broadcast_block_west_clr;
   regNameToValue["cm_event_broadcast_block_west_value"] = aie1::cm_event_broadcast_block_west_value;
   regNameToValue["cm_event_broadcast_block_north_set"] = aie1::cm_event_broadcast_block_north_set;
   regNameToValue["cm_event_broadcast_block_north_clr"] = aie1::cm_event_broadcast_block_north_clr;
   regNameToValue["cm_event_broadcast_block_north_value"] = aie1::cm_event_broadcast_block_north_value;
   regNameToValue["cm_event_broadcast_block_east_set"] = aie1::cm_event_broadcast_block_east_set;
   regNameToValue["cm_event_broadcast_block_east_clr"] = aie1::cm_event_broadcast_block_east_clr;
   regNameToValue["cm_event_broadcast_block_east_value"] = aie1::cm_event_broadcast_block_east_value;
   regNameToValue["cm_trace_control0"] = aie1::cm_trace_control0;
   regNameToValue["cm_trace_control1"] = aie1::cm_trace_control1;
   regNameToValue["cm_trace_status"] = aie1::cm_trace_status;
   regNameToValue["cm_trace_event0"] = aie1::cm_trace_event0;
   regNameToValue["cm_trace_event1"] = aie1::cm_trace_event1;
   regNameToValue["cm_timer_trig_event_low_value"] = aie1::cm_timer_trig_event_low_value;
   regNameToValue["cm_timer_trig_event_high_value"] = aie1::cm_timer_trig_event_high_value;
   regNameToValue["cm_timer_low"] = aie1::cm_timer_low;
   regNameToValue["cm_timer_high"] = aie1::cm_timer_high;
   regNameToValue["cm_event_status0"] = aie1::cm_event_status0;
   regNameToValue["cm_event_status1"] = aie1::cm_event_status1;
   regNameToValue["cm_event_status2"] = aie1::cm_event_status2;
   regNameToValue["cm_event_status3"] = aie1::cm_event_status3;
   regNameToValue["cm_combo_event_inputs"] = aie1::cm_combo_event_inputs;
   regNameToValue["cm_combo_event_control"] = aie1::cm_combo_event_control;
   regNameToValue["cm_event_group_0_enable"] = aie1::cm_event_group_0_enable;
   regNameToValue["cm_event_group_pc_enable"] = aie1::cm_event_group_pc_enable;
   regNameToValue["cm_event_group_core_stall_enable"] = aie1::cm_event_group_core_stall_enable;
   regNameToValue["cm_event_group_core_program_flow_enable"] = aie1::cm_event_group_core_program_flow_enable;
   regNameToValue["cm_event_group_errors0_enable"] = aie1::cm_event_group_errors0_enable;
   regNameToValue["cm_event_group_errors1_enable"] = aie1::cm_event_group_errors1_enable;
   regNameToValue["cm_event_group_stream_switch_enable"] = aie1::cm_event_group_stream_switch_enable;
   regNameToValue["cm_event_group_broadcast_enable"] = aie1::cm_event_group_broadcast_enable;
   regNameToValue["cm_event_group_user_event_enable"] = aie1::cm_event_group_user_event_enable;
   regNameToValue["cm_tile_control"] = aie1::cm_tile_control;
   regNameToValue["cm_tile_control_packet_handler_status"] = aie1::cm_tile_control_packet_handler_status;
   regNameToValue["cm_tile_clock_control"] = aie1::cm_tile_clock_control;
   regNameToValue["cm_cssd_trigger"] = aie1::cm_cssd_trigger;
   regNameToValue["cm_spare_reg"] = aie1::cm_spare_reg;
   regNameToValue["cm_stream_switch_master_config_aie_core0"] = aie1::cm_stream_switch_master_config_aie_core0;
   regNameToValue["cm_stream_switch_master_config_aie_core1"] = aie1::cm_stream_switch_master_config_aie_core1;
   regNameToValue["cm_stream_switch_master_config_dma0"] = aie1::cm_stream_switch_master_config_dma0;
   regNameToValue["cm_stream_switch_master_config_dma1"] = aie1::cm_stream_switch_master_config_dma1;
   regNameToValue["cm_stream_switch_master_config_tile_ctrl"] = aie1::cm_stream_switch_master_config_tile_ctrl;
   regNameToValue["cm_stream_switch_master_config_fifo0"] = aie1::cm_stream_switch_master_config_fifo0;
   regNameToValue["cm_stream_switch_master_config_fifo1"] = aie1::cm_stream_switch_master_config_fifo1;
   regNameToValue["cm_stream_switch_master_config_south0"] = aie1::cm_stream_switch_master_config_south0;
   regNameToValue["cm_stream_switch_master_config_south1"] = aie1::cm_stream_switch_master_config_south1;
   regNameToValue["cm_stream_switch_master_config_south2"] = aie1::cm_stream_switch_master_config_south2;
   regNameToValue["cm_stream_switch_master_config_south3"] = aie1::cm_stream_switch_master_config_south3;
   regNameToValue["cm_stream_switch_master_config_west0"] = aie1::cm_stream_switch_master_config_west0;
   regNameToValue["cm_stream_switch_master_config_west1"] = aie1::cm_stream_switch_master_config_west1;
   regNameToValue["cm_stream_switch_master_config_west2"] = aie1::cm_stream_switch_master_config_west2;
   regNameToValue["cm_stream_switch_master_config_west3"] = aie1::cm_stream_switch_master_config_west3;
   regNameToValue["cm_stream_switch_master_config_north0"] = aie1::cm_stream_switch_master_config_north0;
   regNameToValue["cm_stream_switch_master_config_north1"] = aie1::cm_stream_switch_master_config_north1;
   regNameToValue["cm_stream_switch_master_config_north2"] = aie1::cm_stream_switch_master_config_north2;
   regNameToValue["cm_stream_switch_master_config_north3"] = aie1::cm_stream_switch_master_config_north3;
   regNameToValue["cm_stream_switch_master_config_north4"] = aie1::cm_stream_switch_master_config_north4;
   regNameToValue["cm_stream_switch_master_config_north5"] = aie1::cm_stream_switch_master_config_north5;
   regNameToValue["cm_stream_switch_master_config_east0"] = aie1::cm_stream_switch_master_config_east0;
   regNameToValue["cm_stream_switch_master_config_east1"] = aie1::cm_stream_switch_master_config_east1;
   regNameToValue["cm_stream_switch_master_config_east2"] = aie1::cm_stream_switch_master_config_east2;
   regNameToValue["cm_stream_switch_master_config_east3"] = aie1::cm_stream_switch_master_config_east3;
   regNameToValue["cm_stream_switch_slave_config_aie_core0"] = aie1::cm_stream_switch_slave_config_aie_core0;
   regNameToValue["cm_stream_switch_slave_config_aie_core1"] = aie1::cm_stream_switch_slave_config_aie_core1;
   regNameToValue["cm_stream_switch_slave_config_dma_0"] = aie1::cm_stream_switch_slave_config_dma_0;
   regNameToValue["cm_stream_switch_slave_config_dma_1"] = aie1::cm_stream_switch_slave_config_dma_1;
   regNameToValue["cm_stream_switch_slave_config_tile_ctrl"] = aie1::cm_stream_switch_slave_config_tile_ctrl;
   regNameToValue["cm_stream_switch_slave_config_fifo_0"] = aie1::cm_stream_switch_slave_config_fifo_0;
   regNameToValue["cm_stream_switch_slave_config_fifo_1"] = aie1::cm_stream_switch_slave_config_fifo_1;
   regNameToValue["cm_stream_switch_slave_config_south_0"] = aie1::cm_stream_switch_slave_config_south_0;
   regNameToValue["cm_stream_switch_slave_config_south_1"] = aie1::cm_stream_switch_slave_config_south_1;
   regNameToValue["cm_stream_switch_slave_config_south_2"] = aie1::cm_stream_switch_slave_config_south_2;
   regNameToValue["cm_stream_switch_slave_config_south_3"] = aie1::cm_stream_switch_slave_config_south_3;
   regNameToValue["cm_stream_switch_slave_config_south_4"] = aie1::cm_stream_switch_slave_config_south_4;
   regNameToValue["cm_stream_switch_slave_config_south_5"] = aie1::cm_stream_switch_slave_config_south_5;
   regNameToValue["cm_stream_switch_slave_config_west_0"] = aie1::cm_stream_switch_slave_config_west_0;
   regNameToValue["cm_stream_switch_slave_config_west_1"] = aie1::cm_stream_switch_slave_config_west_1;
   regNameToValue["cm_stream_switch_slave_config_west_2"] = aie1::cm_stream_switch_slave_config_west_2;
   regNameToValue["cm_stream_switch_slave_config_west_3"] = aie1::cm_stream_switch_slave_config_west_3;
   regNameToValue["cm_stream_switch_slave_config_north_0"] = aie1::cm_stream_switch_slave_config_north_0;
   regNameToValue["cm_stream_switch_slave_config_north_1"] = aie1::cm_stream_switch_slave_config_north_1;
   regNameToValue["cm_stream_switch_slave_config_north_2"] = aie1::cm_stream_switch_slave_config_north_2;
   regNameToValue["cm_stream_switch_slave_config_north_3"] = aie1::cm_stream_switch_slave_config_north_3;
   regNameToValue["cm_stream_switch_slave_config_east_0"] = aie1::cm_stream_switch_slave_config_east_0;
   regNameToValue["cm_stream_switch_slave_config_east_1"] = aie1::cm_stream_switch_slave_config_east_1;
   regNameToValue["cm_stream_switch_slave_config_east_2"] = aie1::cm_stream_switch_slave_config_east_2;
   regNameToValue["cm_stream_switch_slave_config_east_3"] = aie1::cm_stream_switch_slave_config_east_3;
   regNameToValue["cm_stream_switch_slave_config_aie_trace"] = aie1::cm_stream_switch_slave_config_aie_trace;
   regNameToValue["cm_stream_switch_slave_config_mem_trace"] = aie1::cm_stream_switch_slave_config_mem_trace;
   regNameToValue["cm_stream_switch_slave_aie_core0_slot0"] = aie1::cm_stream_switch_slave_aie_core0_slot0;
   regNameToValue["cm_stream_switch_slave_aie_core0_slot1"] = aie1::cm_stream_switch_slave_aie_core0_slot1;
   regNameToValue["cm_stream_switch_slave_aie_core0_slot2"] = aie1::cm_stream_switch_slave_aie_core0_slot2;
   regNameToValue["cm_stream_switch_slave_aie_core0_slot3"] = aie1::cm_stream_switch_slave_aie_core0_slot3;
   regNameToValue["cm_stream_switch_slave_aie_core1_slot0"] = aie1::cm_stream_switch_slave_aie_core1_slot0;
   regNameToValue["cm_stream_switch_slave_aie_core1_slot1"] = aie1::cm_stream_switch_slave_aie_core1_slot1;
   regNameToValue["cm_stream_switch_slave_aie_core1_slot2"] = aie1::cm_stream_switch_slave_aie_core1_slot2;
   regNameToValue["cm_stream_switch_slave_aie_core1_slot3"] = aie1::cm_stream_switch_slave_aie_core1_slot3;
   regNameToValue["cm_stream_switch_slave_dma_0_slot0"] = aie1::cm_stream_switch_slave_dma_0_slot0;
   regNameToValue["cm_stream_switch_slave_dma_0_slot1"] = aie1::cm_stream_switch_slave_dma_0_slot1;
   regNameToValue["cm_stream_switch_slave_dma_0_slot2"] = aie1::cm_stream_switch_slave_dma_0_slot2;
   regNameToValue["cm_stream_switch_slave_dma_0_slot3"] = aie1::cm_stream_switch_slave_dma_0_slot3;
   regNameToValue["cm_stream_switch_slave_dma_1_slot0"] = aie1::cm_stream_switch_slave_dma_1_slot0;
   regNameToValue["cm_stream_switch_slave_dma_1_slot1"] = aie1::cm_stream_switch_slave_dma_1_slot1;
   regNameToValue["cm_stream_switch_slave_dma_1_slot2"] = aie1::cm_stream_switch_slave_dma_1_slot2;
   regNameToValue["cm_stream_switch_slave_dma_1_slot3"] = aie1::cm_stream_switch_slave_dma_1_slot3;
   regNameToValue["cm_stream_switch_slave_tile_ctrl_slot0"] = aie1::cm_stream_switch_slave_tile_ctrl_slot0;
   regNameToValue["cm_stream_switch_slave_tile_ctrl_slot1"] = aie1::cm_stream_switch_slave_tile_ctrl_slot1;
   regNameToValue["cm_stream_switch_slave_tile_ctrl_slot2"] = aie1::cm_stream_switch_slave_tile_ctrl_slot2;
   regNameToValue["cm_stream_switch_slave_tile_ctrl_slot3"] = aie1::cm_stream_switch_slave_tile_ctrl_slot3;
   regNameToValue["cm_stream_switch_slave_fifo_0_slot0"] = aie1::cm_stream_switch_slave_fifo_0_slot0;
   regNameToValue["cm_stream_switch_slave_fifo_0_slot1"] = aie1::cm_stream_switch_slave_fifo_0_slot1;
   regNameToValue["cm_stream_switch_slave_fifo_0_slot2"] = aie1::cm_stream_switch_slave_fifo_0_slot2;
   regNameToValue["cm_stream_switch_slave_fifo_0_slot3"] = aie1::cm_stream_switch_slave_fifo_0_slot3;
   regNameToValue["cm_stream_switch_slave_fifo_1_slot0"] = aie1::cm_stream_switch_slave_fifo_1_slot0;
   regNameToValue["cm_stream_switch_slave_fifo_1_slot1"] = aie1::cm_stream_switch_slave_fifo_1_slot1;
   regNameToValue["cm_stream_switch_slave_fifo_1_slot2"] = aie1::cm_stream_switch_slave_fifo_1_slot2;
   regNameToValue["cm_stream_switch_slave_fifo_1_slot3"] = aie1::cm_stream_switch_slave_fifo_1_slot3;
   regNameToValue["cm_stream_switch_slave_south_0_slot0"] = aie1::cm_stream_switch_slave_south_0_slot0;
   regNameToValue["cm_stream_switch_slave_south_0_slot1"] = aie1::cm_stream_switch_slave_south_0_slot1;
   regNameToValue["cm_stream_switch_slave_south_0_slot2"] = aie1::cm_stream_switch_slave_south_0_slot2;
   regNameToValue["cm_stream_switch_slave_south_0_slot3"] = aie1::cm_stream_switch_slave_south_0_slot3;
   regNameToValue["cm_stream_switch_slave_south_1_slot0"] = aie1::cm_stream_switch_slave_south_1_slot0;
   regNameToValue["cm_stream_switch_slave_south_1_slot1"] = aie1::cm_stream_switch_slave_south_1_slot1;
   regNameToValue["cm_stream_switch_slave_south_1_slot2"] = aie1::cm_stream_switch_slave_south_1_slot2;
   regNameToValue["cm_stream_switch_slave_south_1_slot3"] = aie1::cm_stream_switch_slave_south_1_slot3;
   regNameToValue["cm_stream_switch_slave_south_2_slot0"] = aie1::cm_stream_switch_slave_south_2_slot0;
   regNameToValue["cm_stream_switch_slave_south_2_slot1"] = aie1::cm_stream_switch_slave_south_2_slot1;
   regNameToValue["cm_stream_switch_slave_south_2_slot2"] = aie1::cm_stream_switch_slave_south_2_slot2;
   regNameToValue["cm_stream_switch_slave_south_2_slot3"] = aie1::cm_stream_switch_slave_south_2_slot3;
   regNameToValue["cm_stream_switch_slave_south_3_slot0"] = aie1::cm_stream_switch_slave_south_3_slot0;
   regNameToValue["cm_stream_switch_slave_south_3_slot1"] = aie1::cm_stream_switch_slave_south_3_slot1;
   regNameToValue["cm_stream_switch_slave_south_3_slot2"] = aie1::cm_stream_switch_slave_south_3_slot2;
   regNameToValue["cm_stream_switch_slave_south_3_slot3"] = aie1::cm_stream_switch_slave_south_3_slot3;
   regNameToValue["cm_stream_switch_slave_south_4_slot0"] = aie1::cm_stream_switch_slave_south_4_slot0;
   regNameToValue["cm_stream_switch_slave_south_4_slot1"] = aie1::cm_stream_switch_slave_south_4_slot1;
   regNameToValue["cm_stream_switch_slave_south_4_slot2"] = aie1::cm_stream_switch_slave_south_4_slot2;
   regNameToValue["cm_stream_switch_slave_south_4_slot3"] = aie1::cm_stream_switch_slave_south_4_slot3;
   regNameToValue["cm_stream_switch_slave_south_5_slot0"] = aie1::cm_stream_switch_slave_south_5_slot0;
   regNameToValue["cm_stream_switch_slave_south_5_slot1"] = aie1::cm_stream_switch_slave_south_5_slot1;
   regNameToValue["cm_stream_switch_slave_south_5_slot2"] = aie1::cm_stream_switch_slave_south_5_slot2;
   regNameToValue["cm_stream_switch_slave_south_5_slot3"] = aie1::cm_stream_switch_slave_south_5_slot3;
   regNameToValue["cm_stream_switch_slave_west_0_slot0"] = aie1::cm_stream_switch_slave_west_0_slot0;
   regNameToValue["cm_stream_switch_slave_west_0_slot1"] = aie1::cm_stream_switch_slave_west_0_slot1;
   regNameToValue["cm_stream_switch_slave_west_0_slot2"] = aie1::cm_stream_switch_slave_west_0_slot2;
   regNameToValue["cm_stream_switch_slave_west_0_slot3"] = aie1::cm_stream_switch_slave_west_0_slot3;
   regNameToValue["cm_stream_switch_slave_west_1_slot0"] = aie1::cm_stream_switch_slave_west_1_slot0;
   regNameToValue["cm_stream_switch_slave_west_1_slot1"] = aie1::cm_stream_switch_slave_west_1_slot1;
   regNameToValue["cm_stream_switch_slave_west_1_slot2"] = aie1::cm_stream_switch_slave_west_1_slot2;
   regNameToValue["cm_stream_switch_slave_west_1_slot3"] = aie1::cm_stream_switch_slave_west_1_slot3;
   regNameToValue["cm_stream_switch_slave_west_2_slot0"] = aie1::cm_stream_switch_slave_west_2_slot0;
   regNameToValue["cm_stream_switch_slave_west_2_slot1"] = aie1::cm_stream_switch_slave_west_2_slot1;
   regNameToValue["cm_stream_switch_slave_west_2_slot2"] = aie1::cm_stream_switch_slave_west_2_slot2;
   regNameToValue["cm_stream_switch_slave_west_2_slot3"] = aie1::cm_stream_switch_slave_west_2_slot3;
   regNameToValue["cm_stream_switch_slave_west_3_slot0"] = aie1::cm_stream_switch_slave_west_3_slot0;
   regNameToValue["cm_stream_switch_slave_west_3_slot1"] = aie1::cm_stream_switch_slave_west_3_slot1;
   regNameToValue["cm_stream_switch_slave_west_3_slot2"] = aie1::cm_stream_switch_slave_west_3_slot2;
   regNameToValue["cm_stream_switch_slave_west_3_slot3"] = aie1::cm_stream_switch_slave_west_3_slot3;
   regNameToValue["cm_stream_switch_slave_north_0_slot0"] = aie1::cm_stream_switch_slave_north_0_slot0;
   regNameToValue["cm_stream_switch_slave_north_0_slot1"] = aie1::cm_stream_switch_slave_north_0_slot1;
   regNameToValue["cm_stream_switch_slave_north_0_slot2"] = aie1::cm_stream_switch_slave_north_0_slot2;
   regNameToValue["cm_stream_switch_slave_north_0_slot3"] = aie1::cm_stream_switch_slave_north_0_slot3;
   regNameToValue["cm_stream_switch_slave_north_1_slot0"] = aie1::cm_stream_switch_slave_north_1_slot0;
   regNameToValue["cm_stream_switch_slave_north_1_slot1"] = aie1::cm_stream_switch_slave_north_1_slot1;
   regNameToValue["cm_stream_switch_slave_north_1_slot2"] = aie1::cm_stream_switch_slave_north_1_slot2;
   regNameToValue["cm_stream_switch_slave_north_1_slot3"] = aie1::cm_stream_switch_slave_north_1_slot3;
   regNameToValue["cm_stream_switch_slave_north_2_slot0"] = aie1::cm_stream_switch_slave_north_2_slot0;
   regNameToValue["cm_stream_switch_slave_north_2_slot1"] = aie1::cm_stream_switch_slave_north_2_slot1;
   regNameToValue["cm_stream_switch_slave_north_2_slot2"] = aie1::cm_stream_switch_slave_north_2_slot2;
   regNameToValue["cm_stream_switch_slave_north_2_slot3"] = aie1::cm_stream_switch_slave_north_2_slot3;
   regNameToValue["cm_stream_switch_slave_north_3_slot0"] = aie1::cm_stream_switch_slave_north_3_slot0;
   regNameToValue["cm_stream_switch_slave_north_3_slot1"] = aie1::cm_stream_switch_slave_north_3_slot1;
   regNameToValue["cm_stream_switch_slave_north_3_slot2"] = aie1::cm_stream_switch_slave_north_3_slot2;
   regNameToValue["cm_stream_switch_slave_north_3_slot3"] = aie1::cm_stream_switch_slave_north_3_slot3;
   regNameToValue["cm_stream_switch_slave_east_0_slot0"] = aie1::cm_stream_switch_slave_east_0_slot0;
   regNameToValue["cm_stream_switch_slave_east_0_slot1"] = aie1::cm_stream_switch_slave_east_0_slot1;
   regNameToValue["cm_stream_switch_slave_east_0_slot2"] = aie1::cm_stream_switch_slave_east_0_slot2;
   regNameToValue["cm_stream_switch_slave_east_0_slot3"] = aie1::cm_stream_switch_slave_east_0_slot3;
   regNameToValue["cm_stream_switch_slave_east_1_slot0"] = aie1::cm_stream_switch_slave_east_1_slot0;
   regNameToValue["cm_stream_switch_slave_east_1_slot1"] = aie1::cm_stream_switch_slave_east_1_slot1;
   regNameToValue["cm_stream_switch_slave_east_1_slot2"] = aie1::cm_stream_switch_slave_east_1_slot2;
   regNameToValue["cm_stream_switch_slave_east_1_slot3"] = aie1::cm_stream_switch_slave_east_1_slot3;
   regNameToValue["cm_stream_switch_slave_east_2_slot0"] = aie1::cm_stream_switch_slave_east_2_slot0;
   regNameToValue["cm_stream_switch_slave_east_2_slot1"] = aie1::cm_stream_switch_slave_east_2_slot1;
   regNameToValue["cm_stream_switch_slave_east_2_slot2"] = aie1::cm_stream_switch_slave_east_2_slot2;
   regNameToValue["cm_stream_switch_slave_east_2_slot3"] = aie1::cm_stream_switch_slave_east_2_slot3;
   regNameToValue["cm_stream_switch_slave_east_3_slot0"] = aie1::cm_stream_switch_slave_east_3_slot0;
   regNameToValue["cm_stream_switch_slave_east_3_slot1"] = aie1::cm_stream_switch_slave_east_3_slot1;
   regNameToValue["cm_stream_switch_slave_east_3_slot2"] = aie1::cm_stream_switch_slave_east_3_slot2;
   regNameToValue["cm_stream_switch_slave_east_3_slot3"] = aie1::cm_stream_switch_slave_east_3_slot3;
   regNameToValue["cm_stream_switch_slave_aie_trace_slot0"] = aie1::cm_stream_switch_slave_aie_trace_slot0;
   regNameToValue["cm_stream_switch_slave_aie_trace_slot1"] = aie1::cm_stream_switch_slave_aie_trace_slot1;
   regNameToValue["cm_stream_switch_slave_aie_trace_slot2"] = aie1::cm_stream_switch_slave_aie_trace_slot2;
   regNameToValue["cm_stream_switch_slave_aie_trace_slot3"] = aie1::cm_stream_switch_slave_aie_trace_slot3;
   regNameToValue["cm_stream_switch_slave_mem_trace_slot0"] = aie1::cm_stream_switch_slave_mem_trace_slot0;
   regNameToValue["cm_stream_switch_slave_mem_trace_slot1"] = aie1::cm_stream_switch_slave_mem_trace_slot1;
   regNameToValue["cm_stream_switch_slave_mem_trace_slot2"] = aie1::cm_stream_switch_slave_mem_trace_slot2;
   regNameToValue["cm_stream_switch_slave_mem_trace_slot3"] = aie1::cm_stream_switch_slave_mem_trace_slot3;
   regNameToValue["cm_stream_switch_event_port_selection_0"] = aie1::cm_stream_switch_event_port_selection_0;
   regNameToValue["cm_stream_switch_event_port_selection_1"] = aie1::cm_stream_switch_event_port_selection_1;
   regNameToValue["mm_all_lock_state_value"] = aie1::mm_all_lock_state_value;
   regNameToValue["mm_checkbit_error_generation"] = aie1::mm_checkbit_error_generation;
   regNameToValue["mm_combo_event_control"] = aie1::mm_combo_event_control;
   regNameToValue["mm_combo_event_inputs"] = aie1::mm_combo_event_inputs;
   regNameToValue["mm_dma_bd0_2d_x"] = aie1::mm_dma_bd0_2d_x;
   regNameToValue["mm_dma_bd0_2d_y"] = aie1::mm_dma_bd0_2d_y;
   regNameToValue["mm_dma_bd0_addr_a"] = aie1::mm_dma_bd0_addr_a;
   regNameToValue["mm_dma_bd0_addr_b"] = aie1::mm_dma_bd0_addr_b;
   regNameToValue["mm_dma_bd0_control"] = aie1::mm_dma_bd0_control;
   regNameToValue["mm_dma_bd0_interleaved_state"] = aie1::mm_dma_bd0_interleaved_state;
   regNameToValue["mm_dma_bd0_packet"] = aie1::mm_dma_bd0_packet;
   regNameToValue["mm_dma_bd10_2d_x"] = aie1::mm_dma_bd10_2d_x;
   regNameToValue["mm_dma_bd10_2d_y"] = aie1::mm_dma_bd10_2d_y;
   regNameToValue["mm_dma_bd10_addr_a"] = aie1::mm_dma_bd10_addr_a;
   regNameToValue["mm_dma_bd10_addr_b"] = aie1::mm_dma_bd10_addr_b;
   regNameToValue["mm_dma_bd10_control"] = aie1::mm_dma_bd10_control;
   regNameToValue["mm_dma_bd10_interleaved_state"] = aie1::mm_dma_bd10_interleaved_state;
   regNameToValue["mm_dma_bd10_packet"] = aie1::mm_dma_bd10_packet;
   regNameToValue["mm_dma_bd11_2d_x"] = aie1::mm_dma_bd11_2d_x;
   regNameToValue["mm_dma_bd11_2d_y"] = aie1::mm_dma_bd11_2d_y;
   regNameToValue["mm_dma_bd11_addr_a"] = aie1::mm_dma_bd11_addr_a;
   regNameToValue["mm_dma_bd11_addr_b"] = aie1::mm_dma_bd11_addr_b;
   regNameToValue["mm_dma_bd11_control"] = aie1::mm_dma_bd11_control;
   regNameToValue["mm_dma_bd11_interleaved_state"] = aie1::mm_dma_bd11_interleaved_state;
   regNameToValue["mm_dma_bd11_packet"] = aie1::mm_dma_bd11_packet;
   regNameToValue["mm_dma_bd12_2d_x"] = aie1::mm_dma_bd12_2d_x;
   regNameToValue["mm_dma_bd12_2d_y"] = aie1::mm_dma_bd12_2d_y;
   regNameToValue["mm_dma_bd12_addr_a"] = aie1::mm_dma_bd12_addr_a;
   regNameToValue["mm_dma_bd12_addr_b"] = aie1::mm_dma_bd12_addr_b;
   regNameToValue["mm_dma_bd12_control"] = aie1::mm_dma_bd12_control;
   regNameToValue["mm_dma_bd12_interleaved_state"] = aie1::mm_dma_bd12_interleaved_state;
   regNameToValue["mm_dma_bd12_packet"] = aie1::mm_dma_bd12_packet;
   regNameToValue["mm_dma_bd13_2d_x"] = aie1::mm_dma_bd13_2d_x;
   regNameToValue["mm_dma_bd13_2d_y"] = aie1::mm_dma_bd13_2d_y;
   regNameToValue["mm_dma_bd13_addr_a"] = aie1::mm_dma_bd13_addr_a;
   regNameToValue["mm_dma_bd13_addr_b"] = aie1::mm_dma_bd13_addr_b;
   regNameToValue["mm_dma_bd13_control"] = aie1::mm_dma_bd13_control;
   regNameToValue["mm_dma_bd13_interleaved_state"] = aie1::mm_dma_bd13_interleaved_state;
   regNameToValue["mm_dma_bd13_packet"] = aie1::mm_dma_bd13_packet;
   regNameToValue["mm_dma_bd14_2d_x"] = aie1::mm_dma_bd14_2d_x;
   regNameToValue["mm_dma_bd14_2d_y"] = aie1::mm_dma_bd14_2d_y;
   regNameToValue["mm_dma_bd14_addr_a"] = aie1::mm_dma_bd14_addr_a;
   regNameToValue["mm_dma_bd14_addr_b"] = aie1::mm_dma_bd14_addr_b;
   regNameToValue["mm_dma_bd14_control"] = aie1::mm_dma_bd14_control;
   regNameToValue["mm_dma_bd14_interleaved_state"] = aie1::mm_dma_bd14_interleaved_state;
   regNameToValue["mm_dma_bd14_packet"] = aie1::mm_dma_bd14_packet;
   regNameToValue["mm_dma_bd15_2d_x"] = aie1::mm_dma_bd15_2d_x;
   regNameToValue["mm_dma_bd15_2d_y"] = aie1::mm_dma_bd15_2d_y;
   regNameToValue["mm_dma_bd15_addr_a"] = aie1::mm_dma_bd15_addr_a;
   regNameToValue["mm_dma_bd15_addr_b"] = aie1::mm_dma_bd15_addr_b;
   regNameToValue["mm_dma_bd15_control"] = aie1::mm_dma_bd15_control;
   regNameToValue["mm_dma_bd15_interleaved_state"] = aie1::mm_dma_bd15_interleaved_state;
   regNameToValue["mm_dma_bd15_packet"] = aie1::mm_dma_bd15_packet;
   regNameToValue["mm_dma_bd1_2d_x"] = aie1::mm_dma_bd1_2d_x;
   regNameToValue["mm_dma_bd1_2d_y"] = aie1::mm_dma_bd1_2d_y;
   regNameToValue["mm_dma_bd1_addr_a"] = aie1::mm_dma_bd1_addr_a;
   regNameToValue["mm_dma_bd1_addr_b"] = aie1::mm_dma_bd1_addr_b;
   regNameToValue["mm_dma_bd1_control"] = aie1::mm_dma_bd1_control;
   regNameToValue["mm_dma_bd1_interleaved_state"] = aie1::mm_dma_bd1_interleaved_state;
   regNameToValue["mm_dma_bd1_packet"] = aie1::mm_dma_bd1_packet;
   regNameToValue["mm_dma_bd2_2d_x"] = aie1::mm_dma_bd2_2d_x;
   regNameToValue["mm_dma_bd2_2d_y"] = aie1::mm_dma_bd2_2d_y;
   regNameToValue["mm_dma_bd2_addr_a"] = aie1::mm_dma_bd2_addr_a;
   regNameToValue["mm_dma_bd2_addr_b"] = aie1::mm_dma_bd2_addr_b;
   regNameToValue["mm_dma_bd2_control"] = aie1::mm_dma_bd2_control;
   regNameToValue["mm_dma_bd2_interleaved_state"] = aie1::mm_dma_bd2_interleaved_state;
   regNameToValue["mm_dma_bd2_packet"] = aie1::mm_dma_bd2_packet;
   regNameToValue["mm_dma_bd3_2d_x"] = aie1::mm_dma_bd3_2d_x;
   regNameToValue["mm_dma_bd3_2d_y"] = aie1::mm_dma_bd3_2d_y;
   regNameToValue["mm_dma_bd3_addr_a"] = aie1::mm_dma_bd3_addr_a;
   regNameToValue["mm_dma_bd3_addr_b"] = aie1::mm_dma_bd3_addr_b;
   regNameToValue["mm_dma_bd3_control"] = aie1::mm_dma_bd3_control;
   regNameToValue["mm_dma_bd3_interleaved_state"] = aie1::mm_dma_bd3_interleaved_state;
   regNameToValue["mm_dma_bd3_packet"] = aie1::mm_dma_bd3_packet;
   regNameToValue["mm_dma_bd4_2d_x"] = aie1::mm_dma_bd4_2d_x;
   regNameToValue["mm_dma_bd4_2d_y"] = aie1::mm_dma_bd4_2d_y;
   regNameToValue["mm_dma_bd4_addr_a"] = aie1::mm_dma_bd4_addr_a;
   regNameToValue["mm_dma_bd4_addr_b"] = aie1::mm_dma_bd4_addr_b;
   regNameToValue["mm_dma_bd4_control"] = aie1::mm_dma_bd4_control;
   regNameToValue["mm_dma_bd4_interleaved_state"] = aie1::mm_dma_bd4_interleaved_state;
   regNameToValue["mm_dma_bd4_packet"] = aie1::mm_dma_bd4_packet;
   regNameToValue["mm_dma_bd5_2d_x"] = aie1::mm_dma_bd5_2d_x;
   regNameToValue["mm_dma_bd5_2d_y"] = aie1::mm_dma_bd5_2d_y;
   regNameToValue["mm_dma_bd5_addr_a"] = aie1::mm_dma_bd5_addr_a;
   regNameToValue["mm_dma_bd5_addr_b"] = aie1::mm_dma_bd5_addr_b;
   regNameToValue["mm_dma_bd5_control"] = aie1::mm_dma_bd5_control;
   regNameToValue["mm_dma_bd5_interleaved_state"] = aie1::mm_dma_bd5_interleaved_state;
   regNameToValue["mm_dma_bd5_packet"] = aie1::mm_dma_bd5_packet;
   regNameToValue["mm_dma_bd6_2d_x"] = aie1::mm_dma_bd6_2d_x;
   regNameToValue["mm_dma_bd6_2d_y"] = aie1::mm_dma_bd6_2d_y;
   regNameToValue["mm_dma_bd6_addr_a"] = aie1::mm_dma_bd6_addr_a;
   regNameToValue["mm_dma_bd6_addr_b"] = aie1::mm_dma_bd6_addr_b;
   regNameToValue["mm_dma_bd6_control"] = aie1::mm_dma_bd6_control;
   regNameToValue["mm_dma_bd6_interleaved_state"] = aie1::mm_dma_bd6_interleaved_state;
   regNameToValue["mm_dma_bd6_packet"] = aie1::mm_dma_bd6_packet;
   regNameToValue["mm_dma_bd7_2d_x"] = aie1::mm_dma_bd7_2d_x;
   regNameToValue["mm_dma_bd7_2d_y"] = aie1::mm_dma_bd7_2d_y;
   regNameToValue["mm_dma_bd7_addr_a"] = aie1::mm_dma_bd7_addr_a;
   regNameToValue["mm_dma_bd7_addr_b"] = aie1::mm_dma_bd7_addr_b;
   regNameToValue["mm_dma_bd7_control"] = aie1::mm_dma_bd7_control;
   regNameToValue["mm_dma_bd7_interleaved_state"] = aie1::mm_dma_bd7_interleaved_state;
   regNameToValue["mm_dma_bd7_packet"] = aie1::mm_dma_bd7_packet;
   regNameToValue["mm_dma_bd8_2d_x"] = aie1::mm_dma_bd8_2d_x;
   regNameToValue["mm_dma_bd8_2d_y"] = aie1::mm_dma_bd8_2d_y;
   regNameToValue["mm_dma_bd8_addr_a"] = aie1::mm_dma_bd8_addr_a;
   regNameToValue["mm_dma_bd8_addr_b"] = aie1::mm_dma_bd8_addr_b;
   regNameToValue["mm_dma_bd8_control"] = aie1::mm_dma_bd8_control;
   regNameToValue["mm_dma_bd8_interleaved_state"] = aie1::mm_dma_bd8_interleaved_state;
   regNameToValue["mm_dma_bd8_packet"] = aie1::mm_dma_bd8_packet;
   regNameToValue["mm_dma_bd9_2d_x"] = aie1::mm_dma_bd9_2d_x;
   regNameToValue["mm_dma_bd9_2d_y"] = aie1::mm_dma_bd9_2d_y;
   regNameToValue["mm_dma_bd9_addr_a"] = aie1::mm_dma_bd9_addr_a;
   regNameToValue["mm_dma_bd9_addr_b"] = aie1::mm_dma_bd9_addr_b;
   regNameToValue["mm_dma_bd9_control"] = aie1::mm_dma_bd9_control;
   regNameToValue["mm_dma_bd9_interleaved_state"] = aie1::mm_dma_bd9_interleaved_state;
   regNameToValue["mm_dma_bd9_packet"] = aie1::mm_dma_bd9_packet;
   regNameToValue["mm_dma_fifo_counter"] = aie1::mm_dma_fifo_counter;
   regNameToValue["mm_dma_mm2s_0_ctrl"] = aie1::mm_dma_mm2s_0_ctrl;
   regNameToValue["mm_dma_mm2s_0_start_queue"] = aie1::mm_dma_mm2s_0_start_queue;
   regNameToValue["mm_dma_mm2s_1_ctrl"] = aie1::mm_dma_mm2s_1_ctrl;
   regNameToValue["mm_dma_mm2s_1_start_queue"] = aie1::mm_dma_mm2s_1_start_queue;
   regNameToValue["mm_dma_mm2s_status"] = aie1::mm_dma_mm2s_status;
   regNameToValue["mm_dma_s2mm_0_ctrl"] = aie1::mm_dma_s2mm_0_ctrl;
   regNameToValue["mm_dma_s2mm_0_start_queue"] = aie1::mm_dma_s2mm_0_start_queue;
   regNameToValue["mm_dma_s2mm_1_ctrl"] = aie1::mm_dma_s2mm_1_ctrl;
   regNameToValue["mm_dma_s2mm_1_start_queue"] = aie1::mm_dma_s2mm_1_start_queue;
   regNameToValue["mm_dma_s2mm_status"] = aie1::mm_dma_s2mm_status;
   regNameToValue["mm_ecc_failing_address"] = aie1::mm_ecc_failing_address;
   regNameToValue["mm_ecc_scrubbing_event"] = aie1::mm_ecc_scrubbing_event;
   regNameToValue["mm_event_broadcast0"] = aie1::mm_event_broadcast0;
   regNameToValue["mm_event_broadcast1"] = aie1::mm_event_broadcast1;
   regNameToValue["mm_event_broadcast10"] = aie1::mm_event_broadcast10;
   regNameToValue["mm_event_broadcast11"] = aie1::mm_event_broadcast11;
   regNameToValue["mm_event_broadcast12"] = aie1::mm_event_broadcast12;
   regNameToValue["mm_event_broadcast13"] = aie1::mm_event_broadcast13;
   regNameToValue["mm_event_broadcast14"] = aie1::mm_event_broadcast14;
   regNameToValue["mm_event_broadcast15"] = aie1::mm_event_broadcast15;
   regNameToValue["mm_event_broadcast2"] = aie1::mm_event_broadcast2;
   regNameToValue["mm_event_broadcast3"] = aie1::mm_event_broadcast3;
   regNameToValue["mm_event_broadcast4"] = aie1::mm_event_broadcast4;
   regNameToValue["mm_event_broadcast5"] = aie1::mm_event_broadcast5;
   regNameToValue["mm_event_broadcast6"] = aie1::mm_event_broadcast6;
   regNameToValue["mm_event_broadcast7"] = aie1::mm_event_broadcast7;
   regNameToValue["mm_event_broadcast8"] = aie1::mm_event_broadcast8;
   regNameToValue["mm_event_broadcast9"] = aie1::mm_event_broadcast9;
   regNameToValue["mm_event_broadcast_block_east_clr"] = aie1::mm_event_broadcast_block_east_clr;
   regNameToValue["mm_event_broadcast_block_east_set"] = aie1::mm_event_broadcast_block_east_set;
   regNameToValue["mm_event_broadcast_block_east_value"] = aie1::mm_event_broadcast_block_east_value;
   regNameToValue["mm_event_broadcast_block_north_clr"] = aie1::mm_event_broadcast_block_north_clr;
   regNameToValue["mm_event_broadcast_block_north_set"] = aie1::mm_event_broadcast_block_north_set;
   regNameToValue["mm_event_broadcast_block_north_value"] = aie1::mm_event_broadcast_block_north_value;
   regNameToValue["mm_event_broadcast_block_south_clr"] = aie1::mm_event_broadcast_block_south_clr;
   regNameToValue["mm_event_broadcast_block_south_set"] = aie1::mm_event_broadcast_block_south_set;
   regNameToValue["mm_event_broadcast_block_south_value"] = aie1::mm_event_broadcast_block_south_value;
   regNameToValue["mm_event_broadcast_block_west_clr"] = aie1::mm_event_broadcast_block_west_clr;
   regNameToValue["mm_event_broadcast_block_west_set"] = aie1::mm_event_broadcast_block_west_set;
   regNameToValue["mm_event_broadcast_block_west_value"] = aie1::mm_event_broadcast_block_west_value;
   regNameToValue["mm_event_generate"] = aie1::mm_event_generate;
   regNameToValue["mm_event_group_0_enable"] = aie1::mm_event_group_0_enable;
   regNameToValue["mm_event_group_broadcast_enable"] = aie1::mm_event_group_broadcast_enable;
   regNameToValue["mm_event_group_dma_enable"] = aie1::mm_event_group_dma_enable;
   regNameToValue["mm_event_group_error_enable"] = aie1::mm_event_group_error_enable;
   regNameToValue["mm_event_group_lock_enable"] = aie1::mm_event_group_lock_enable;
   regNameToValue["mm_event_group_memory_conflict_enable"] = aie1::mm_event_group_memory_conflict_enable;
   regNameToValue["mm_event_group_user_event_enable"] = aie1::mm_event_group_user_event_enable;
   regNameToValue["mm_event_group_watchpoint_enable"] = aie1::mm_event_group_watchpoint_enable;
   regNameToValue["mm_event_status0"] = aie1::mm_event_status0;
   regNameToValue["mm_event_status1"] = aie1::mm_event_status1;
   regNameToValue["mm_event_status2"] = aie1::mm_event_status2;
   regNameToValue["mm_event_status3"] = aie1::mm_event_status3;
   regNameToValue["mm_lock0_acquire_nv"] = aie1::mm_lock0_acquire_nv;
   regNameToValue["mm_lock0_acquire_v0"] = aie1::mm_lock0_acquire_v0;
   regNameToValue["mm_lock0_acquire_v1"] = aie1::mm_lock0_acquire_v1;
   regNameToValue["mm_lock0_release_nv"] = aie1::mm_lock0_release_nv;
   regNameToValue["mm_lock0_release_v0"] = aie1::mm_lock0_release_v0;
   regNameToValue["mm_lock0_release_v1"] = aie1::mm_lock0_release_v1;
   regNameToValue["mm_lock10_acquire_nv"] = aie1::mm_lock10_acquire_nv;
   regNameToValue["mm_lock10_acquire_v0"] = aie1::mm_lock10_acquire_v0;
   regNameToValue["mm_lock10_acquire_v1"] = aie1::mm_lock10_acquire_v1;
   regNameToValue["mm_lock10_release_nv"] = aie1::mm_lock10_release_nv;
   regNameToValue["mm_lock10_release_v0"] = aie1::mm_lock10_release_v0;
   regNameToValue["mm_lock10_release_v1"] = aie1::mm_lock10_release_v1;
   regNameToValue["mm_lock11_acquire_nv"] = aie1::mm_lock11_acquire_nv;
   regNameToValue["mm_lock11_acquire_v0"] = aie1::mm_lock11_acquire_v0;
   regNameToValue["mm_lock11_acquire_v1"] = aie1::mm_lock11_acquire_v1;
   regNameToValue["mm_lock11_release_nv"] = aie1::mm_lock11_release_nv;
   regNameToValue["mm_lock11_release_v0"] = aie1::mm_lock11_release_v0;
   regNameToValue["mm_lock11_release_v1"] = aie1::mm_lock11_release_v1;
   regNameToValue["mm_lock12_acquire_nv"] = aie1::mm_lock12_acquire_nv;
   regNameToValue["mm_lock12_acquire_v0"] = aie1::mm_lock12_acquire_v0;
   regNameToValue["mm_lock12_acquire_v1"] = aie1::mm_lock12_acquire_v1;
   regNameToValue["mm_lock12_release_nv"] = aie1::mm_lock12_release_nv;
   regNameToValue["mm_lock12_release_v0"] = aie1::mm_lock12_release_v0;
   regNameToValue["mm_lock12_release_v1"] = aie1::mm_lock12_release_v1;
   regNameToValue["mm_lock13_acquire_nv"] = aie1::mm_lock13_acquire_nv;
   regNameToValue["mm_lock13_acquire_v0"] = aie1::mm_lock13_acquire_v0;
   regNameToValue["mm_lock13_acquire_v1"] = aie1::mm_lock13_acquire_v1;
   regNameToValue["mm_lock13_release_nv"] = aie1::mm_lock13_release_nv;
   regNameToValue["mm_lock13_release_v0"] = aie1::mm_lock13_release_v0;
   regNameToValue["mm_lock13_release_v1"] = aie1::mm_lock13_release_v1;
   regNameToValue["mm_lock14_acquire_nv"] = aie1::mm_lock14_acquire_nv;
   regNameToValue["mm_lock14_acquire_v0"] = aie1::mm_lock14_acquire_v0;
   regNameToValue["mm_lock14_acquire_v1"] = aie1::mm_lock14_acquire_v1;
   regNameToValue["mm_lock14_release_nv"] = aie1::mm_lock14_release_nv;
   regNameToValue["mm_lock14_release_v0"] = aie1::mm_lock14_release_v0;
   regNameToValue["mm_lock14_release_v1"] = aie1::mm_lock14_release_v1;
   regNameToValue["mm_lock15_acquire_nv"] = aie1::mm_lock15_acquire_nv;
   regNameToValue["mm_lock15_acquire_v0"] = aie1::mm_lock15_acquire_v0;
   regNameToValue["mm_lock15_acquire_v1"] = aie1::mm_lock15_acquire_v1;
   regNameToValue["mm_lock15_release_nv"] = aie1::mm_lock15_release_nv;
   regNameToValue["mm_lock15_release_v0"] = aie1::mm_lock15_release_v0;
   regNameToValue["mm_lock15_release_v1"] = aie1::mm_lock15_release_v1;
   regNameToValue["mm_lock1_acquire_nv"] = aie1::mm_lock1_acquire_nv;
   regNameToValue["mm_lock1_acquire_v0"] = aie1::mm_lock1_acquire_v0;
   regNameToValue["mm_lock1_acquire_v1"] = aie1::mm_lock1_acquire_v1;
   regNameToValue["mm_lock1_release_nv"] = aie1::mm_lock1_release_nv;
   regNameToValue["mm_lock1_release_v0"] = aie1::mm_lock1_release_v0;
   regNameToValue["mm_lock1_release_v1"] = aie1::mm_lock1_release_v1;
   regNameToValue["mm_lock2_acquire_nv"] = aie1::mm_lock2_acquire_nv;
   regNameToValue["mm_lock2_acquire_v0"] = aie1::mm_lock2_acquire_v0;
   regNameToValue["mm_lock2_acquire_v1"] = aie1::mm_lock2_acquire_v1;
   regNameToValue["mm_lock2_release_nv"] = aie1::mm_lock2_release_nv;
   regNameToValue["mm_lock2_release_v0"] = aie1::mm_lock2_release_v0;
   regNameToValue["mm_lock2_release_v1"] = aie1::mm_lock2_release_v1;
   regNameToValue["mm_lock3_acquire_nv"] = aie1::mm_lock3_acquire_nv;
   regNameToValue["mm_lock3_acquire_v0"] = aie1::mm_lock3_acquire_v0;
   regNameToValue["mm_lock3_acquire_v1"] = aie1::mm_lock3_acquire_v1;
   regNameToValue["mm_lock3_release_nv"] = aie1::mm_lock3_release_nv;
   regNameToValue["mm_lock3_release_v0"] = aie1::mm_lock3_release_v0;
   regNameToValue["mm_lock3_release_v1"] = aie1::mm_lock3_release_v1;
   regNameToValue["mm_lock4_acquire_nv"] = aie1::mm_lock4_acquire_nv;
   regNameToValue["mm_lock4_acquire_v0"] = aie1::mm_lock4_acquire_v0;
   regNameToValue["mm_lock4_acquire_v1"] = aie1::mm_lock4_acquire_v1;
   regNameToValue["mm_lock4_release_nv"] = aie1::mm_lock4_release_nv;
   regNameToValue["mm_lock4_release_v0"] = aie1::mm_lock4_release_v0;
   regNameToValue["mm_lock4_release_v1"] = aie1::mm_lock4_release_v1;
   regNameToValue["mm_lock5_acquire_nv"] = aie1::mm_lock5_acquire_nv;
   regNameToValue["mm_lock5_acquire_v0"] = aie1::mm_lock5_acquire_v0;
   regNameToValue["mm_lock5_acquire_v1"] = aie1::mm_lock5_acquire_v1;
   regNameToValue["mm_lock5_release_nv"] = aie1::mm_lock5_release_nv;
   regNameToValue["mm_lock5_release_v0"] = aie1::mm_lock5_release_v0;
   regNameToValue["mm_lock5_release_v1"] = aie1::mm_lock5_release_v1;
   regNameToValue["mm_lock6_acquire_nv"] = aie1::mm_lock6_acquire_nv;
   regNameToValue["mm_lock6_acquire_v0"] = aie1::mm_lock6_acquire_v0;
   regNameToValue["mm_lock6_acquire_v1"] = aie1::mm_lock6_acquire_v1;
   regNameToValue["mm_lock6_release_nv"] = aie1::mm_lock6_release_nv;
   regNameToValue["mm_lock6_release_v0"] = aie1::mm_lock6_release_v0;
   regNameToValue["mm_lock6_release_v1"] = aie1::mm_lock6_release_v1;
   regNameToValue["mm_lock7_acquire_nv"] = aie1::mm_lock7_acquire_nv;
   regNameToValue["mm_lock7_acquire_v0"] = aie1::mm_lock7_acquire_v0;
   regNameToValue["mm_lock7_acquire_v1"] = aie1::mm_lock7_acquire_v1;
   regNameToValue["mm_lock7_release_nv"] = aie1::mm_lock7_release_nv;
   regNameToValue["mm_lock7_release_v0"] = aie1::mm_lock7_release_v0;
   regNameToValue["mm_lock7_release_v1"] = aie1::mm_lock7_release_v1;
   regNameToValue["mm_lock8_acquire_nv"] = aie1::mm_lock8_acquire_nv;
   regNameToValue["mm_lock8_acquire_v0"] = aie1::mm_lock8_acquire_v0;
   regNameToValue["mm_lock8_acquire_v1"] = aie1::mm_lock8_acquire_v1;
   regNameToValue["mm_lock8_release_nv"] = aie1::mm_lock8_release_nv;
   regNameToValue["mm_lock8_release_v0"] = aie1::mm_lock8_release_v0;
   regNameToValue["mm_lock8_release_v1"] = aie1::mm_lock8_release_v1;
   regNameToValue["mm_lock9_acquire_nv"] = aie1::mm_lock9_acquire_nv;
   regNameToValue["mm_lock9_acquire_v0"] = aie1::mm_lock9_acquire_v0;
   regNameToValue["mm_lock9_acquire_v1"] = aie1::mm_lock9_acquire_v1;
   regNameToValue["mm_lock9_release_nv"] = aie1::mm_lock9_release_nv;
   regNameToValue["mm_lock9_release_v0"] = aie1::mm_lock9_release_v0;
   regNameToValue["mm_lock9_release_v1"] = aie1::mm_lock9_release_v1;
   regNameToValue["mm_lock_event_value_control_0"] = aie1::mm_lock_event_value_control_0;
   regNameToValue["mm_lock_event_value_control_1"] = aie1::mm_lock_event_value_control_1;
   regNameToValue["mm_parity_failing_address"] = aie1::mm_parity_failing_address;
   regNameToValue["mm_performance_control0"] = aie1::mm_performance_control0;
   regNameToValue["mm_performance_control1"] = aie1::mm_performance_control1;
   regNameToValue["mm_performance_counter0"] = aie1::mm_performance_counter0;
   regNameToValue["mm_performance_counter0_event_value"] = aie1::mm_performance_counter0_event_value;
   regNameToValue["mm_performance_counter1"] = aie1::mm_performance_counter1;
   regNameToValue["mm_performance_counter1_event_value"] = aie1::mm_performance_counter1_event_value;
   regNameToValue["mm_reserved0"] = aie1::mm_reserved0;
   regNameToValue["mm_reserved1"] = aie1::mm_reserved1;
   regNameToValue["mm_reserved2"] = aie1::mm_reserved2;
   regNameToValue["mm_reserved3"] = aie1::mm_reserved3;
   regNameToValue["mm_reset_control"] = aie1::mm_reset_control;
   regNameToValue["mm_spare_reg"] = aie1::mm_spare_reg;
   regNameToValue["mm_timer_control"] = aie1::mm_timer_control;
   regNameToValue["mm_timer_high"] = aie1::mm_timer_high;
   regNameToValue["mm_timer_low"] = aie1::mm_timer_low;
   regNameToValue["mm_timer_trig_event_high_value"] = aie1::mm_timer_trig_event_high_value;
   regNameToValue["mm_timer_trig_event_low_value"] = aie1::mm_timer_trig_event_low_value;
   regNameToValue["mm_trace_control0"] = aie1::mm_trace_control0;
   regNameToValue["mm_trace_control1"] = aie1::mm_trace_control1;
   regNameToValue["mm_trace_event0"] = aie1::mm_trace_event0;
   regNameToValue["mm_trace_event1"] = aie1::mm_trace_event1;
   regNameToValue["mm_trace_status"] = aie1::mm_trace_status;
   regNameToValue["mm_watchpoint0"] = aie1::mm_watchpoint0;
   regNameToValue["mm_watchpoint1"] = aie1::mm_watchpoint1;
   regNameToValue["shim_all_lock_state_value"] = aie1::shim_all_lock_state_value;
   regNameToValue["shim_bisr_cache_ctrl"] = aie1::shim_bisr_cache_ctrl;
   regNameToValue["shim_bisr_cache_data0"] = aie1::shim_bisr_cache_data0;
   regNameToValue["shim_bisr_cache_data1"] = aie1::shim_bisr_cache_data1;
   regNameToValue["shim_bisr_cache_data2"] = aie1::shim_bisr_cache_data2;
   regNameToValue["shim_bisr_cache_data3"] = aie1::shim_bisr_cache_data3;
   regNameToValue["shim_bisr_cache_status"] = aie1::shim_bisr_cache_status;
   regNameToValue["shim_bisr_test_data0"] = aie1::shim_bisr_test_data0;
   regNameToValue["shim_bisr_test_data1"] = aie1::shim_bisr_test_data1;
   regNameToValue["shim_bisr_test_data2"] = aie1::shim_bisr_test_data2;
   regNameToValue["shim_bisr_test_data3"] = aie1::shim_bisr_test_data3;
   regNameToValue["shim_cssd_trigger"] = aie1::shim_cssd_trigger;
   regNameToValue["shim_combo_event_control"] = aie1::shim_combo_event_control;
   regNameToValue["shim_combo_event_inputs"] = aie1::shim_combo_event_inputs;
   regNameToValue["shim_control_packet_handler_status"] = aie1::shim_control_packet_handler_status;
   regNameToValue["shim_dma_bd0_axi_config"] = aie1::shim_dma_bd0_axi_config;
   regNameToValue["shim_dma_bd0_addr_low"] = aie1::shim_dma_bd0_addr_low;
   regNameToValue["shim_dma_bd0_buffer_length"] = aie1::shim_dma_bd0_buffer_length;
   regNameToValue["shim_dma_bd0_control"] = aie1::shim_dma_bd0_control;
   regNameToValue["shim_dma_bd0_packet"] = aie1::shim_dma_bd0_packet;
   regNameToValue["shim_dma_bd10_axi_config"] = aie1::shim_dma_bd10_axi_config;
   regNameToValue["shim_dma_bd10_addr_low"] = aie1::shim_dma_bd10_addr_low;
   regNameToValue["shim_dma_bd10_buffer_control"] = aie1::shim_dma_bd10_buffer_control;
   regNameToValue["shim_dma_bd10_buffer_length"] = aie1::shim_dma_bd10_buffer_length;
   regNameToValue["shim_dma_bd10_packet"] = aie1::shim_dma_bd10_packet;
   regNameToValue["shim_dma_bd11_axi_config"] = aie1::shim_dma_bd11_axi_config;
   regNameToValue["shim_dma_bd11_addr_low"] = aie1::shim_dma_bd11_addr_low;
   regNameToValue["shim_dma_bd11_buffer_control"] = aie1::shim_dma_bd11_buffer_control;
   regNameToValue["shim_dma_bd11_buffer_length"] = aie1::shim_dma_bd11_buffer_length;
   regNameToValue["shim_dma_bd11_packet"] = aie1::shim_dma_bd11_packet;
   regNameToValue["shim_dma_bd12_axi_config"] = aie1::shim_dma_bd12_axi_config;
   regNameToValue["shim_dma_bd12_addr_low"] = aie1::shim_dma_bd12_addr_low;
   regNameToValue["shim_dma_bd12_buffer_control"] = aie1::shim_dma_bd12_buffer_control;
   regNameToValue["shim_dma_bd12_buffer_length"] = aie1::shim_dma_bd12_buffer_length;
   regNameToValue["shim_dma_bd12_packet"] = aie1::shim_dma_bd12_packet;
   regNameToValue["shim_dma_bd13_axi_config"] = aie1::shim_dma_bd13_axi_config;
   regNameToValue["shim_dma_bd13_addr_low"] = aie1::shim_dma_bd13_addr_low;
   regNameToValue["shim_dma_bd13_buffer_control"] = aie1::shim_dma_bd13_buffer_control;
   regNameToValue["shim_dma_bd13_buffer_length"] = aie1::shim_dma_bd13_buffer_length;
   regNameToValue["shim_dma_bd13_packet"] = aie1::shim_dma_bd13_packet;
   regNameToValue["shim_dma_bd14_axi_config"] = aie1::shim_dma_bd14_axi_config;
   regNameToValue["shim_dma_bd14_addr_low"] = aie1::shim_dma_bd14_addr_low;
   regNameToValue["shim_dma_bd14_buffer_control"] = aie1::shim_dma_bd14_buffer_control;
   regNameToValue["shim_dma_bd14_buffer_length"] = aie1::shim_dma_bd14_buffer_length;
   regNameToValue["shim_dma_bd14_packet"] = aie1::shim_dma_bd14_packet;
   regNameToValue["shim_dma_bd15_axi_config"] = aie1::shim_dma_bd15_axi_config;
   regNameToValue["shim_dma_bd15_addr_low"] = aie1::shim_dma_bd15_addr_low;
   regNameToValue["shim_dma_bd15_buffer_control"] = aie1::shim_dma_bd15_buffer_control;
   regNameToValue["shim_dma_bd15_buffer_length"] = aie1::shim_dma_bd15_buffer_length;
   regNameToValue["shim_dma_bd15_packet"] = aie1::shim_dma_bd15_packet;
   regNameToValue["shim_dma_bd1_axi_config"] = aie1::shim_dma_bd1_axi_config;
   regNameToValue["shim_dma_bd1_addr_low"] = aie1::shim_dma_bd1_addr_low;
   regNameToValue["shim_dma_bd1_buffer_control"] = aie1::shim_dma_bd1_buffer_control;
   regNameToValue["shim_dma_bd1_buffer_length"] = aie1::shim_dma_bd1_buffer_length;
   regNameToValue["shim_dma_bd1_packet"] = aie1::shim_dma_bd1_packet;
   regNameToValue["shim_dma_bd2_axi_config"] = aie1::shim_dma_bd2_axi_config;
   regNameToValue["shim_dma_bd2_addr_low"] = aie1::shim_dma_bd2_addr_low;
   regNameToValue["shim_dma_bd2_buffer_control"] = aie1::shim_dma_bd2_buffer_control;
   regNameToValue["shim_dma_bd2_buffer_length"] = aie1::shim_dma_bd2_buffer_length;
   regNameToValue["shim_dma_bd2_packet"] = aie1::shim_dma_bd2_packet;
   regNameToValue["shim_dma_bd3_axi_config"] = aie1::shim_dma_bd3_axi_config;
   regNameToValue["shim_dma_bd3_addr_low"] = aie1::shim_dma_bd3_addr_low;
   regNameToValue["shim_dma_bd3_buffer_control"] = aie1::shim_dma_bd3_buffer_control;
   regNameToValue["shim_dma_bd3_buffer_length"] = aie1::shim_dma_bd3_buffer_length;
   regNameToValue["shim_dma_bd3_packet"] = aie1::shim_dma_bd3_packet;
   regNameToValue["shim_dma_bd4_axi_config"] = aie1::shim_dma_bd4_axi_config;
   regNameToValue["shim_dma_bd4_addr_low"] = aie1::shim_dma_bd4_addr_low;
   regNameToValue["shim_dma_bd4_buffer_control"] = aie1::shim_dma_bd4_buffer_control;
   regNameToValue["shim_dma_bd4_buffer_length"] = aie1::shim_dma_bd4_buffer_length;
   regNameToValue["shim_dma_bd4_packet"] = aie1::shim_dma_bd4_packet;
   regNameToValue["shim_dma_bd5_axi_config"] = aie1::shim_dma_bd5_axi_config;
   regNameToValue["shim_dma_bd5_addr_low"] = aie1::shim_dma_bd5_addr_low;
   regNameToValue["shim_dma_bd5_buffer_control"] = aie1::shim_dma_bd5_buffer_control;
   regNameToValue["shim_dma_bd5_buffer_length"] = aie1::shim_dma_bd5_buffer_length;
   regNameToValue["shim_dma_bd5_packet"] = aie1::shim_dma_bd5_packet;
   regNameToValue["shim_dma_bd6_axi_config"] = aie1::shim_dma_bd6_axi_config;
   regNameToValue["shim_dma_bd6_addr_low"] = aie1::shim_dma_bd6_addr_low;
   regNameToValue["shim_dma_bd6_buffer_control"] = aie1::shim_dma_bd6_buffer_control;
   regNameToValue["shim_dma_bd6_buffer_length"] = aie1::shim_dma_bd6_buffer_length;
   regNameToValue["shim_dma_bd6_packet"] = aie1::shim_dma_bd6_packet;
   regNameToValue["shim_dma_bd7_axi_config"] = aie1::shim_dma_bd7_axi_config;
   regNameToValue["shim_dma_bd7_addr_low"] = aie1::shim_dma_bd7_addr_low;
   regNameToValue["shim_dma_bd7_buffer_control"] = aie1::shim_dma_bd7_buffer_control;
   regNameToValue["shim_dma_bd7_buffer_length"] = aie1::shim_dma_bd7_buffer_length;
   regNameToValue["shim_dma_bd7_packet"] = aie1::shim_dma_bd7_packet;
   regNameToValue["shim_dma_bd8_axi_config"] = aie1::shim_dma_bd8_axi_config;
   regNameToValue["shim_dma_bd8_addr_low"] = aie1::shim_dma_bd8_addr_low;
   regNameToValue["shim_dma_bd8_buffer_control"] = aie1::shim_dma_bd8_buffer_control;
   regNameToValue["shim_dma_bd8_buffer_length"] = aie1::shim_dma_bd8_buffer_length;
   regNameToValue["shim_dma_bd8_packet"] = aie1::shim_dma_bd8_packet;
   regNameToValue["shim_dma_bd9_axi_config"] = aie1::shim_dma_bd9_axi_config;
   regNameToValue["shim_dma_bd9_addr_low"] = aie1::shim_dma_bd9_addr_low;
   regNameToValue["shim_dma_bd9_buffer_control"] = aie1::shim_dma_bd9_buffer_control;
   regNameToValue["shim_dma_bd9_buffer_length"] = aie1::shim_dma_bd9_buffer_length;
   regNameToValue["shim_dma_bd9_packet"] = aie1::shim_dma_bd9_packet;
   regNameToValue["shim_dma_mm2s_0_ctrl"] = aie1::shim_dma_mm2s_0_ctrl;
   regNameToValue["shim_dma_mm2s_0_start_queue"] = aie1::shim_dma_mm2s_0_start_queue;
   regNameToValue["shim_dma_mm2s_1_ctrl"] = aie1::shim_dma_mm2s_1_ctrl;
   regNameToValue["shim_dma_mm2s_1_start_queue"] = aie1::shim_dma_mm2s_1_start_queue;
   regNameToValue["shim_dma_mm2s_status"] = aie1::shim_dma_mm2s_status;
   regNameToValue["shim_dma_s2mm_0_ctrl"] = aie1::shim_dma_s2mm_0_ctrl;
   regNameToValue["shim_dma_s2mm_0_start_queue"] = aie1::shim_dma_s2mm_0_start_queue;
   regNameToValue["shim_dma_s2mm_1_ctrl"] = aie1::shim_dma_s2mm_1_ctrl;
   regNameToValue["shim_dma_s2mm_1_start_queue"] = aie1::shim_dma_s2mm_1_start_queue;
   regNameToValue["shim_dma_s2mm_status"] = aie1::shim_dma_s2mm_status;
   regNameToValue["shim_demux_config"] = aie1::shim_demux_config;
   regNameToValue["shim_event_broadcast_a_0"] = aie1::shim_event_broadcast_a_0;
   regNameToValue["shim_event_broadcast_a_10"] = aie1::shim_event_broadcast_a_10;
   regNameToValue["shim_event_broadcast_a_11"] = aie1::shim_event_broadcast_a_11;
   regNameToValue["shim_event_broadcast_a_12"] = aie1::shim_event_broadcast_a_12;
   regNameToValue["shim_event_broadcast_a_13"] = aie1::shim_event_broadcast_a_13;
   regNameToValue["shim_event_broadcast_a_14"] = aie1::shim_event_broadcast_a_14;
   regNameToValue["shim_event_broadcast_a_15"] = aie1::shim_event_broadcast_a_15;
   regNameToValue["shim_event_broadcast_a_1"] = aie1::shim_event_broadcast_a_1;
   regNameToValue["shim_event_broadcast_a_2"] = aie1::shim_event_broadcast_a_2;
   regNameToValue["shim_event_broadcast_a_3"] = aie1::shim_event_broadcast_a_3;
   regNameToValue["shim_event_broadcast_a_4"] = aie1::shim_event_broadcast_a_4;
   regNameToValue["shim_event_broadcast_a_5"] = aie1::shim_event_broadcast_a_5;
   regNameToValue["shim_event_broadcast_a_6"] = aie1::shim_event_broadcast_a_6;
   regNameToValue["shim_event_broadcast_a_7"] = aie1::shim_event_broadcast_a_7;
   regNameToValue["shim_event_broadcast_a_8"] = aie1::shim_event_broadcast_a_8;
   regNameToValue["shim_event_broadcast_a_9"] = aie1::shim_event_broadcast_a_9;
   regNameToValue["shim_event_broadcast_a_block_east_clr"] = aie1::shim_event_broadcast_a_block_east_clr;
   regNameToValue["shim_event_broadcast_a_block_east_set"] = aie1::shim_event_broadcast_a_block_east_set;
   regNameToValue["shim_event_broadcast_a_block_east_value"] = aie1::shim_event_broadcast_a_block_east_value;
   regNameToValue["shim_event_broadcast_a_block_north_clr"] = aie1::shim_event_broadcast_a_block_north_clr;
   regNameToValue["shim_event_broadcast_a_block_north_set"] = aie1::shim_event_broadcast_a_block_north_set;
   regNameToValue["shim_event_broadcast_a_block_north_value"] = aie1::shim_event_broadcast_a_block_north_value;
   regNameToValue["shim_event_broadcast_a_block_south_clr"] = aie1::shim_event_broadcast_a_block_south_clr;
   regNameToValue["shim_event_broadcast_a_block_south_set"] = aie1::shim_event_broadcast_a_block_south_set;
   regNameToValue["shim_event_broadcast_a_block_south_value"] = aie1::shim_event_broadcast_a_block_south_value;
   regNameToValue["shim_event_broadcast_a_block_west_clr"] = aie1::shim_event_broadcast_a_block_west_clr;
   regNameToValue["shim_event_broadcast_a_block_west_set"] = aie1::shim_event_broadcast_a_block_west_set;
   regNameToValue["shim_event_broadcast_a_block_west_value"] = aie1::shim_event_broadcast_a_block_west_value;
   regNameToValue["shim_event_broadcast_b_block_east_clr"] = aie1::shim_event_broadcast_b_block_east_clr;
   regNameToValue["shim_event_broadcast_b_block_east_set"] = aie1::shim_event_broadcast_b_block_east_set;
   regNameToValue["shim_event_broadcast_b_block_east_value"] = aie1::shim_event_broadcast_b_block_east_value;
   regNameToValue["shim_event_broadcast_b_block_north_clr"] = aie1::shim_event_broadcast_b_block_north_clr;
   regNameToValue["shim_event_broadcast_b_block_north_set"] = aie1::shim_event_broadcast_b_block_north_set;
   regNameToValue["shim_event_broadcast_b_block_north_value"] = aie1::shim_event_broadcast_b_block_north_value;
   regNameToValue["shim_event_broadcast_b_block_south_clr"] = aie1::shim_event_broadcast_b_block_south_clr;
   regNameToValue["shim_event_broadcast_b_block_south_set"] = aie1::shim_event_broadcast_b_block_south_set;
   regNameToValue["shim_event_broadcast_b_block_south_value"] = aie1::shim_event_broadcast_b_block_south_value;
   regNameToValue["shim_event_broadcast_b_block_west_clr"] = aie1::shim_event_broadcast_b_block_west_clr;
   regNameToValue["shim_event_broadcast_b_block_west_set"] = aie1::shim_event_broadcast_b_block_west_set;
   regNameToValue["shim_event_broadcast_b_block_west_value"] = aie1::shim_event_broadcast_b_block_west_value;
   regNameToValue["shim_event_generate"] = aie1::shim_event_generate;
   regNameToValue["shim_event_group_0_enable"] = aie1::shim_event_group_0_enable;
   regNameToValue["shim_event_group_broadcast_a_enable"] = aie1::shim_event_group_broadcast_a_enable;
   regNameToValue["shim_event_group_dma_enable"] = aie1::shim_event_group_dma_enable;
   regNameToValue["shim_event_group_errors_enable"] = aie1::shim_event_group_errors_enable;
   regNameToValue["shim_event_group_lock_enable"] = aie1::shim_event_group_lock_enable;
   regNameToValue["shim_event_group_stream_switch_enable"] = aie1::shim_event_group_stream_switch_enable;
   regNameToValue["shim_event_group_user_enable"] = aie1::shim_event_group_user_enable;
   regNameToValue["shim_event_status0"] = aie1::shim_event_status0;
   regNameToValue["shim_event_status1"] = aie1::shim_event_status1;
   regNameToValue["shim_event_status2"] = aie1::shim_event_status2;
   regNameToValue["shim_event_status3"] = aie1::shim_event_status3;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_a_clear"] = aie1::shim_interrupt_controller_1st_level_block_north_in_a_clear;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_a_set"] = aie1::shim_interrupt_controller_1st_level_block_north_in_a_set;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_a_value"] = aie1::shim_interrupt_controller_1st_level_block_north_in_a_value;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_b_clear"] = aie1::shim_interrupt_controller_1st_level_block_north_in_b_clear;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_b_set"] = aie1::shim_interrupt_controller_1st_level_block_north_in_b_set;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_b_value"] = aie1::shim_interrupt_controller_1st_level_block_north_in_b_value;
   regNameToValue["shim_interrupt_controller_1st_level_disable_a"] = aie1::shim_interrupt_controller_1st_level_disable_a;
   regNameToValue["shim_interrupt_controller_1st_level_disable_b"] = aie1::shim_interrupt_controller_1st_level_disable_b;
   regNameToValue["shim_interrupt_controller_1st_level_enable_a"] = aie1::shim_interrupt_controller_1st_level_enable_a;
   regNameToValue["shim_interrupt_controller_1st_level_enable_b"] = aie1::shim_interrupt_controller_1st_level_enable_b;
   regNameToValue["shim_interrupt_controller_1st_level_irq_event_a"] = aie1::shim_interrupt_controller_1st_level_irq_event_a;
   regNameToValue["shim_interrupt_controller_1st_level_irq_event_b"] = aie1::shim_interrupt_controller_1st_level_irq_event_b;
   regNameToValue["shim_interrupt_controller_1st_level_irq_no_a"] = aie1::shim_interrupt_controller_1st_level_irq_no_a;
   regNameToValue["shim_interrupt_controller_1st_level_irq_no_b"] = aie1::shim_interrupt_controller_1st_level_irq_no_b;
   regNameToValue["shim_interrupt_controller_1st_level_mask_a"] = aie1::shim_interrupt_controller_1st_level_mask_a;
   regNameToValue["shim_interrupt_controller_1st_level_mask_b"] = aie1::shim_interrupt_controller_1st_level_mask_b;
   regNameToValue["shim_interrupt_controller_1st_level_status_a"] = aie1::shim_interrupt_controller_1st_level_status_a;
   regNameToValue["shim_interrupt_controller_1st_level_status_b"] = aie1::shim_interrupt_controller_1st_level_status_b;
   regNameToValue["shim_interrupt_controller_2nd_level_disable"] = aie1::shim_interrupt_controller_2nd_level_disable;
   regNameToValue["shim_interrupt_controller_2nd_level_enable"] = aie1::shim_interrupt_controller_2nd_level_enable;
   regNameToValue["shim_interrupt_controller_2nd_level_interrupt"] = aie1::shim_interrupt_controller_2nd_level_interrupt;
   regNameToValue["shim_interrupt_controller_2nd_level_mask"] = aie1::shim_interrupt_controller_2nd_level_mask;
   regNameToValue["shim_interrupt_controller_2nd_level_status"] = aie1::shim_interrupt_controller_2nd_level_status;
   regNameToValue["shim_lock0_acquire_nv"] = aie1::shim_lock0_acquire_nv;
   regNameToValue["shim_lock0_acquire_v0"] = aie1::shim_lock0_acquire_v0;
   regNameToValue["shim_lock0_acquire_v1"] = aie1::shim_lock0_acquire_v1;
   regNameToValue["shim_lock0_release_nv"] = aie1::shim_lock0_release_nv;
   regNameToValue["shim_lock0_release_v0"] = aie1::shim_lock0_release_v0;
   regNameToValue["shim_lock0_release_v1"] = aie1::shim_lock0_release_v1;
   regNameToValue["shim_lock10_acquire_nv"] = aie1::shim_lock10_acquire_nv;
   regNameToValue["shim_lock10_acquire_v0"] = aie1::shim_lock10_acquire_v0;
   regNameToValue["shim_lock10_acquire_v1"] = aie1::shim_lock10_acquire_v1;
   regNameToValue["shim_lock10_release_nv"] = aie1::shim_lock10_release_nv;
   regNameToValue["shim_lock10_release_v0"] = aie1::shim_lock10_release_v0;
   regNameToValue["shim_lock10_release_v1"] = aie1::shim_lock10_release_v1;
   regNameToValue["shim_lock11_acquire_nv"] = aie1::shim_lock11_acquire_nv;
   regNameToValue["shim_lock11_acquire_v0"] = aie1::shim_lock11_acquire_v0;
   regNameToValue["shim_lock11_acquire_v1"] = aie1::shim_lock11_acquire_v1;
   regNameToValue["shim_lock11_release_nv"] = aie1::shim_lock11_release_nv;
   regNameToValue["shim_lock11_release_v0"] = aie1::shim_lock11_release_v0;
   regNameToValue["shim_lock11_release_v1"] = aie1::shim_lock11_release_v1;
   regNameToValue["shim_lock12_acquire_nv"] = aie1::shim_lock12_acquire_nv;
   regNameToValue["shim_lock12_acquire_v0"] = aie1::shim_lock12_acquire_v0;
   regNameToValue["shim_lock12_acquire_v1"] = aie1::shim_lock12_acquire_v1;
   regNameToValue["shim_lock12_release_nv"] = aie1::shim_lock12_release_nv;
   regNameToValue["shim_lock12_release_v0"] = aie1::shim_lock12_release_v0;
   regNameToValue["shim_lock12_release_v1"] = aie1::shim_lock12_release_v1;
   regNameToValue["shim_lock13_acquire_nv"] = aie1::shim_lock13_acquire_nv;
   regNameToValue["shim_lock13_acquire_v0"] = aie1::shim_lock13_acquire_v0;
   regNameToValue["shim_lock13_acquire_v1"] = aie1::shim_lock13_acquire_v1;
   regNameToValue["shim_lock13_release_nv"] = aie1::shim_lock13_release_nv;
   regNameToValue["shim_lock13_release_v0"] = aie1::shim_lock13_release_v0;
   regNameToValue["shim_lock13_release_v1"] = aie1::shim_lock13_release_v1;
   regNameToValue["shim_lock14_acquire_nv"] = aie1::shim_lock14_acquire_nv;
   regNameToValue["shim_lock14_acquire_v0"] = aie1::shim_lock14_acquire_v0;
   regNameToValue["shim_lock14_acquire_v1"] = aie1::shim_lock14_acquire_v1;
   regNameToValue["shim_lock14_release_nv"] = aie1::shim_lock14_release_nv;
   regNameToValue["shim_lock14_release_v0"] = aie1::shim_lock14_release_v0;
   regNameToValue["shim_lock14_release_v1"] = aie1::shim_lock14_release_v1;
   regNameToValue["shim_lock15_acquire_nv"] = aie1::shim_lock15_acquire_nv;
   regNameToValue["shim_lock15_acquire_v0"] = aie1::shim_lock15_acquire_v0;
   regNameToValue["shim_lock15_acquire_v1"] = aie1::shim_lock15_acquire_v1;
   regNameToValue["shim_lock15_release_nv"] = aie1::shim_lock15_release_nv;
   regNameToValue["shim_lock15_release_v0"] = aie1::shim_lock15_release_v0;
   regNameToValue["shim_lock15_release_v1"] = aie1::shim_lock15_release_v1;
   regNameToValue["shim_lock1_acquire_nv"] = aie1::shim_lock1_acquire_nv;
   regNameToValue["shim_lock1_acquire_v0"] = aie1::shim_lock1_acquire_v0;
   regNameToValue["shim_lock1_acquire_v1"] = aie1::shim_lock1_acquire_v1;
   regNameToValue["shim_lock1_release_nv"] = aie1::shim_lock1_release_nv;
   regNameToValue["shim_lock1_release_v0"] = aie1::shim_lock1_release_v0;
   regNameToValue["shim_lock1_release_v1"] = aie1::shim_lock1_release_v1;
   regNameToValue["shim_lock2_acquire_nv"] = aie1::shim_lock2_acquire_nv;
   regNameToValue["shim_lock2_acquire_v0"] = aie1::shim_lock2_acquire_v0;
   regNameToValue["shim_lock2_acquire_v1"] = aie1::shim_lock2_acquire_v1;
   regNameToValue["shim_lock2_release_nv"] = aie1::shim_lock2_release_nv;
   regNameToValue["shim_lock2_release_v0"] = aie1::shim_lock2_release_v0;
   regNameToValue["shim_lock2_release_v1"] = aie1::shim_lock2_release_v1;
   regNameToValue["shim_lock3_acquire_nv"] = aie1::shim_lock3_acquire_nv;
   regNameToValue["shim_lock3_acquire_v0"] = aie1::shim_lock3_acquire_v0;
   regNameToValue["shim_lock3_acquire_v1"] = aie1::shim_lock3_acquire_v1;
   regNameToValue["shim_lock3_release_nv"] = aie1::shim_lock3_release_nv;
   regNameToValue["shim_lock3_release_v0"] = aie1::shim_lock3_release_v0;
   regNameToValue["shim_lock3_release_v1"] = aie1::shim_lock3_release_v1;
   regNameToValue["shim_lock4_acquire_nv"] = aie1::shim_lock4_acquire_nv;
   regNameToValue["shim_lock4_acquire_v0"] = aie1::shim_lock4_acquire_v0;
   regNameToValue["shim_lock4_acquire_v1"] = aie1::shim_lock4_acquire_v1;
   regNameToValue["shim_lock4_release_nv"] = aie1::shim_lock4_release_nv;
   regNameToValue["shim_lock4_release_v0"] = aie1::shim_lock4_release_v0;
   regNameToValue["shim_lock4_release_v1"] = aie1::shim_lock4_release_v1;
   regNameToValue["shim_lock5_acquire_nv"] = aie1::shim_lock5_acquire_nv;
   regNameToValue["shim_lock5_acquire_v0"] = aie1::shim_lock5_acquire_v0;
   regNameToValue["shim_lock5_acquire_v1"] = aie1::shim_lock5_acquire_v1;
   regNameToValue["shim_lock5_release_nv"] = aie1::shim_lock5_release_nv;
   regNameToValue["shim_lock5_release_v0"] = aie1::shim_lock5_release_v0;
   regNameToValue["shim_lock5_release_v1"] = aie1::shim_lock5_release_v1;
   regNameToValue["shim_lock6_acquire_nv"] = aie1::shim_lock6_acquire_nv;
   regNameToValue["shim_lock6_acquire_v0"] = aie1::shim_lock6_acquire_v0;
   regNameToValue["shim_lock6_acquire_v1"] = aie1::shim_lock6_acquire_v1;
   regNameToValue["shim_lock6_release_nv"] = aie1::shim_lock6_release_nv;
   regNameToValue["shim_lock6_release_v0"] = aie1::shim_lock6_release_v0;
   regNameToValue["shim_lock6_release_v1"] = aie1::shim_lock6_release_v1;
   regNameToValue["shim_lock7_acquire_nv"] = aie1::shim_lock7_acquire_nv;
   regNameToValue["shim_lock7_acquire_v0"] = aie1::shim_lock7_acquire_v0;
   regNameToValue["shim_lock7_acquire_v1"] = aie1::shim_lock7_acquire_v1;
   regNameToValue["shim_lock7_release_nv"] = aie1::shim_lock7_release_nv;
   regNameToValue["shim_lock7_release_v0"] = aie1::shim_lock7_release_v0;
   regNameToValue["shim_lock7_release_v1"] = aie1::shim_lock7_release_v1;
   regNameToValue["shim_lock8_acquire_nv"] = aie1::shim_lock8_acquire_nv;
   regNameToValue["shim_lock8_acquire_v0"] = aie1::shim_lock8_acquire_v0;
   regNameToValue["shim_lock8_acquire_v1"] = aie1::shim_lock8_acquire_v1;
   regNameToValue["shim_lock8_release_nv"] = aie1::shim_lock8_release_nv;
   regNameToValue["shim_lock8_release_v0"] = aie1::shim_lock8_release_v0;
   regNameToValue["shim_lock8_release_v1"] = aie1::shim_lock8_release_v1;
   regNameToValue["shim_lock9_acquire_nv"] = aie1::shim_lock9_acquire_nv;
   regNameToValue["shim_lock9_acquire_v0"] = aie1::shim_lock9_acquire_v0;
   regNameToValue["shim_lock9_acquire_v1"] = aie1::shim_lock9_acquire_v1;
   regNameToValue["shim_lock9_release_nv"] = aie1::shim_lock9_release_nv;
   regNameToValue["shim_lock9_release_v0"] = aie1::shim_lock9_release_v0;
   regNameToValue["shim_lock9_release_v1"] = aie1::shim_lock9_release_v1;
   regNameToValue["shim_lock_event_value_control_0"] = aie1::shim_lock_event_value_control_0;
   regNameToValue["shim_lock_event_value_control_1"] = aie1::shim_lock_event_value_control_1;
   regNameToValue["shim_me_aximm_config"] = aie1::shim_me_aximm_config;
   regNameToValue["shim_me_shim_reset_enable"] = aie1::shim_me_shim_reset_enable;
   regNameToValue["shim_me_tile_column_reset"] = aie1::shim_me_tile_column_reset;
   regNameToValue["shim_mux_config"] = aie1::shim_mux_config;
   regNameToValue["shim_noc_interface_me_to_noc_south2"] = aie1::shim_noc_interface_me_to_noc_south2;
   regNameToValue["shim_noc_interface_me_to_noc_south3"] = aie1::shim_noc_interface_me_to_noc_south3;
   regNameToValue["shim_noc_interface_me_to_noc_south4"] = aie1::shim_noc_interface_me_to_noc_south4;
   regNameToValue["shim_noc_interface_me_to_noc_south5"] = aie1::shim_noc_interface_me_to_noc_south5;
   regNameToValue["shim_pl_interface_downsizer_bypass"] = aie1::shim_pl_interface_downsizer_bypass;
   regNameToValue["shim_pl_interface_downsizer_config"] = aie1::shim_pl_interface_downsizer_config;
   regNameToValue["shim_pl_interface_downsizer_enable"] = aie1::shim_pl_interface_downsizer_enable;
   regNameToValue["shim_pl_interface_upsizer_config"] = aie1::shim_pl_interface_upsizer_config;
   regNameToValue["shim_performance_counter0"] = aie1::shim_performance_counter0;
   regNameToValue["shim_performance_counter0_event_value"] = aie1::shim_performance_counter0_event_value;
   regNameToValue["shim_performance_counter1"] = aie1::shim_performance_counter1;
   regNameToValue["shim_performance_counter1_event_value"] = aie1::shim_performance_counter1_event_value;
   regNameToValue["shim_performance_control0"] = aie1::shim_performance_control0;
   regNameToValue["shim_performance_start_stop_0_1"] = aie1::shim_performance_start_stop_0_1;
   regNameToValue["shim_performance_control1"] = aie1::shim_performance_control1;
   regNameToValue["shim_performance_reset_0_1"] = aie1::shim_performance_reset_0_1;
   regNameToValue["shim_reserved0"] = aie1::shim_reserved0;
   regNameToValue["shim_reserved1"] = aie1::shim_reserved1;
   regNameToValue["shim_reserved2"] = aie1::shim_reserved2;
   regNameToValue["shim_reserved3"] = aie1::shim_reserved3;
   regNameToValue["shim_spare_reg"] = aie1::shim_spare_reg;
   regNameToValue["shim_spare_reg"] = aie1::shim_spare_reg;
   regNameToValue["shim_stream_switch_event_port_selection_0"] = aie1::shim_stream_switch_event_port_selection_0;
   regNameToValue["shim_stream_switch_event_port_selection_1"] = aie1::shim_stream_switch_event_port_selection_1;
   regNameToValue["shim_stream_switch_master_config_east0"] = aie1::shim_stream_switch_master_config_east0;
   regNameToValue["shim_stream_switch_master_config_east1"] = aie1::shim_stream_switch_master_config_east1;
   regNameToValue["shim_stream_switch_master_config_east2"] = aie1::shim_stream_switch_master_config_east2;
   regNameToValue["shim_stream_switch_master_config_east3"] = aie1::shim_stream_switch_master_config_east3;
   regNameToValue["shim_stream_switch_master_config_fifo0"] = aie1::shim_stream_switch_master_config_fifo0;
   regNameToValue["shim_stream_switch_master_config_fifo1"] = aie1::shim_stream_switch_master_config_fifo1;
   regNameToValue["shim_stream_switch_master_config_north0"] = aie1::shim_stream_switch_master_config_north0;
   regNameToValue["shim_stream_switch_master_config_north1"] = aie1::shim_stream_switch_master_config_north1;
   regNameToValue["shim_stream_switch_master_config_north2"] = aie1::shim_stream_switch_master_config_north2;
   regNameToValue["shim_stream_switch_master_config_north3"] = aie1::shim_stream_switch_master_config_north3;
   regNameToValue["shim_stream_switch_master_config_north4"] = aie1::shim_stream_switch_master_config_north4;
   regNameToValue["shim_stream_switch_master_config_north5"] = aie1::shim_stream_switch_master_config_north5;
   regNameToValue["shim_stream_switch_master_config_south0"] = aie1::shim_stream_switch_master_config_south0;
   regNameToValue["shim_stream_switch_master_config_south1"] = aie1::shim_stream_switch_master_config_south1;
   regNameToValue["shim_stream_switch_master_config_south2"] = aie1::shim_stream_switch_master_config_south2;
   regNameToValue["shim_stream_switch_master_config_south3"] = aie1::shim_stream_switch_master_config_south3;
   regNameToValue["shim_stream_switch_master_config_south4"] = aie1::shim_stream_switch_master_config_south4;
   regNameToValue["shim_stream_switch_master_config_south5"] = aie1::shim_stream_switch_master_config_south5;
   regNameToValue["shim_stream_switch_master_config_tile_ctrl"] = aie1::shim_stream_switch_master_config_tile_ctrl;
   regNameToValue["shim_stream_switch_master_config_west0"] = aie1::shim_stream_switch_master_config_west0;
   regNameToValue["shim_stream_switch_master_config_west1"] = aie1::shim_stream_switch_master_config_west1;
   regNameToValue["shim_stream_switch_master_config_west2"] = aie1::shim_stream_switch_master_config_west2;
   regNameToValue["shim_stream_switch_master_config_west3"] = aie1::shim_stream_switch_master_config_west3;
   regNameToValue["shim_stream_switch_slave_east_0_config"] = aie1::shim_stream_switch_slave_east_0_config;
   regNameToValue["shim_stream_switch_slave_east_0_slot0"] = aie1::shim_stream_switch_slave_east_0_slot0;
   regNameToValue["shim_stream_switch_slave_east_0_slot1"] = aie1::shim_stream_switch_slave_east_0_slot1;
   regNameToValue["shim_stream_switch_slave_east_0_slot2"] = aie1::shim_stream_switch_slave_east_0_slot2;
   regNameToValue["shim_stream_switch_slave_east_0_slot3"] = aie1::shim_stream_switch_slave_east_0_slot3;
   regNameToValue["shim_stream_switch_slave_east_1_config"] = aie1::shim_stream_switch_slave_east_1_config;
   regNameToValue["shim_stream_switch_slave_east_1_slot0"] = aie1::shim_stream_switch_slave_east_1_slot0;
   regNameToValue["shim_stream_switch_slave_east_1_slot1"] = aie1::shim_stream_switch_slave_east_1_slot1;
   regNameToValue["shim_stream_switch_slave_east_1_slot2"] = aie1::shim_stream_switch_slave_east_1_slot2;
   regNameToValue["shim_stream_switch_slave_east_1_slot3"] = aie1::shim_stream_switch_slave_east_1_slot3;
   regNameToValue["shim_stream_switch_slave_east_2_config"] = aie1::shim_stream_switch_slave_east_2_config;
   regNameToValue["shim_stream_switch_slave_east_2_slot0"] = aie1::shim_stream_switch_slave_east_2_slot0;
   regNameToValue["shim_stream_switch_slave_east_2_slot1"] = aie1::shim_stream_switch_slave_east_2_slot1;
   regNameToValue["shim_stream_switch_slave_east_2_slot2"] = aie1::shim_stream_switch_slave_east_2_slot2;
   regNameToValue["shim_stream_switch_slave_east_2_slot3"] = aie1::shim_stream_switch_slave_east_2_slot3;
   regNameToValue["shim_stream_switch_slave_east_3_config"] = aie1::shim_stream_switch_slave_east_3_config;
   regNameToValue["shim_stream_switch_slave_east_3_slot0"] = aie1::shim_stream_switch_slave_east_3_slot0;
   regNameToValue["shim_stream_switch_slave_east_3_slot1"] = aie1::shim_stream_switch_slave_east_3_slot1;
   regNameToValue["shim_stream_switch_slave_east_3_slot2"] = aie1::shim_stream_switch_slave_east_3_slot2;
   regNameToValue["shim_stream_switch_slave_east_3_slot3"] = aie1::shim_stream_switch_slave_east_3_slot3;
   regNameToValue["shim_stream_switch_slave_fifo_0_config"] = aie1::shim_stream_switch_slave_fifo_0_config;
   regNameToValue["shim_stream_switch_slave_fifo_0_slot0"] = aie1::shim_stream_switch_slave_fifo_0_slot0;
   regNameToValue["shim_stream_switch_slave_fifo_0_slot1"] = aie1::shim_stream_switch_slave_fifo_0_slot1;
   regNameToValue["shim_stream_switch_slave_fifo_0_slot2"] = aie1::shim_stream_switch_slave_fifo_0_slot2;
   regNameToValue["shim_stream_switch_slave_fifo_0_slot3"] = aie1::shim_stream_switch_slave_fifo_0_slot3;
   regNameToValue["shim_stream_switch_slave_fifo_1_config"] = aie1::shim_stream_switch_slave_fifo_1_config;
   regNameToValue["shim_stream_switch_slave_fifo_1_slot0"] = aie1::shim_stream_switch_slave_fifo_1_slot0;
   regNameToValue["shim_stream_switch_slave_fifo_1_slot1"] = aie1::shim_stream_switch_slave_fifo_1_slot1;
   regNameToValue["shim_stream_switch_slave_fifo_1_slot2"] = aie1::shim_stream_switch_slave_fifo_1_slot2;
   regNameToValue["shim_stream_switch_slave_fifo_1_slot3"] = aie1::shim_stream_switch_slave_fifo_1_slot3;
   regNameToValue["shim_stream_switch_slave_north_0_config"] = aie1::shim_stream_switch_slave_north_0_config;
   regNameToValue["shim_stream_switch_slave_north_0_slot0"] = aie1::shim_stream_switch_slave_north_0_slot0;
   regNameToValue["shim_stream_switch_slave_north_0_slot1"] = aie1::shim_stream_switch_slave_north_0_slot1;
   regNameToValue["shim_stream_switch_slave_north_0_slot2"] = aie1::shim_stream_switch_slave_north_0_slot2;
   regNameToValue["shim_stream_switch_slave_north_0_slot3"] = aie1::shim_stream_switch_slave_north_0_slot3;
   regNameToValue["shim_stream_switch_slave_north_1_config"] = aie1::shim_stream_switch_slave_north_1_config;
   regNameToValue["shim_stream_switch_slave_north_1_slot0"] = aie1::shim_stream_switch_slave_north_1_slot0;
   regNameToValue["shim_stream_switch_slave_north_1_slot1"] = aie1::shim_stream_switch_slave_north_1_slot1;
   regNameToValue["shim_stream_switch_slave_north_1_slot2"] = aie1::shim_stream_switch_slave_north_1_slot2;
   regNameToValue["shim_stream_switch_slave_north_1_slot3"] = aie1::shim_stream_switch_slave_north_1_slot3;
   regNameToValue["shim_stream_switch_slave_north_2_config"] = aie1::shim_stream_switch_slave_north_2_config;
   regNameToValue["shim_stream_switch_slave_north_2_slot0"] = aie1::shim_stream_switch_slave_north_2_slot0;
   regNameToValue["shim_stream_switch_slave_north_2_slot1"] = aie1::shim_stream_switch_slave_north_2_slot1;
   regNameToValue["shim_stream_switch_slave_north_2_slot2"] = aie1::shim_stream_switch_slave_north_2_slot2;
   regNameToValue["shim_stream_switch_slave_north_2_slot3"] = aie1::shim_stream_switch_slave_north_2_slot3;
   regNameToValue["shim_stream_switch_slave_north_3_config"] = aie1::shim_stream_switch_slave_north_3_config;
   regNameToValue["shim_stream_switch_slave_north_3_slot0"] = aie1::shim_stream_switch_slave_north_3_slot0;
   regNameToValue["shim_stream_switch_slave_north_3_slot1"] = aie1::shim_stream_switch_slave_north_3_slot1;
   regNameToValue["shim_stream_switch_slave_north_3_slot2"] = aie1::shim_stream_switch_slave_north_3_slot2;
   regNameToValue["shim_stream_switch_slave_north_3_slot3"] = aie1::shim_stream_switch_slave_north_3_slot3;
   regNameToValue["shim_stream_switch_slave_south_0_config"] = aie1::shim_stream_switch_slave_south_0_config;
   regNameToValue["shim_stream_switch_slave_south_0_slot0"] = aie1::shim_stream_switch_slave_south_0_slot0;
   regNameToValue["shim_stream_switch_slave_south_0_slot1"] = aie1::shim_stream_switch_slave_south_0_slot1;
   regNameToValue["shim_stream_switch_slave_south_0_slot2"] = aie1::shim_stream_switch_slave_south_0_slot2;
   regNameToValue["shim_stream_switch_slave_south_0_slot3"] = aie1::shim_stream_switch_slave_south_0_slot3;
   regNameToValue["shim_stream_switch_slave_south_1_config"] = aie1::shim_stream_switch_slave_south_1_config;
   regNameToValue["shim_stream_switch_slave_south_1_slot0"] = aie1::shim_stream_switch_slave_south_1_slot0;
   regNameToValue["shim_stream_switch_slave_south_1_slot1"] = aie1::shim_stream_switch_slave_south_1_slot1;
   regNameToValue["shim_stream_switch_slave_south_1_slot2"] = aie1::shim_stream_switch_slave_south_1_slot2;
   regNameToValue["shim_stream_switch_slave_south_1_slot3"] = aie1::shim_stream_switch_slave_south_1_slot3;
   regNameToValue["shim_stream_switch_slave_south_2_config"] = aie1::shim_stream_switch_slave_south_2_config;
   regNameToValue["shim_stream_switch_slave_south_2_slot0"] = aie1::shim_stream_switch_slave_south_2_slot0;
   regNameToValue["shim_stream_switch_slave_south_2_slot1"] = aie1::shim_stream_switch_slave_south_2_slot1;
   regNameToValue["shim_stream_switch_slave_south_2_slot2"] = aie1::shim_stream_switch_slave_south_2_slot2;
   regNameToValue["shim_stream_switch_slave_south_2_slot3"] = aie1::shim_stream_switch_slave_south_2_slot3;
   regNameToValue["shim_stream_switch_slave_south_3_config"] = aie1::shim_stream_switch_slave_south_3_config;
   regNameToValue["shim_stream_switch_slave_south_3_slot0"] = aie1::shim_stream_switch_slave_south_3_slot0;
   regNameToValue["shim_stream_switch_slave_south_3_slot1"] = aie1::shim_stream_switch_slave_south_3_slot1;
   regNameToValue["shim_stream_switch_slave_south_3_slot2"] = aie1::shim_stream_switch_slave_south_3_slot2;
   regNameToValue["shim_stream_switch_slave_south_3_slot3"] = aie1::shim_stream_switch_slave_south_3_slot3;
   regNameToValue["shim_stream_switch_slave_south_4_config"] = aie1::shim_stream_switch_slave_south_4_config;
   regNameToValue["shim_stream_switch_slave_south_4_slot0"] = aie1::shim_stream_switch_slave_south_4_slot0;
   regNameToValue["shim_stream_switch_slave_south_4_slot1"] = aie1::shim_stream_switch_slave_south_4_slot1;
   regNameToValue["shim_stream_switch_slave_south_4_slot2"] = aie1::shim_stream_switch_slave_south_4_slot2;
   regNameToValue["shim_stream_switch_slave_south_4_slot3"] = aie1::shim_stream_switch_slave_south_4_slot3;
   regNameToValue["shim_stream_switch_slave_south_5_config"] = aie1::shim_stream_switch_slave_south_5_config;
   regNameToValue["shim_stream_switch_slave_south_5_slot0"] = aie1::shim_stream_switch_slave_south_5_slot0;
   regNameToValue["shim_stream_switch_slave_south_5_slot1"] = aie1::shim_stream_switch_slave_south_5_slot1;
   regNameToValue["shim_stream_switch_slave_south_5_slot2"] = aie1::shim_stream_switch_slave_south_5_slot2;
   regNameToValue["shim_stream_switch_slave_south_5_slot3"] = aie1::shim_stream_switch_slave_south_5_slot3;
   regNameToValue["shim_stream_switch_slave_south_6_config"] = aie1::shim_stream_switch_slave_south_6_config;
   regNameToValue["shim_stream_switch_slave_south_6_slot0"] = aie1::shim_stream_switch_slave_south_6_slot0;
   regNameToValue["shim_stream_switch_slave_south_6_slot1"] = aie1::shim_stream_switch_slave_south_6_slot1;
   regNameToValue["shim_stream_switch_slave_south_6_slot2"] = aie1::shim_stream_switch_slave_south_6_slot2;
   regNameToValue["shim_stream_switch_slave_south_6_slot3"] = aie1::shim_stream_switch_slave_south_6_slot3;
   regNameToValue["shim_stream_switch_slave_south_7_config"] = aie1::shim_stream_switch_slave_south_7_config;
   regNameToValue["shim_stream_switch_slave_south_7_slot0"] = aie1::shim_stream_switch_slave_south_7_slot0;
   regNameToValue["shim_stream_switch_slave_south_7_slot1"] = aie1::shim_stream_switch_slave_south_7_slot1;
   regNameToValue["shim_stream_switch_slave_south_7_slot2"] = aie1::shim_stream_switch_slave_south_7_slot2;
   regNameToValue["shim_stream_switch_slave_south_7_slot3"] = aie1::shim_stream_switch_slave_south_7_slot3;
   regNameToValue["shim_stream_switch_slave_tile_ctrl_config"] = aie1::shim_stream_switch_slave_tile_ctrl_config;
   regNameToValue["shim_stream_switch_slave_tile_ctrl_slot0"] = aie1::shim_stream_switch_slave_tile_ctrl_slot0;
   regNameToValue["shim_stream_switch_slave_tile_ctrl_slot1"] = aie1::shim_stream_switch_slave_tile_ctrl_slot1;
   regNameToValue["shim_stream_switch_slave_tile_ctrl_slot2"] = aie1::shim_stream_switch_slave_tile_ctrl_slot2;
   regNameToValue["shim_stream_switch_slave_tile_ctrl_slot3"] = aie1::shim_stream_switch_slave_tile_ctrl_slot3;
   regNameToValue["shim_stream_switch_slave_trace_config"] = aie1::shim_stream_switch_slave_trace_config;
   regNameToValue["shim_stream_switch_slave_trace_slot0"] = aie1::shim_stream_switch_slave_trace_slot0;
   regNameToValue["shim_stream_switch_slave_trace_slot1"] = aie1::shim_stream_switch_slave_trace_slot1;
   regNameToValue["shim_stream_switch_slave_trace_slot2"] = aie1::shim_stream_switch_slave_trace_slot2;
   regNameToValue["shim_stream_switch_slave_trace_slot3"] = aie1::shim_stream_switch_slave_trace_slot3;
   regNameToValue["shim_stream_switch_slave_west_0_config"] = aie1::shim_stream_switch_slave_west_0_config;
   regNameToValue["shim_stream_switch_slave_west_0_slot0"] = aie1::shim_stream_switch_slave_west_0_slot0;
   regNameToValue["shim_stream_switch_slave_west_0_slot1"] = aie1::shim_stream_switch_slave_west_0_slot1;
   regNameToValue["shim_stream_switch_slave_west_0_slot2"] = aie1::shim_stream_switch_slave_west_0_slot2;
   regNameToValue["shim_stream_switch_slave_west_0_slot3"] = aie1::shim_stream_switch_slave_west_0_slot3;
   regNameToValue["shim_stream_switch_slave_west_1_config"] = aie1::shim_stream_switch_slave_west_1_config;
   regNameToValue["shim_stream_switch_slave_west_1_slot0"] = aie1::shim_stream_switch_slave_west_1_slot0;
   regNameToValue["shim_stream_switch_slave_west_1_slot1"] = aie1::shim_stream_switch_slave_west_1_slot1;
   regNameToValue["shim_stream_switch_slave_west_1_slot2"] = aie1::shim_stream_switch_slave_west_1_slot2;
   regNameToValue["shim_stream_switch_slave_west_1_slot3"] = aie1::shim_stream_switch_slave_west_1_slot3;
   regNameToValue["shim_stream_switch_slave_west_2_config"] = aie1::shim_stream_switch_slave_west_2_config;
   regNameToValue["shim_stream_switch_slave_west_2_slot0"] = aie1::shim_stream_switch_slave_west_2_slot0;
   regNameToValue["shim_stream_switch_slave_west_2_slot1"] = aie1::shim_stream_switch_slave_west_2_slot1;
   regNameToValue["shim_stream_switch_slave_west_2_slot2"] = aie1::shim_stream_switch_slave_west_2_slot2;
   regNameToValue["shim_stream_switch_slave_west_2_slot3"] = aie1::shim_stream_switch_slave_west_2_slot3;
   regNameToValue["shim_stream_switch_slave_west_3_config"] = aie1::shim_stream_switch_slave_west_3_config;
   regNameToValue["shim_stream_switch_slave_west_3_slot0"] = aie1::shim_stream_switch_slave_west_3_slot0;
   regNameToValue["shim_stream_switch_slave_west_3_slot1"] = aie1::shim_stream_switch_slave_west_3_slot1;
   regNameToValue["shim_stream_switch_slave_west_3_slot2"] = aie1::shim_stream_switch_slave_west_3_slot2;
   regNameToValue["shim_stream_switch_slave_west_3_slot3"] = aie1::shim_stream_switch_slave_west_3_slot3;
   regNameToValue["shim_tile_clock_control"] = aie1::shim_tile_clock_control;
   regNameToValue["shim_tile_control"] = aie1::shim_tile_control;
   regNameToValue["shim_timer_control"] = aie1::shim_timer_control;
   regNameToValue["shim_timer_high"] = aie1::shim_timer_high;
   regNameToValue["shim_timer_low"] = aie1::shim_timer_low;
   regNameToValue["shim_timer_trig_event_high_value"] = aie1::shim_timer_trig_event_high_value;
   regNameToValue["shim_timer_trig_event_low_value"] = aie1::shim_timer_trig_event_low_value;
   regNameToValue["shim_trace_control0"] = aie1::shim_trace_control0;
   regNameToValue["shim_trace_control1"] = aie1::shim_trace_control1;
   regNameToValue["shim_trace_event0"] = aie1::shim_trace_event0;
   regNameToValue["shim_trace_event1"] = aie1::shim_trace_event1;
   regNameToValue["shim_trace_status"] = aie1::shim_trace_status;
   regNameToValue["shim_dma_bd_step_size"] = aie1::shim_dma_bd_step_size;
   regNameToValue["shim_dma_s2mm_step_size"] = aie1::shim_dma_s2mm_step_size;
   regNameToValue["mem_performance_counter0"] = aie1::mem_performance_counter0;
}
  /*
  void populateRegValueToNameMap() {
    regValueToName=  {
      {0x0003f054, "cm_stream_switch_master_config_east0"},
      {0x0003f058, "cm_stream_switch_master_config_east1"},
      {0x0003f05c, "cm_stream_switch_master_config_east2"},
      {0x0003f060, "cm_stream_switch_master_config_east3"},
      {0x0001ef20, "mm_lock_event_value_control_0"},
      {0x0001d110, "mm_dma_bd8_packet"},
      {0x00034208, "cm_event_status2"},
      {0x0003420c, "cm_event_status3"},
      {0x00034200, "cm_event_status0"},
      {0x00034204, "cm_event_status1"},
      {0x00034048, "cm_event_broadcast14"},
      {0x0003404c, "cm_event_broadcast15"},
      {0x00034040, "cm_event_broadcast12"},
      {0x00034044, "cm_event_broadcast13"},
      {0x00034038, "cm_event_broadcast10"},
      {0x0003403c, "cm_event_broadcast11"},
      {0x00034080, "cm_event_broadcast_block_east_set"},
      {0x0001d174, "mm_dma_bd11_interleaved_state"},
      {0x0001d1b4, "mm_dma_bd13_interleaved_state"},
      {0x0001d1d4, "mm_dma_bd14_interleaved_state"},
      {0x0001d1cc, "mm_dma_bd14_2d_y"},
      {0x0001d1c8, "mm_dma_bd14_2d_x"},
      {0x0001e070, "mm_lock0_acquire_v1"},
      {0x0001e060, "mm_lock0_acquire_v0"},
      {0x0001e4b0, "mm_lock9_release_v1"},
      {0x0001e4a0, "mm_lock9_release_v0"},
      {0x000340f4, "shim_timer_trig_event_high_value"},
      {0x0001d150, "mm_dma_bd10_packet"},
      {0x00034068, "shim_event_broadcast_a_block_west_value"},
      {0x00034504, "shim_event_group_dma_enable"},
      {0x0001d060, "mm_dma_bd3_addr_a"},
      {0x0001d064, "mm_dma_bd3_addr_b"},
      {0x0003450c, "cm_event_group_core_program_flow_enable"},
      {0x00014074, "mm_event_broadcast_block_north_clr"},
      {0x00014050, "mm_event_broadcast_block_south_set"},
      {0x0001e100, "mm_lock2_release_nv"},
      {0x0001e2b0, "mm_lock5_release_v1"},
      {0x0001e2a0, "mm_lock5_release_v0"},
      {0x0003f154, "cm_stream_switch_slave_config_east_0"},
      {0x0001d1c0, "mm_dma_bd14_addr_a"},
      {0x00014008, "mm_event_generate"},
      {0x0003f2f4, "cm_stream_switch_slave_west_2_slot1"},
      {0x0003f2f0, "cm_stream_switch_slave_west_2_slot0"},
      {0x0003f2fc, "cm_stream_switch_slave_west_2_slot3"},
      {0x0003f2f8, "cm_stream_switch_slave_west_2_slot2"},
      {0x00034014, "shim_event_broadcast_a_1"},
      {0x0001e320, "mm_lock6_release_v0"},
      {0x00034500, "cm_event_group_0_enable"},
      {0x0001d00c, "shim_dma_bd0_axi_config"},
      {0x00016000, "mm_spare_reg"},
      {0x0003f150, "cm_stream_switch_slave_config_north_3"},
      {0x0003f14c, "cm_stream_switch_slave_config_north_2"},
      {0x0003f148, "cm_stream_switch_slave_config_north_1"},
      {0x0003f144, "cm_stream_switch_slave_config_north_0"},
      {0x0001de14, "mm_dma_mm2s_0_start_queue"},
      {0x0001e420, "mm_lock8_release_v0"},
      {0x0001e430, "mm_lock8_release_v1"},
      {0x000140d8, "mm_trace_status"},
      {0x0001d190, "mm_dma_bd12_packet"},
      {0x0003f204, "cm_stream_switch_slave_aie_core0_slot1"},
      {0x0003f200, "cm_stream_switch_slave_aie_core0_slot0"},
      {0x0003f20c, "cm_stream_switch_slave_aie_core0_slot3"},
      {0x0003f208, "cm_stream_switch_slave_aie_core0_slot2"},
      {0x0001d08c, "mm_dma_bd4_2d_y"},
      {0x0001d088, "mm_dma_bd4_2d_x"},
      {0x0001451c, "mm_event_group_user_event_enable"},
      {0x00014000, "mm_timer_control"},
      {0x00031000, "shim_performance_control0"},
      {0x0003f388, "cm_stream_switch_slave_east_3_slot2"},
      {0x0001e560, "mm_lock10_acquire_v0"},
      {0x00034024, "cm_event_broadcast5"},
      {0x0003f380, "cm_stream_switch_slave_east_3_slot0"},
      {0x0003f384, "cm_stream_switch_slave_east_3_slot1"},
      {0x0001d168, "mm_dma_bd11_2d_x"},
      {0x0001d16c, "mm_dma_bd11_2d_y"},
      {0x00031080, "shim_performance_counter0_event_value"},
      {0x0001d114, "mm_dma_bd8_interleaved_state"},
      {0x0001de0c, "mm_dma_s2mm_1_start_queue"},
      {0x0001d0d0, "mm_dma_bd6_packet"},
      {0x00034508, "cm_event_group_core_stall_enable"},
      {0x0001d158, "mm_dma_bd10_control"},
      {0x00034510, "cm_event_group_errors0_enable"},
      {0x00034058, "shim_event_broadcast_a_block_south_value"},
      {0x0001e770, "mm_lock14_acquire_v1"},
      {0x0001e760, "mm_lock14_acquire_v0"},
      {0x00032030, "cm_error_halt_control"},
      {0x0001e6b0, "mm_lock13_release_v1"},
      {0x0001e6c0, "mm_lock13_acquire_nv"},
      {0x0001e0b0, "mm_lock1_release_v1"},
      {0x0001e0a0, "mm_lock1_release_v0"},
      {0x0001d004, "shim_dma_bd0_buffer_length"},
      {0x0003f110, "cm_stream_switch_slave_config_tile_ctrl"},
      {0x0001d0b0, "mm_dma_bd5_packet"},
      {0x00014404, "mm_combo_event_control"},
      {0x0003f334, "cm_stream_switch_slave_north_2_slot1"},
      {0x0003f338, "cm_stream_switch_slave_north_2_slot2"},
      {0x0003f33c, "cm_stream_switch_slave_north_2_slot3"},
      {0x0001d134, "mm_dma_bd9_interleaved_state"},
      {0x00031020, "shim_performance_counter0"},
      {0x00031024, "shim_performance_counter1"},
      {0x00014514, "mm_event_group_error_enable"},
      {0x00014508, "mm_event_group_dma_enable"},
      {0x000340f4, "cm_timer_trig_event_high_value"},
      {0x0001d140, "shim_dma_s2mm_0_ctrl"},
      {0x00031008, "shim_performance_control1"},
      {0x0001e780, "mm_lock15_release_nv"},
      {0x0003ff00, "cm_stream_switch_event_port_selection_0"},
      {0x0001d038, "mm_dma_bd1_control"},
      {0x0001e680, "mm_lock13_release_nv"},
      {0x00034050, "cm_event_broadcast_block_south_set"},
      {0x0001e330, "mm_lock6_release_v1"},
      {0x0001d080, "mm_dma_bd4_addr_a"},
      {0x0001d084, "mm_dma_bd4_addr_b"},
      {0x000340f0, "cm_timer_trig_event_low_value"},
      {0x00014058, "mm_event_broadcast_block_south_value"},
      {0x0003f354, "cm_stream_switch_slave_east_0_slot1"},
      {0x0003f350, "cm_stream_switch_slave_east_0_slot0"},
      {0x0003f35c, "cm_stream_switch_slave_east_0_slot3"},
      {0x0003f358, "cm_stream_switch_slave_east_0_slot2"},
      {0x0003f018, "cm_stream_switch_master_config_fifo1"},
      {0x0003f014, "cm_stream_switch_master_config_fifo0"},
      {0x0003f298, "cm_stream_switch_slave_south_2_slot2"},
      {0x0003f29c, "cm_stream_switch_slave_south_2_slot3"},
      {0x0003f290, "cm_stream_switch_slave_south_2_slot0"},
      {0x0003f294, "cm_stream_switch_slave_south_2_slot1"},
      {0x00014200, "mm_event_status0"},
      {0x00014204, "mm_event_status1"},
      {0x00014208, "mm_event_status2"},
      {0x0001420c, "mm_event_status3"},
      {0x00034088, "cm_event_broadcast_block_east_value"},
      {0x00011084, "mm_performance_counter1_event_value"},
      {0x00034058, "cm_event_broadcast_block_south_value"},
      {0x0003202c, "cm_pc_event3"},
      {0x0001d0d4, "mm_dma_bd6_interleaved_state"},
      {0x0003ff04, "cm_stream_switch_event_port_selection_1"},
      {0x0003f360, "cm_stream_switch_slave_east_1_slot0"},
      {0x0003f364, "cm_stream_switch_slave_east_1_slot1"},
      {0x0003f368, "cm_stream_switch_slave_east_1_slot2"},
      {0x0003f36c, "cm_stream_switch_slave_east_1_slot3"},
      {0x0001d070, "mm_dma_bd3_packet"},
      {0x0001d14c, "mm_dma_bd10_2d_y"},
      {0x0001d148, "mm_dma_bd10_2d_x"},
      {0x0001d1a4, "mm_dma_bd13_addr_b"},
      {0x0001d1a0, "mm_dma_bd13_addr_a"},
      {0x0001ef24, "mm_lock_event_value_control_1"},
      {0x0001d068, "mm_dma_bd3_2d_x"},
      {0x0001d06c, "mm_dma_bd3_2d_y"},
      {0x0001e340, "mm_lock6_acquire_nv"},
      {0x0001d058, "mm_dma_bd2_control"},
      {0x00031088, "cm_performance_counter2_event_value"},
      {0x000340f0, "shim_timer_trig_event_low_value"},
      {0x000140f4, "mm_timer_trig_event_high_value"},
      {0x0001d0d8, "mm_dma_bd6_control"},
      {0x00032034, "cm_error_halt_event"},
      {0x00034074, "shim_event_broadcast_a_block_north_clr"},
      {0x00014400, "mm_combo_event_inputs"},
      {0x0001e5b0, "mm_lock11_release_v1"},
      {0x0001e5a0, "mm_lock11_release_v0"},
      {0x0, "mem_performance_counter0"},
      {0x0003f100, "cm_stream_switch_slave_config_aie_core0"},
      {0x0003f040, "cm_stream_switch_master_config_north1"},
      {0x0003f03c, "cm_stream_switch_master_config_north0"},
      {0x0003f048, "cm_stream_switch_master_config_north3"},
      {0x0003f044, "cm_stream_switch_master_config_north2"},
      {0x0003f050, "cm_stream_switch_master_config_north5"},
      {0x0003f04c, "cm_stream_switch_master_config_north4"},
      {0x0001d1c4, "mm_dma_bd14_addr_b"},
      {0x8, "shim_dma_s2mm_step_size"},
      {0x0003f37c, "cm_stream_switch_slave_east_2_slot3"},
      {0x0003f378, "cm_stream_switch_slave_east_2_slot2"},
      {0x0003f374, "cm_stream_switch_slave_east_2_slot1"},
      {0x0003f370, "cm_stream_switch_slave_east_2_slot0"},
      {0x00014070, "mm_event_broadcast_block_north_set"},
      {0x0001de10, "mm_dma_mm2s_0_ctrl"},
      {0x0001d180, "mm_dma_bd12_addr_a"},
      {0x0001d0f0, "mm_dma_bd7_packet"},
      {0x0001e280, "mm_lock5_release_nv"},
      {0x0001e170, "mm_lock2_acquire_v1"},
      {0x0001e160, "mm_lock2_acquire_v0"},
      {0x0001e4c0, "mm_lock9_acquire_nv"},
      {0x0003f284, "cm_stream_switch_slave_south_1_slot1"},
      {0x0003f280, "cm_stream_switch_slave_south_1_slot0"},
      {0x0003f28c, "cm_stream_switch_slave_south_1_slot3"},
      {0x0003f288, "cm_stream_switch_slave_south_1_slot2"},
      {0x000140e0, "mm_trace_event0"},
      {0x000140e4, "mm_trace_event1"},
      {0x0003f21c, "cm_stream_switch_slave_aie_core1_slot3"},
      {0x0001df00, "mm_dma_s2mm_status"},
      {0x00034078, "cm_event_broadcast_block_north_value"},
      {0x00014060, "mm_event_broadcast_block_west_set"},
      {0x00034400, "cm_combo_event_inputs"},
      {0x0001e180, "mm_lock3_release_nv"},
      {0x0001450c, "mm_event_group_lock_enable"},
      {0x0003f164, "cm_stream_switch_slave_config_aie_trace"},
      {0x0001e7f0, "mm_lock15_acquire_v1"},
      {0x0001d078, "mm_dma_bd3_control"},
      {0x0003f240, "cm_stream_switch_slave_tile_ctrl_slot0"},
      {0x0003f244, "cm_stream_switch_slave_tile_ctrl_slot1"},
      {0x0003f248, "cm_stream_switch_slave_tile_ctrl_slot2"},
      {0x0003f24c, "cm_stream_switch_slave_tile_ctrl_slot3"},
      {0x00034070, "cm_event_broadcast_block_north_set"},
      {0x0003f158, "cm_stream_switch_slave_config_east_1"},
      {0x0003f15c, "cm_stream_switch_slave_config_east_2"},
      {0x0003f160, "cm_stream_switch_slave_config_east_3"},
      {0x00034008, "shim_event_generate"},
      {0x0001e400, "mm_lock8_release_nv"},
      {0x0001e3c0, "mm_lock7_acquire_nv"},
      {0x0001d18c, "mm_dma_bd12_2d_y"},
      {0x0001d188, "mm_dma_bd12_2d_x"},
      {0x0001d1f0, "mm_dma_bd15_packet"},
      {0x0001e040, "mm_lock0_acquire_nv"},
      {0x0001e540, "mm_lock10_acquire_nv"},
      {0x000140fc, "mm_timer_high"},
      {0x0003f3a4, "cm_stream_switch_slave_mem_trace_slot1"},
      {0x0003f12c, "cm_stream_switch_slave_config_south_4"},
      {0x0003f2c4, "cm_stream_switch_slave_south_5_slot1"},
      {0x0003f2c0, "cm_stream_switch_slave_south_5_slot0"},
      {0x0003f2cc, "cm_stream_switch_slave_south_5_slot3"},
      {0x0003f2c8, "cm_stream_switch_slave_south_5_slot2"},
      {0x0001d04c, "mm_dma_bd2_2d_y"},
      {0x000340d4, "cm_trace_control1"},
      {0x000340d0, "cm_trace_control0"},
      {0x0001d048, "mm_dma_bd2_2d_x"},
      {0x0003f210, "cm_stream_switch_slave_aie_core1_slot0"},
      {0x0003f214, "cm_stream_switch_slave_aie_core1_slot1"},
      {0x0003f218, "cm_stream_switch_slave_aie_core1_slot2"},
      {0x00014500, "mm_event_group_0_enable"},
      {0x0003f394, "cm_stream_switch_slave_aie_trace_slot1"},
      {0x0003f390, "cm_stream_switch_slave_aie_trace_slot0"},
      {0x0003f39c, "cm_stream_switch_slave_aie_trace_slot3"},
      {0x0003f398, "cm_stream_switch_slave_aie_trace_slot2"},
      {0x0001d10c, "mm_dma_bd8_2d_y"},
      {0x0001d108, "mm_dma_bd8_2d_x"},
      {0x0001d144, "shim_dma_s2mm_0_start_queue"},
      {0x0001e6e0, "mm_lock13_acquire_v0"},
      {0x0001e6f0, "mm_lock13_acquire_v1"},
      {0x0003f010, "cm_stream_switch_master_config_tile_ctrl"},
      {0x0001e7b0, "mm_lock15_release_v1"},
      {0x0001e7a0, "mm_lock15_release_v0"},
      {0x0001d024, "mm_dma_bd1_addr_b"},
      {0x0001d020, "mm_dma_bd1_addr_a"},
      {0x0001e360, "mm_lock6_acquire_v0"},
      {0x0001d028, "mm_dma_bd1_2d_x"},
      {0x0001d02c, "mm_dma_bd1_2d_y"},
      {0x0001d124, "mm_dma_bd9_addr_b"},
      {0x0001d120, "mm_dma_bd9_addr_a"},
      {0x0001e240, "mm_lock4_acquire_nv"},
      {0x00031024, "cm_performance_counter1"},
      {0x00031020, "cm_performance_counter0"},
      {0x0003102c, "cm_performance_counter3"},
      {0x00031028, "cm_performance_counter2"},
      {0x000340e0, "cm_trace_event0"},
      {0x000340e4, "cm_trace_event1"},
      {0x0001e080, "mm_lock1_release_nv"},
      {0x0001e1a0, "mm_lock3_release_v0"},
      {0x0003f2dc, "cm_stream_switch_slave_west_0_slot3"},
      {0x0003f2d8, "cm_stream_switch_slave_west_0_slot2"},
      {0x0003f2d4, "cm_stream_switch_slave_west_0_slot1"},
      {0x0003f2d0, "cm_stream_switch_slave_west_0_slot0"},
      {0x0001e300, "mm_lock6_release_nv"},
      {0x00034060, "cm_event_broadcast_block_west_set"},
      {0x00034080, "shim_event_broadcast_a_block_east_set"},
      {0x0001d094, "mm_dma_bd4_interleaved_state"},
      {0x0001e7e0, "mm_lock15_acquire_v0"},
      {0x00034084, "cm_event_broadcast_block_east_clr"},
      {0x0001e370, "mm_lock6_acquire_v1"},
      {0x00034064, "shim_event_broadcast_a_block_west_clr"},
      {0x0001e0e0, "mm_lock1_acquire_v0"},
      {0x0001e0f0, "mm_lock1_acquire_v1"},
      {0x0003f318, "cm_stream_switch_slave_north_0_slot2"},
      {0x0003f31c, "cm_stream_switch_slave_north_0_slot3"},
      {0x0003f310, "cm_stream_switch_slave_north_0_slot0"},
      {0x0003f314, "cm_stream_switch_slave_north_0_slot1"},
      {0x0003f2e8, "cm_stream_switch_slave_west_1_slot2"},
      {0x00011024, "mm_performance_counter1"},
      {0x00011020, "mm_performance_counter0"},
      {0x0003f2ec, "cm_stream_switch_slave_west_1_slot3"},
      {0x0003f2e0, "cm_stream_switch_slave_west_1_slot0"},
      {0x0001d000, "shim_dma_bd0_addr_low"},
      {0x0003f348, "cm_stream_switch_slave_north_3_slot2"},
      {0x0001d118, "mm_dma_bd8_control"},
      {0x00014068, "mm_event_broadcast_block_west_value"},
      {0x0001d100, "mm_dma_bd8_addr_a"},
      {0x0001d104, "mm_dma_bd8_addr_b"},
      {0x0001d0cc, "mm_dma_bd6_2d_y"},
      {0x0001d010, "mm_dma_bd0_packet"},
      {0x0001d0c8, "mm_dma_bd6_2d_x"},
      {0x0001e000, "mm_lock0_release_nv"},
      {0x0001d0ec, "mm_dma_bd7_2d_y"},
      {0x00014088, "mm_event_broadcast_block_east_value"},
      {0x0001d0b8, "mm_dma_bd5_control"},
      {0x00034000, "cm_timer_control"},
      {0x00032000, "cm_core_control"},
      {0x0001d130, "mm_dma_bd9_packet"},
      {0x0001e2e0, "mm_lock5_acquire_v0"},
      {0x0001e4e0, "mm_lock9_acquire_v0"},
      {0x0001e4f0, "mm_lock9_acquire_v1"},
      {0x00014054, "mm_event_broadcast_block_south_clr"},
      {0x00034084, "shim_event_broadcast_a_block_east_clr"},
      {0x0001e5f0, "mm_lock11_acquire_v1"},
      {0x00031084, "cm_performance_counter1_event_value"},
      {0x0001d1b0, "mm_dma_bd13_packet"},
      {0x0001df20, "mm_dma_fifo_counter"},
      {0x0001e7c0, "mm_lock15_acquire_nv"},
      {0x00036034, "cm_tile_control_packet_handler_status"},
      {0x00034030, "cm_event_broadcast8"},
      {0x00034034, "cm_event_broadcast9"},
      {0x0001e580, "mm_lock11_release_nv"},
      {0x00034020, "cm_event_broadcast4"},
      {0x0003f38c, "cm_stream_switch_slave_east_3_slot3"},
      {0x00034028, "cm_event_broadcast6"},
      {0x0003402c, "cm_event_broadcast7"},
      {0x00034010, "cm_event_broadcast0"},
      {0x00034014, "cm_event_broadcast1"},
      {0x00034018, "cm_event_broadcast2"},
      {0x0003401c, "cm_event_broadcast3"},
      {0x0001d0f4, "mm_dma_bd7_interleaved_state"},
      {0x0001e2c0, "mm_lock5_acquire_nv"},
      {0x0003f008, "cm_stream_switch_master_config_dma0"},
      {0x0003f00c, "cm_stream_switch_master_config_dma1"},
      {0x0001d1d0, "mm_dma_bd14_packet"},
      {0x0001d194, "mm_dma_bd12_interleaved_state"},
      {0x00034504, "cm_event_group_pc_enable"},
      {0x00034054, "cm_event_broadcast_block_south_clr"},
      {0x0003f034, "cm_stream_switch_master_config_west2"},
      {0x0003f038, "cm_stream_switch_master_config_west3"},
      {0x0003f02c, "cm_stream_switch_master_config_west0"},
      {0x0003f030, "cm_stream_switch_master_config_west1"},
      {0x0003f264, "cm_stream_switch_slave_fifo_1_slot1"},
      {0x0003f260, "cm_stream_switch_slave_fifo_1_slot0"},
      {0x0003f26c, "cm_stream_switch_slave_fifo_1_slot3"},
      {0x0003f268, "cm_stream_switch_slave_fifo_1_slot2"},
      {0x0001d184, "mm_dma_bd12_addr_b"},
      {0x0003420c, "shim_event_status3"},
      {0x00034208, "shim_event_status2"},
      {0x00034204, "shim_event_status1"},
      {0x00034200, "shim_event_status0"},
      {0x0001e140, "mm_lock2_acquire_nv"},
      {0x0001d1a8, "mm_dma_bd13_2d_x"},
      {0x0001d1ac, "mm_dma_bd13_2d_y"},
      {0x0003f130, "cm_stream_switch_slave_config_south_5"},
      {0x0003f3a0, "cm_stream_switch_slave_mem_trace_slot0"},
      {0x0003f3ac, "cm_stream_switch_slave_mem_trace_slot3"},
      {0x0003f3a8, "cm_stream_switch_slave_mem_trace_slot2"},
      {0x0003f120, "cm_stream_switch_slave_config_south_1"},
      {0x0003f11c, "cm_stream_switch_slave_config_south_0"},
      {0x0003f128, "cm_stream_switch_slave_config_south_3"},
      {0x0003f124, "cm_stream_switch_slave_config_south_2"},
      {0x0001d1f4, "mm_dma_bd15_interleaved_state"},
      {0x0001d160, "mm_dma_bd11_addr_a"},
      {0x0001e1b0, "mm_lock3_release_v1"},
      {0x00012110, "mm_ecc_scrubbing_event"},
      {0x0003f028, "cm_stream_switch_master_config_south3"},
      {0x0003f024, "cm_stream_switch_master_config_south2"},
      {0x0003f020, "cm_stream_switch_master_config_south1"},
      {0x0003f01c, "cm_stream_switch_master_config_south0"},
      {0x0001d074, "mm_dma_bd3_interleaved_state"},
      {0x00031084, "shim_performance_counter1_event_value"},
      {0x000340e4, "shim_trace_event1"},
      {0x0001d138, "mm_dma_bd9_control"},
      {0x000340e0, "shim_trace_event0"},
      {0x00013000, "mm_reset_control"},
      {0x00014100, "mm_watchpoint0"},
      {0x00014104, "mm_watchpoint1"},
      {0x0001421c, "mm_reserved3"},
      {0x00014218, "mm_reserved2"},
      {0x00014214, "mm_reserved1"},
      {0x00014210, "mm_reserved0"},
      {0x0001e270, "mm_lock4_acquire_v1"},
      {0x0001e260, "mm_lock4_acquire_v0"},
      {0x0001d144, "mm_dma_bd10_addr_b"},
      {0x0001d140, "mm_dma_bd10_addr_a"},
      {0x0001d0b4, "mm_dma_bd5_interleaved_state"},
      {0x00014038, "mm_event_broadcast10"},
      {0x0001403c, "mm_event_broadcast11"},
      {0x00014040, "mm_event_broadcast12"},
      {0x00014044, "mm_event_broadcast13"},
      {0x00014048, "mm_event_broadcast14"},
      {0x0001404c, "mm_event_broadcast15"},
      {0x0001d00c, "mm_dma_bd0_2d_y"},
      {0x0001d008, "mm_dma_bd0_2d_x"},
      {0x0003f168, "cm_stream_switch_slave_config_mem_trace"},
      {0x00034514, "cm_event_group_errors1_enable"},
      {0x0001e1c0, "mm_lock3_acquire_nv"},
      {0x0001d044, "mm_dma_bd2_addr_b"},
      {0x0001df10, "mm_dma_mm2s_status"},
      {0x0001d040, "mm_dma_bd2_addr_a"},
      {0x00031008, "cm_performance_control2"},
      {0x0001d054, "mm_dma_bd2_interleaved_state"},
      {0x0003f2ac, "cm_stream_switch_slave_south_3_slot3"},
      {0x0003f2a8, "cm_stream_switch_slave_south_3_slot2"},
      {0x0003f2a4, "cm_stream_switch_slave_south_3_slot1"},
      {0x0003f2a0, "cm_stream_switch_slave_south_3_slot0"},
      {0x0001e5c0, "mm_lock11_acquire_nv"},
      {0x0001e460, "mm_lock8_acquire_v0"},
      {0x000140d4, "mm_trace_control1"},
      {0x000140d0, "mm_trace_control0"},
      {0x0001e0c0, "mm_lock1_acquire_nv"},
      {0x0001e3e0, "mm_lock7_acquire_v0"},
      {0x0001e3b0, "mm_lock7_release_v1"},
      {0x0001e3a0, "mm_lock7_release_v0"},
      {0x00034050, "shim_event_broadcast_a_block_south_set"},
      {0x00034078, "shim_event_broadcast_a_block_north_value"},
      {0x00034064, "cm_event_broadcast_block_west_clr"},
      {0x00034054, "shim_event_broadcast_a_block_south_clr"},
      {0x0001e3f0, "mm_lock7_acquire_v1"},
      {0x0001d0e0, "mm_dma_bd7_addr_a"},
      {0x0001d0a4, "mm_dma_bd5_addr_b"},
      {0x0001d0a0, "mm_dma_bd5_addr_a"},
      {0x0001d0e4, "mm_dma_bd7_addr_b"},
      {0x000140f0, "mm_timer_trig_event_low_value"},
      {0x0001d1b8, "mm_dma_bd13_control"},
      {0x0003451c, "cm_event_group_broadcast_enable"},
      {0x00014084, "mm_event_broadcast_block_east_clr"},
      {0x00014064, "mm_event_broadcast_block_west_clr"},
      {0x0001e520, "mm_lock10_release_v0"},
      {0x0001e530, "mm_lock10_release_v1"},
      {0x0001e440, "mm_lock8_acquire_nv"},
      {0x0001d090, "mm_dma_bd4_packet"},
      {0x0001d000, "mm_dma_bd0_addr_a"},
      {0x0001d004, "mm_dma_bd0_addr_b"},
      {0x0003404c, "shim_event_broadcast_a_15"},
      {0x0003108c, "cm_performance_counter3_event_value"},
      {0x00011080, "mm_performance_counter0_event_value"},
      {0x0001ef00, "mm_all_lock_state_value"},
      {0x0003f108, "cm_stream_switch_slave_config_dma_0"},
      {0x0003f10c, "cm_stream_switch_slave_config_dma_1"},
      {0x00014510, "mm_event_group_memory_conflict_enable"},
      {0x00034068, "cm_event_broadcast_block_west_value"},
      {0x00034034, "shim_event_broadcast_a_9"},
      {0x00034030, "shim_event_broadcast_a_8"},
      {0x0003402c, "shim_event_broadcast_a_7"},
      {0x00034028, "shim_event_broadcast_a_6"},
      {0x00034024, "shim_event_broadcast_a_5"},
      {0x00034020, "shim_event_broadcast_a_4"},
      {0x0003401c, "shim_event_broadcast_a_3"},
      {0x00034018, "shim_event_broadcast_a_2"},
      {0x0001d198, "mm_dma_bd12_control"},
      {0x00034010, "shim_event_broadcast_a_0"},
      {0x0001d170, "mm_dma_bd11_packet"},
      {0x0001d008, "shim_dma_bd0_control"},
      {0x00036040, "cm_tile_clock_control"},
      {0x0001e220, "mm_lock4_release_v0"},
      {0x0001e230, "mm_lock4_release_v1"},
      {0x0001d014, "mm_dma_bd0_interleaved_state"},
      {0x0003f104, "cm_stream_switch_slave_config_aie_core1"},
      {0x0001d0c0, "mm_dma_bd6_addr_a"},
      {0x000340d8, "shim_trace_status"},
      {0x000340d8, "cm_trace_status"},
      {0x00012120, "mm_ecc_failing_address"},
      {0x0003f250, "cm_stream_switch_slave_fifo_0_slot0"},
      {0x0003f254, "cm_stream_switch_slave_fifo_0_slot1"},
      {0x0003f258, "cm_stream_switch_slave_fifo_0_slot2"},
      {0x0003f25c, "cm_stream_switch_slave_fifo_0_slot3"},
      {0x0003200c, "cm_reset_event"},
      {0x0001e700, "mm_lock14_release_nv"},
      {0x000140f8, "mm_timer_low"},
      {0x0001e020, "mm_lock0_release_v0"},
      {0x0001e030, "mm_lock0_release_v1"},
      {0x00014030, "mm_event_broadcast8"},
      {0x00014034, "mm_event_broadcast9"},
      {0x00014028, "mm_event_broadcast6"},
      {0x0001402c, "mm_event_broadcast7"},
      {0x00014020, "mm_event_broadcast4"},
      {0x00014024, "mm_event_broadcast5"},
      {0x00014018, "mm_event_broadcast2"},
      {0x0001401c, "mm_event_broadcast3"},
      {0x00014010, "mm_event_broadcast0"},
      {0x00014014, "mm_event_broadcast1"},
      {0x0001e470, "mm_lock8_acquire_v1"},
      {0x00012124, "mm_parity_failing_address"},
      {0x0003f2b0, "cm_stream_switch_slave_south_4_slot0"},
      {0x0003f2b4, "cm_stream_switch_slave_south_4_slot1"},
      {0x0003f2b8, "cm_stream_switch_slave_south_4_slot2"},
      {0x0003f2bc, "cm_stream_switch_slave_south_4_slot3"},
      {0x0003f238, "cm_stream_switch_slave_dma_1_slot2"},
      {0x0003f23c, "cm_stream_switch_slave_dma_1_slot3"},
      {0x0003f230, "cm_stream_switch_slave_dma_1_slot0"},
      {0x0003f234, "cm_stream_switch_slave_dma_1_slot1"},
      {0x0001d034, "mm_dma_bd1_interleaved_state"},
      {0x0001d164, "mm_dma_bd11_addr_b"},
      {0x00034008, "cm_event_generate"},
      {0x0001de18, "mm_dma_mm2s_1_ctrl"},
      {0x0001d178, "mm_dma_bd11_control"},
      {0x00031000, "cm_performance_control0"},
      {0x00031004, "cm_performance_control1"},
      {0x00030470, "cm_mc1"},
      {0x00030460, "cm_mc0"},
      {0x0001d0a8, "mm_dma_bd5_2d_x"},
      {0x0001d0ac, "mm_dma_bd5_2d_y"},
      {0x00034044, "shim_event_broadcast_a_13"},
      {0x00034040, "shim_event_broadcast_a_12"},
      {0x0003403c, "shim_event_broadcast_a_11"},
      {0x00034038, "shim_event_broadcast_a_10"},
      {0x0001e480, "mm_lock9_release_nv"},
      {0x0001de00, "mm_dma_s2mm_0_ctrl"},
      {0x00034048, "shim_event_broadcast_a_14"},
      {0x00012000, "mm_checkbit_error_generation"},
      {0x0001d1f8, "mm_dma_bd15_control"},
      {0x00034404, "cm_combo_event_control"},
      {0x0001e640, "mm_lock12_acquire_nv"},
      {0x00011000, "mm_performance_control0"},
      {0x00011008, "mm_performance_control1"},
      {0x00032018, "cm_debug_control2"},
      {0x00032014, "cm_debug_control1"},
      {0x00032010, "cm_debug_control0"},
      {0x0003f13c, "cm_stream_switch_slave_config_west_2"},
      {0x0003f140, "cm_stream_switch_slave_config_west_3"},
      {0x0003f134, "cm_stream_switch_slave_config_west_0"},
      {0x0003f138, "cm_stream_switch_slave_config_west_1"},
      {0x00014518, "mm_event_group_broadcast_enable"},
      {0x00034070, "shim_event_broadcast_a_block_north_set"},
      {0x0001e600, "mm_lock12_release_nv"},
      {0x00030440, "cm_md0"},
      {0x00030450, "cm_md1"},
      {0x0001e2f0, "mm_lock5_acquire_v1"},
      {0x0003ff04, "shim_stream_switch_event_port_selection_1"},
      {0x0001e1e0, "mm_lock3_acquire_v0"},
      {0x0001d050, "mm_dma_bd2_packet"},
      {0x0001e1f0, "mm_lock3_acquire_v1"},
      {0x0001d0c4, "mm_dma_bd6_addr_b"},
      {0x000340f8, "shim_timer_low"},
      {0x0001e200, "mm_lock4_release_nv"},
      {0x0003f32c, "cm_stream_switch_slave_north_1_slot3"},
      {0x0003f328, "cm_stream_switch_slave_north_1_slot2"},
      {0x0003f324, "cm_stream_switch_slave_north_1_slot1"},
      {0x0003f320, "cm_stream_switch_slave_north_1_slot0"},
      {0x00034088, "shim_event_broadcast_a_block_east_value"},
      {0x00014078, "mm_event_broadcast_block_north_value"},
      {0x00014080, "mm_event_broadcast_block_east_set"},
      {0x00034518, "cm_event_group_stream_switch_enable"},
      {0x0003f300, "cm_stream_switch_slave_west_3_slot0"},
      {0x0003f304, "cm_stream_switch_slave_west_3_slot1"},
      {0x0003f308, "cm_stream_switch_slave_west_3_slot2"},
      {0x0003f30c, "cm_stream_switch_slave_west_3_slot3"},
      {0x0003201c, "cm_debug_status"},
      {0x0001e6a0, "mm_lock13_release_v0"},
      {0x0001d154, "mm_dma_bd10_interleaved_state"},
      {0x000340fc, "cm_timer_high"},
      {0x00032028, "cm_pc_event2"},
      {0x00032024, "cm_pc_event1"},
      {0x00032020, "cm_pc_event0"},
      {0x0003f000, "cm_stream_switch_master_config_aie_core0"},
      {0x0003f004, "cm_stream_switch_master_config_aie_core1"},
      {0x0001e120, "mm_lock2_release_v0"},
      {0x0001e130, "mm_lock2_release_v1"},
      {0x0001d018, "mm_dma_bd0_control"},
      {0x0001d030, "mm_dma_bd1_packet"},
      {0x14, "shim_dma_bd_step_size"},
      {0x0001e740, "mm_lock14_acquire_nv"},
      {0x0001e380, "mm_lock7_release_nv"},
      {0x0003ff00, "shim_stream_switch_event_port_selection_0"},
      {0x0001de1c, "mm_dma_mm2s_1_start_queue"},
      {0x00034060, "shim_event_broadcast_a_block_west_set"},
      {0x00032110, "cm_ecc_scrubbing_event"},
      {0x0001e720, "mm_lock14_release_v0"},
      {0x0001e730, "mm_lock14_release_v1"},
      {0x0003f344, "cm_stream_switch_slave_north_3_slot1"},
      {0x0003f340, "cm_stream_switch_slave_north_3_slot0"},
      {0x0003f34c, "cm_stream_switch_slave_north_3_slot3"},
      {0x0003f2e4, "cm_stream_switch_slave_west_1_slot1"},
      {0x0001e500, "mm_lock10_release_nv"},
      {0x0003f118, "cm_stream_switch_slave_config_fifo_1"},
      {0x0003f114, "cm_stream_switch_slave_config_fifo_0"},
      {0x000340d0, "shim_trace_control0"},
      {0x000340d4, "shim_trace_control1"},
      {0x0001e5e0, "mm_lock11_acquire_v0"},
      {0x0001d1d8, "mm_dma_bd14_control"},
      {0x00036030, "cm_tile_control"},
      {0x00031080, "cm_performance_counter0_event_value"},
      {0x00034074, "cm_event_broadcast_block_north_clr"},
      {0x00032008, "cm_enable_events"},
      {0x00030280, "cm_program_counter"},
      {0x0001d1e0, "mm_dma_bd15_addr_a"},
      {0x0001d1e4, "mm_dma_bd15_addr_b"},
      {0x0001d128, "mm_dma_bd9_2d_x"},
      {0x0001d12c, "mm_dma_bd9_2d_y"},
      {0x000340f8, "cm_timer_low"},
      {0x0003f270, "cm_stream_switch_slave_south_0_slot0"},
      {0x0003f274, "cm_stream_switch_slave_south_0_slot1"},
      {0x0003f278, "cm_stream_switch_slave_south_0_slot2"},
      {0x0003f27c, "cm_stream_switch_slave_south_0_slot3"},
      {0x0001e670, "mm_lock12_acquire_v1"},
      {0x0001e660, "mm_lock12_acquire_v0"},
      {0x00014504, "mm_event_group_watchpoint_enable"},
      {0x0001e570, "mm_lock10_acquire_v1"},
      {0x0003f330, "cm_stream_switch_slave_north_2_slot0"},
      {0x0001d098, "mm_dma_bd4_control"},
      {0x00032004, "cm_core_status"},
      {0x00014f00, "shim_all_lock_state_value"},
      {0x0001d0e8, "mm_dma_bd7_2d_x"},
      {0x0001de08, "mm_dma_s2mm_1_ctrl"},
      {0x0001d0f8, "mm_dma_bd7_control"},
      {0x0001d010, "shim_dma_bd0_packet"},
      {0x0001d1e8, "mm_dma_bd15_2d_x"},
      {0x0001d1ec, "mm_dma_bd15_2d_y"},
      {0x0001de04, "mm_dma_s2mm_0_start_queue"},
      {0x0001e620, "mm_lock12_release_v0"},
      {0x0001e630, "mm_lock12_release_v1"},
      {0x000340fc, "shim_timer_high"},
      {0x00034520, "cm_event_group_user_event_enable"},
      {0x0003f22c, "cm_stream_switch_slave_dma_0_slot3"},
      {0x0003f228, "cm_stream_switch_slave_dma_0_slot2"},
      {0x0003f224, "cm_stream_switch_slave_dma_0_slot1"},
      {0x0003f220, "cm_stream_switch_slave_dma_0_slot0"},
      {0x00030660, "cm_core_amh0_part1"},
      {0x00030120, "cm_core_p2"},
      {0x000304C0, "cm_core_s4"}
    };
  }
  */

void AIE1UsedRegisters::populateRegValueToNameMap() {
   // core_module registers
   coreRegValueToName = {
      {0x00020000, "cm_program_memory"},
      {0x00024000, "cm_program_memory_error_injection"},
      {0x00030000, "cm_core_r0"},
      {0x00030010, "cm_core_r1"},
      {0x00030020, "cm_core_r2"},
      {0x00030030, "cm_core_r3"},
      {0x00030040, "cm_core_r4"},
      {0x00030050, "cm_core_r5"},
      {0x00030060, "cm_core_r6"},
      {0x00030070, "cm_core_r7"},
      {0x00030080, "cm_core_r8"},
      {0x00030090, "cm_core_r9"},
      {0x000300A0, "cm_core_r10"},
      {0x000300B0, "cm_core_r11"},
      {0x000300C0, "cm_core_r12"},
      {0x000300D0, "cm_core_r13"},
      {0x000300E0, "cm_core_r14"},
      {0x000300F0, "cm_core_r15"},
      {0x00030100, "cm_core_p0"},
      {0x00030110, "cm_core_p1"},
      {0x00030120, "cm_core_p2"},
      {0x00030130, "cm_core_p3"},
      {0x00030140, "cm_core_p4"},
      {0x00030150, "cm_core_p5"},
      {0x00030160, "cm_core_p6"},
      {0x00030170, "cm_core_p7"},
      {0x00030180, "cm_core_cl0"},
      {0x00030190, "cm_core_ch0"},
      {0x000301A0, "cm_core_cl1"},
      {0x000301B0, "cm_core_ch1"},
      {0x000301C0, "cm_core_cl2"},
      {0x000301D0, "cm_core_ch2"},
      {0x000301E0, "cm_core_cl3"},
      {0x000301F0, "cm_core_ch3"},
      {0x00030200, "cm_core_cl4"},
      {0x00030210, "cm_core_ch4"},
      {0x00030220, "cm_core_cl5"},
      {0x00030230, "cm_core_ch5"},
      {0x00030240, "cm_core_cl6"},
      {0x00030250, "cm_core_ch6"},
      {0x00030260, "cm_core_cl7"},
      {0x00030270, "cm_core_ch7"},
      {0x00030280, "cm_program_counter"},
      {0x00030290, "cm_core_fc"},
      {0x000302A0, "cm_core_sp"},
      {0x000302B0, "cm_core_lr"},
      {0x000302C0, "cm_core_m0"},
      {0x000302D0, "cm_core_m1"},
      {0x000302E0, "cm_core_m2"},
      {0x000302F0, "cm_core_m3"},
      {0x00030300, "cm_core_m4"},
      {0x00030310, "cm_core_m5"},
      {0x00030320, "cm_core_m6"},
      {0x00030330, "cm_core_m7"},
      {0x00030340, "cm_core_cb0"},
      {0x00030350, "cm_core_cb1"},
      {0x00030360, "cm_core_cb2"},
      {0x00030370, "cm_core_cb3"},
      {0x00030380, "cm_core_cb4"},
      {0x00030390, "cm_core_cb5"},
      {0x000303A0, "cm_core_cb6"},
      {0x000303B0, "cm_core_cb7"},
      {0x000303C0, "cm_core_cs0"},
      {0x000303D0, "cm_core_cs1"},
      {0x000303E0, "cm_core_cs2"},
      {0x000303F0, "cm_core_cs3"},
      {0x00030400, "cm_core_cs4"},
      {0x00030410, "cm_core_cs5"},
      {0x00030420, "cm_core_cs6"},
      {0x00030430, "cm_core_cs7"},
      {0x00030440, "cm_md0"},
      {0x00030450, "cm_md1"},
      {0x00030460, "cm_mc0"},
      {0x00030470, "cm_mc1"},
      {0x00030480, "cm_core_s0"},
      {0x00030490, "cm_core_s1"},
      {0x000304A0, "cm_core_s2"},
      {0x000304B0, "cm_core_s3"},
      {0x000304C0, "cm_core_s4"},
      {0x000304D0, "cm_core_s5"},
      {0x000304E0, "cm_core_s6"},
      {0x000304F0, "cm_core_s7"},
      {0x00030500, "cm_core_ls"},
      {0x00030510, "cm_core_le"},
      {0x00030520, "cm_core_lc"},
      {0x00030530, "cm_core_vrl0"},
      {0x00030540, "cm_core_vrh0"},
      {0x00030550, "cm_core_vrl1"},
      {0x00030560, "cm_core_vrh1"},
      {0x00030570, "cm_core_vrl2"},
      {0x00030580, "cm_core_vrh2"},
      {0x00030590, "cm_core_vrl3"},
      {0x000305A0, "cm_core_vrh3"},
      {0x000305B0, "cm_core_vcl0"},
      {0x000305C0, "cm_core_vch0"},
      {0x000305D0, "cm_core_vcl1"},
      {0x000305E0, "cm_core_vch1"},
      {0x000305F0, "cm_core_vdl0"},
      {0x00030600, "cm_core_vdh0"},
      {0x00030610, "cm_core_vdl1"},
      {0x00030620, "cm_core_vdh1"},
      {0x00030630, "cm_core_aml0_part1"},
      {0x00030640, "cm_core_aml0_part2"},
      {0x00030650, "cm_core_aml0_part3"},
      {0x00030660, "cm_core_amh0_part1"},
      {0x00030670, "cm_core_amh0_part2"},
      {0x00030680, "cm_core_amh0_part3"},
      {0x00030690, "cm_core_aml1_part1"},
      {0x000306A0, "cm_core_aml1_part2"},
      {0x000306B0, "cm_core_aml1_part3"},
      {0x000306C0, "cm_core_amh1_part1"},
      {0x000306D0, "cm_core_amh1_part2"},
      {0x000306E0, "cm_core_amh1_part3"},
      {0x000306F0, "cm_core_aml2_part1"},
      {0x00030700, "cm_core_aml2_part2"},
      {0x00030710, "cm_core_aml2_part3"},
      {0x00030720, "cm_core_amh2_part1"},
      {0x00030730, "cm_core_amh2_part2"},
      {0x00030740, "cm_core_amh2_part3"},
      {0x00030750, "cm_core_aml3_part1"},
      {0x00030760, "cm_core_aml3_part2"},
      {0x00030770, "cm_core_aml3_part3"},
      {0x00030780, "cm_core_amh3_part1"},
      {0x00030790, "cm_core_amh3_part2"},
      {0x000307A0, "cm_core_amh3_part3"},
      {0x00031000, "cm_performance_control0"},
      {0x00031004, "cm_performance_control1"},
      {0x00031008, "cm_performance_control2"},
      {0x00031020, "cm_performance_counter0"},
      {0x00031024, "cm_performance_counter1"},
      {0x00031028, "cm_performance_counter2"},
      {0x0003102C, "cm_performance_counter3"},
      {0x00031080, "cm_performance_counter0_event_value"},
      {0x00031084, "cm_performance_counter1_event_value"},
      {0x00031088, "cm_performance_counter2_event_value"},
      {0x0003108C, "cm_performance_counter3_event_value"},
      {0x00032000, "cm_core_control"},
      {0x00032004, "cm_core_status"},
      {0x00032008, "cm_enable_events"},
      {0x0003200C, "cm_reset_event"},
      {0x00032010, "cm_debug_control0"},
      {0x00032014, "cm_debug_control1"},
      {0x00032018, "cm_debug_control2"},
      {0x0003201C, "cm_debug_status"},
      {0x00032020, "cm_pc_event0"},
      {0x00032024, "cm_pc_event1"},
      {0x00032028, "cm_pc_event2"},
      {0x0003202C, "cm_pc_event3"},
      {0x00032030, "cm_error_halt_control"},
      {0x00032034, "cm_error_halt_event"},
      {0x00032100, "cm_ecc_control"},
      {0x00032110, "cm_ecc_scrubbing_event"},
      {0x00032120, "cm_ecc_failing_address"},
      {0x00032130, "cm_reserved0"},
      {0x00032134, "cm_reserved1"},
      {0x00032138, "cm_reserved2"},
      {0x0003213C, "cm_reserved3"},
      {0x00034000, "cm_timer_control"},
      {0x00034008, "cm_event_generate"},
      {0x00034010, "cm_event_broadcast0"},
      {0x00034014, "cm_event_broadcast1"},
      {0x00034018, "cm_event_broadcast2"},
      {0x0003401C, "cm_event_broadcast3"},
      {0x00034020, "cm_event_broadcast4"},
      {0x00034024, "cm_event_broadcast5"},
      {0x00034028, "cm_event_broadcast6"},
      {0x0003402C, "cm_event_broadcast7"},
      {0x00034030, "cm_event_broadcast8"},
      {0x00034034, "cm_event_broadcast9"},
      {0x00034038, "cm_event_broadcast10"},
      {0x0003403C, "cm_event_broadcast11"},
      {0x00034040, "cm_event_broadcast12"},
      {0x00034044, "cm_event_broadcast13"},
      {0x00034048, "cm_event_broadcast14"},
      {0x0003404C, "cm_event_broadcast15"},
      {0x00034050, "cm_event_broadcast_block_south_set"},
      {0x00034054, "cm_event_broadcast_block_south_clr"},
      {0x00034058, "cm_event_broadcast_block_south_value"},
      {0x00034060, "cm_event_broadcast_block_west_set"},
      {0x00034064, "cm_event_broadcast_block_west_clr"},
      {0x00034068, "cm_event_broadcast_block_west_value"},
      {0x00034070, "cm_event_broadcast_block_north_set"},
      {0x00034074, "cm_event_broadcast_block_north_clr"},
      {0x00034078, "cm_event_broadcast_block_north_value"},
      {0x00034080, "cm_event_broadcast_block_east_set"},
      {0x00034084, "cm_event_broadcast_block_east_clr"},
      {0x00034088, "cm_event_broadcast_block_east_value"},
      {0x000340D0, "cm_trace_control0"},
      {0x000340D4, "cm_trace_control1"},
      {0x000340D8, "cm_trace_status"},
      {0x000340E0, "cm_trace_event0"},
      {0x000340E4, "cm_trace_event1"},
      {0x000340F0, "cm_timer_trig_event_low_value"},
      {0x000340F4, "cm_timer_trig_event_high_value"},
      {0x000340F8, "cm_timer_low"},
      {0x000340FC, "cm_timer_high"},
      {0x00034200, "cm_event_status0"},
      {0x00034204, "cm_event_status1"},
      {0x00034208, "cm_event_status2"},
      {0x0003420C, "cm_event_status3"},
      {0x00034400, "cm_combo_event_inputs"},
      {0x00034404, "cm_combo_event_control"},
      {0x00034500, "cm_event_group_0_enable"},
      {0x00034504, "cm_event_group_pc_enable"},
      {0x00034508, "cm_event_group_core_stall_enable"},
      {0x0003450C, "cm_event_group_core_program_flow_enable"},
      {0x00034510, "cm_event_group_errors0_enable"},
      {0x00034514, "cm_event_group_errors1_enable"},
      {0x00034518, "cm_event_group_stream_switch_enable"},
      {0x0003451C, "cm_event_group_broadcast_enable"},
      {0x00034520, "cm_event_group_user_event_enable"},
      {0x00036030, "cm_tile_control"},
      {0x00036034, "cm_tile_control_packet_handler_status"},
      {0x00036040, "cm_tile_clock_control"},
      {0x00036044, "cm_cssd_trigger"},
      {0x00036050, "cm_spare_reg"},
      {0x0003F000, "cm_stream_switch_master_config_aie_core0"},
      {0x0003F004, "cm_stream_switch_master_config_aie_core1"},
      {0x0003F008, "cm_stream_switch_master_config_dma0"},
      {0x0003F00C, "cm_stream_switch_master_config_dma1"},
      {0x0003F010, "cm_stream_switch_master_config_tile_ctrl"},
      {0x0003F014, "cm_stream_switch_master_config_fifo0"},
      {0x0003F018, "cm_stream_switch_master_config_fifo1"},
      {0x0003F01C, "cm_stream_switch_master_config_south0"},
      {0x0003F020, "cm_stream_switch_master_config_south1"},
      {0x0003F024, "cm_stream_switch_master_config_south2"},
      {0x0003F028, "cm_stream_switch_master_config_south3"},
      {0x0003F02C, "cm_stream_switch_master_config_west0"},
      {0x0003F030, "cm_stream_switch_master_config_west1"},
      {0x0003F034, "cm_stream_switch_master_config_west2"},
      {0x0003F038, "cm_stream_switch_master_config_west3"},
      {0x0003F03C, "cm_stream_switch_master_config_north0"},
      {0x0003F040, "cm_stream_switch_master_config_north1"},
      {0x0003F044, "cm_stream_switch_master_config_north2"},
      {0x0003F048, "cm_stream_switch_master_config_north3"},
      {0x0003F04C, "cm_stream_switch_master_config_north4"},
      {0x0003F050, "cm_stream_switch_master_config_north5"},
      {0x0003F054, "cm_stream_switch_master_config_east0"},
      {0x0003F058, "cm_stream_switch_master_config_east1"},
      {0x0003F05C, "cm_stream_switch_master_config_east2"},
      {0x0003F060, "cm_stream_switch_master_config_east3"},
      {0x0003F100, "cm_stream_switch_slave_config_aie_core0"},
      {0x0003F104, "cm_stream_switch_slave_config_aie_core1"},
      {0x0003F108, "cm_stream_switch_slave_config_dma_0"},
      {0x0003F10C, "cm_stream_switch_slave_config_dma_1"},
      {0x0003F110, "cm_stream_switch_slave_config_tile_ctrl"},
      {0x0003F114, "cm_stream_switch_slave_config_fifo_0"},
      {0x0003F118, "cm_stream_switch_slave_config_fifo_1"},
      {0x0003F11C, "cm_stream_switch_slave_config_south_0"},
      {0x0003F120, "cm_stream_switch_slave_config_south_1"},
      {0x0003F124, "cm_stream_switch_slave_config_south_2"},
      {0x0003F128, "cm_stream_switch_slave_config_south_3"},
      {0x0003F12C, "cm_stream_switch_slave_config_south_4"},
      {0x0003F130, "cm_stream_switch_slave_config_south_5"},
      {0x0003F134, "cm_stream_switch_slave_config_west_0"},
      {0x0003F138, "cm_stream_switch_slave_config_west_1"},
      {0x0003F13C, "cm_stream_switch_slave_config_west_2"},
      {0x0003F140, "cm_stream_switch_slave_config_west_3"},
      {0x0003F144, "cm_stream_switch_slave_config_north_0"},
      {0x0003F148, "cm_stream_switch_slave_config_north_1"},
      {0x0003F14C, "cm_stream_switch_slave_config_north_2"},
      {0x0003F150, "cm_stream_switch_slave_config_north_3"},
      {0x0003F154, "cm_stream_switch_slave_config_east_0"},
      {0x0003F158, "cm_stream_switch_slave_config_east_1"},
      {0x0003F15C, "cm_stream_switch_slave_config_east_2"},
      {0x0003F160, "cm_stream_switch_slave_config_east_3"},
      {0x0003F164, "cm_stream_switch_slave_config_aie_trace"},
      {0x0003F168, "cm_stream_switch_slave_config_mem_trace"},
      {0x0003F200, "cm_stream_switch_slave_aie_core0_slot0"},
      {0x0003F204, "cm_stream_switch_slave_aie_core0_slot1"},
      {0x0003F208, "cm_stream_switch_slave_aie_core0_slot2"},
      {0x0003F20C, "cm_stream_switch_slave_aie_core0_slot3"},
      {0x0003F210, "cm_stream_switch_slave_aie_core1_slot0"},
      {0x0003F214, "cm_stream_switch_slave_aie_core1_slot1"},
      {0x0003F218, "cm_stream_switch_slave_aie_core1_slot2"},
      {0x0003F21C, "cm_stream_switch_slave_aie_core1_slot3"},
      {0x0003F220, "cm_stream_switch_slave_dma_0_slot0"},
      {0x0003F224, "cm_stream_switch_slave_dma_0_slot1"},
      {0x0003F228, "cm_stream_switch_slave_dma_0_slot2"},
      {0x0003F22C, "cm_stream_switch_slave_dma_0_slot3"},
      {0x0003F230, "cm_stream_switch_slave_dma_1_slot0"},
      {0x0003F234, "cm_stream_switch_slave_dma_1_slot1"},
      {0x0003F238, "cm_stream_switch_slave_dma_1_slot2"},
      {0x0003F23C, "cm_stream_switch_slave_dma_1_slot3"},
      {0x0003F240, "cm_stream_switch_slave_tile_ctrl_slot0"},
      {0x0003F244, "cm_stream_switch_slave_tile_ctrl_slot1"},
      {0x0003F248, "cm_stream_switch_slave_tile_ctrl_slot2"},
      {0x0003F24C, "cm_stream_switch_slave_tile_ctrl_slot3"},
      {0x0003F250, "cm_stream_switch_slave_fifo_0_slot0"},
      {0x0003F254, "cm_stream_switch_slave_fifo_0_slot1"},
      {0x0003F258, "cm_stream_switch_slave_fifo_0_slot2"},
      {0x0003F25C, "cm_stream_switch_slave_fifo_0_slot3"},
      {0x0003F260, "cm_stream_switch_slave_fifo_1_slot0"},
      {0x0003F264, "cm_stream_switch_slave_fifo_1_slot1"},
      {0x0003F268, "cm_stream_switch_slave_fifo_1_slot2"},
      {0x0003F26C, "cm_stream_switch_slave_fifo_1_slot3"},
      {0x0003F270, "cm_stream_switch_slave_south_0_slot0"},
      {0x0003F274, "cm_stream_switch_slave_south_0_slot1"},
      {0x0003F278, "cm_stream_switch_slave_south_0_slot2"},
      {0x0003F27C, "cm_stream_switch_slave_south_0_slot3"},
      {0x0003F280, "cm_stream_switch_slave_south_1_slot0"},
      {0x0003F284, "cm_stream_switch_slave_south_1_slot1"},
      {0x0003F288, "cm_stream_switch_slave_south_1_slot2"},
      {0x0003F28C, "cm_stream_switch_slave_south_1_slot3"},
      {0x0003F290, "cm_stream_switch_slave_south_2_slot0"},
      {0x0003F294, "cm_stream_switch_slave_south_2_slot1"},
      {0x0003F298, "cm_stream_switch_slave_south_2_slot2"},
      {0x0003F29C, "cm_stream_switch_slave_south_2_slot3"},
      {0x0003F2A0, "cm_stream_switch_slave_south_3_slot0"},
      {0x0003F2A4, "cm_stream_switch_slave_south_3_slot1"},
      {0x0003F2A8, "cm_stream_switch_slave_south_3_slot2"},
      {0x0003F2AC, "cm_stream_switch_slave_south_3_slot3"},
      {0x0003F2B0, "cm_stream_switch_slave_south_4_slot0"},
      {0x0003F2B4, "cm_stream_switch_slave_south_4_slot1"},
      {0x0003F2B8, "cm_stream_switch_slave_south_4_slot2"},
      {0x0003F2BC, "cm_stream_switch_slave_south_4_slot3"},
      {0x0003F2C0, "cm_stream_switch_slave_south_5_slot0"},
      {0x0003F2C4, "cm_stream_switch_slave_south_5_slot1"},
      {0x0003F2C8, "cm_stream_switch_slave_south_5_slot2"},
      {0x0003F2CC, "cm_stream_switch_slave_south_5_slot3"},
      {0x0003F2D0, "cm_stream_switch_slave_west_0_slot0"},
      {0x0003F2D4, "cm_stream_switch_slave_west_0_slot1"},
      {0x0003F2D8, "cm_stream_switch_slave_west_0_slot2"},
      {0x0003F2DC, "cm_stream_switch_slave_west_0_slot3"},
      {0x0003F2E0, "cm_stream_switch_slave_west_1_slot0"},
      {0x0003F2E4, "cm_stream_switch_slave_west_1_slot1"},
      {0x0003F2E8, "cm_stream_switch_slave_west_1_slot2"},
      {0x0003F2EC, "cm_stream_switch_slave_west_1_slot3"},
      {0x0003F2F0, "cm_stream_switch_slave_west_2_slot0"},
      {0x0003F2F4, "cm_stream_switch_slave_west_2_slot1"},
      {0x0003F2F8, "cm_stream_switch_slave_west_2_slot2"},
      {0x0003F2FC, "cm_stream_switch_slave_west_2_slot3"},
      {0x0003F300, "cm_stream_switch_slave_west_3_slot0"},
      {0x0003F304, "cm_stream_switch_slave_west_3_slot1"},
      {0x0003F308, "cm_stream_switch_slave_west_3_slot2"},
      {0x0003F30C, "cm_stream_switch_slave_west_3_slot3"},
      {0x0003F310, "cm_stream_switch_slave_north_0_slot0"},
      {0x0003F314, "cm_stream_switch_slave_north_0_slot1"},
      {0x0003F318, "cm_stream_switch_slave_north_0_slot2"},
      {0x0003F31C, "cm_stream_switch_slave_north_0_slot3"},
      {0x0003F320, "cm_stream_switch_slave_north_1_slot0"},
      {0x0003F324, "cm_stream_switch_slave_north_1_slot1"},
      {0x0003F328, "cm_stream_switch_slave_north_1_slot2"},
      {0x0003F32C, "cm_stream_switch_slave_north_1_slot3"},
      {0x0003F330, "cm_stream_switch_slave_north_2_slot0"},
      {0x0003F334, "cm_stream_switch_slave_north_2_slot1"},
      {0x0003F338, "cm_stream_switch_slave_north_2_slot2"},
      {0x0003F33C, "cm_stream_switch_slave_north_2_slot3"},
      {0x0003F340, "cm_stream_switch_slave_north_3_slot0"},
      {0x0003F344, "cm_stream_switch_slave_north_3_slot1"},
      {0x0003F348, "cm_stream_switch_slave_north_3_slot2"},
      {0x0003F34C, "cm_stream_switch_slave_north_3_slot3"},
      {0x0003F350, "cm_stream_switch_slave_east_0_slot0"},
      {0x0003F354, "cm_stream_switch_slave_east_0_slot1"},
      {0x0003F358, "cm_stream_switch_slave_east_0_slot2"},
      {0x0003F35C, "cm_stream_switch_slave_east_0_slot3"},
      {0x0003F360, "cm_stream_switch_slave_east_1_slot0"},
      {0x0003F364, "cm_stream_switch_slave_east_1_slot1"},
      {0x0003F368, "cm_stream_switch_slave_east_1_slot2"},
      {0x0003F36C, "cm_stream_switch_slave_east_1_slot3"},
      {0x0003F370, "cm_stream_switch_slave_east_2_slot0"},
      {0x0003F374, "cm_stream_switch_slave_east_2_slot1"},
      {0x0003F378, "cm_stream_switch_slave_east_2_slot2"},
      {0x0003F37C, "cm_stream_switch_slave_east_2_slot3"},
      {0x0003F380, "cm_stream_switch_slave_east_3_slot0"},
      {0x0003F384, "cm_stream_switch_slave_east_3_slot1"},
      {0x0003F388, "cm_stream_switch_slave_east_3_slot2"},
      {0x0003F38C, "cm_stream_switch_slave_east_3_slot3"},
      {0x0003F390, "cm_stream_switch_slave_aie_trace_slot0"},
      {0x0003F394, "cm_stream_switch_slave_aie_trace_slot1"},
      {0x0003F398, "cm_stream_switch_slave_aie_trace_slot2"},
      {0x0003F39C, "cm_stream_switch_slave_aie_trace_slot3"},
      {0x0003F3A0, "cm_stream_switch_slave_mem_trace_slot0"},
      {0x0003F3A4, "cm_stream_switch_slave_mem_trace_slot1"},
      {0x0003F3A8, "cm_stream_switch_slave_mem_trace_slot2"},
      {0x0003F3AC, "cm_stream_switch_slave_mem_trace_slot3"},
      {0x0003FF00, "cm_stream_switch_event_port_selection_0"},
      {0x0003FF04, "cm_stream_switch_event_port_selection_1"}
   };

   // memory_module registers
   memoryRegValueToName = {
      {0x0001EF00, "mm_all_lock_state_value"},
      {0x00012000, "mm_checkbit_error_generation"},
      {0x00014404, "mm_combo_event_control"},
      {0x00014400, "mm_combo_event_inputs"},
      {0x0001D008, "mm_dma_bd0_2d_x"},
      {0x0001D00C, "mm_dma_bd0_2d_y"},
      {0x0001D000, "mm_dma_bd0_addr_a"},
      {0x0001D004, "mm_dma_bd0_addr_b"},
      {0x0001D018, "mm_dma_bd0_control"},
      {0x0001D014, "mm_dma_bd0_interleaved_state"},
      {0x0001D010, "mm_dma_bd0_packet"},
      {0x0001D148, "mm_dma_bd10_2d_x"},
      {0x0001D14C, "mm_dma_bd10_2d_y"},
      {0x0001D140, "mm_dma_bd10_addr_a"},
      {0x0001D144, "mm_dma_bd10_addr_b"},
      {0x0001D158, "mm_dma_bd10_control"},
      {0x0001D154, "mm_dma_bd10_interleaved_state"},
      {0x0001D150, "mm_dma_bd10_packet"},
      {0x0001D168, "mm_dma_bd11_2d_x"},
      {0x0001D16C, "mm_dma_bd11_2d_y"},
      {0x0001D160, "mm_dma_bd11_addr_a"},
      {0x0001D164, "mm_dma_bd11_addr_b"},
      {0x0001D178, "mm_dma_bd11_control"},
      {0x0001D174, "mm_dma_bd11_interleaved_state"},
      {0x0001D170, "mm_dma_bd11_packet"},
      {0x0001D188, "mm_dma_bd12_2d_x"},
      {0x0001D18C, "mm_dma_bd12_2d_y"},
      {0x0001D180, "mm_dma_bd12_addr_a"},
      {0x0001D184, "mm_dma_bd12_addr_b"},
      {0x0001D198, "mm_dma_bd12_control"},
      {0x0001D194, "mm_dma_bd12_interleaved_state"},
      {0x0001D190, "mm_dma_bd12_packet"},
      {0x0001D1A8, "mm_dma_bd13_2d_x"},
      {0x0001D1AC, "mm_dma_bd13_2d_y"},
      {0x0001D1A0, "mm_dma_bd13_addr_a"},
      {0x0001D1A4, "mm_dma_bd13_addr_b"},
      {0x0001D1B8, "mm_dma_bd13_control"},
      {0x0001D1B4, "mm_dma_bd13_interleaved_state"},
      {0x0001D1B0, "mm_dma_bd13_packet"},
      {0x0001D1C8, "mm_dma_bd14_2d_x"},
      {0x0001D1CC, "mm_dma_bd14_2d_y"},
      {0x0001D1C0, "mm_dma_bd14_addr_a"},
      {0x0001D1C4, "mm_dma_bd14_addr_b"},
      {0x0001D1D8, "mm_dma_bd14_control"},
      {0x0001D1D4, "mm_dma_bd14_interleaved_state"},
      {0x0001D1D0, "mm_dma_bd14_packet"},
      {0x0001D1E8, "mm_dma_bd15_2d_x"},
      {0x0001D1EC, "mm_dma_bd15_2d_y"},
      {0x0001D1E0, "mm_dma_bd15_addr_a"},
      {0x0001D1E4, "mm_dma_bd15_addr_b"},
      {0x0001D1F8, "mm_dma_bd15_control"},
      {0x0001D1F4, "mm_dma_bd15_interleaved_state"},
      {0x0001D1F0, "mm_dma_bd15_packet"},
      {0x0001D028, "mm_dma_bd1_2d_x"},
      {0x0001D02C, "mm_dma_bd1_2d_y"},
      {0x0001D020, "mm_dma_bd1_addr_a"},
      {0x0001D024, "mm_dma_bd1_addr_b"},
      {0x0001D038, "mm_dma_bd1_control"},
      {0x0001D034, "mm_dma_bd1_interleaved_state"},
      {0x0001D030, "mm_dma_bd1_packet"},
      {0x0001D048, "mm_dma_bd2_2d_x"},
      {0x0001D04C, "mm_dma_bd2_2d_y"},
      {0x0001D040, "mm_dma_bd2_addr_a"},
      {0x0001D044, "mm_dma_bd2_addr_b"},
      {0x0001D058, "mm_dma_bd2_control"},
      {0x0001D054, "mm_dma_bd2_interleaved_state"},
      {0x0001D050, "mm_dma_bd2_packet"},
      {0x0001D068, "mm_dma_bd3_2d_x"},
      {0x0001D06C, "mm_dma_bd3_2d_y"},
      {0x0001D060, "mm_dma_bd3_addr_a"},
      {0x0001D064, "mm_dma_bd3_addr_b"},
      {0x0001D078, "mm_dma_bd3_control"},
      {0x0001D074, "mm_dma_bd3_interleaved_state"},
      {0x0001D070, "mm_dma_bd3_packet"},
      {0x0001D088, "mm_dma_bd4_2d_x"},
      {0x0001D08C, "mm_dma_bd4_2d_y"},
      {0x0001D080, "mm_dma_bd4_addr_a"},
      {0x0001D084, "mm_dma_bd4_addr_b"},
      {0x0001D098, "mm_dma_bd4_control"},
      {0x0001D094, "mm_dma_bd4_interleaved_state"},
      {0x0001D090, "mm_dma_bd4_packet"},
      {0x0001D0A8, "mm_dma_bd5_2d_x"},
      {0x0001D0AC, "mm_dma_bd5_2d_y"},
      {0x0001D0A0, "mm_dma_bd5_addr_a"},
      {0x0001D0A4, "mm_dma_bd5_addr_b"},
      {0x0001D0B8, "mm_dma_bd5_control"},
      {0x0001D0B4, "mm_dma_bd5_interleaved_state"},
      {0x0001D0B0, "mm_dma_bd5_packet"},
      {0x0001D0C8, "mm_dma_bd6_2d_x"},
      {0x0001D0CC, "mm_dma_bd6_2d_y"},
      {0x0001D0C0, "mm_dma_bd6_addr_a"},
      {0x0001D0C4, "mm_dma_bd6_addr_b"},
      {0x0001D0D8, "mm_dma_bd6_control"},
      {0x0001D0D4, "mm_dma_bd6_interleaved_state"},
      {0x0001D0D0, "mm_dma_bd6_packet"},
      {0x0001D0E8, "mm_dma_bd7_2d_x"},
      {0x0001D0EC, "mm_dma_bd7_2d_y"},
      {0x0001D0E0, "mm_dma_bd7_addr_a"},
      {0x0001D0E4, "mm_dma_bd7_addr_b"},
      {0x0001D0F8, "mm_dma_bd7_control"},
      {0x0001D0F4, "mm_dma_bd7_interleaved_state"},
      {0x0001D0F0, "mm_dma_bd7_packet"},
      {0x0001D108, "mm_dma_bd8_2d_x"},
      {0x0001D10C, "mm_dma_bd8_2d_y"},
      {0x0001D100, "mm_dma_bd8_addr_a"},
      {0x0001D104, "mm_dma_bd8_addr_b"},
      {0x0001D118, "mm_dma_bd8_control"},
      {0x0001D114, "mm_dma_bd8_interleaved_state"},
      {0x0001D110, "mm_dma_bd8_packet"},
      {0x0001D128, "mm_dma_bd9_2d_x"},
      {0x0001D12C, "mm_dma_bd9_2d_y"},
      {0x0001D120, "mm_dma_bd9_addr_a"},
      {0x0001D124, "mm_dma_bd9_addr_b"},
      {0x0001D138, "mm_dma_bd9_control"},
      {0x0001D134, "mm_dma_bd9_interleaved_state"},
      {0x0001D130, "mm_dma_bd9_packet"},
      {0x0001DF20, "mm_dma_fifo_counter"},
      {0x0001DE10, "mm_dma_mm2s_0_ctrl"},
      {0x0001DE14, "mm_dma_mm2s_0_start_queue"},
      {0x0001DE18, "mm_dma_mm2s_1_ctrl"},
      {0x0001DE1C, "mm_dma_mm2s_1_start_queue"},
      {0x0001DF10, "mm_dma_mm2s_status"},
      {0x0001DE00, "mm_dma_s2mm_0_ctrl"},
      {0x0001DE04, "mm_dma_s2mm_0_start_queue"},
      {0x0001DE08, "mm_dma_s2mm_1_ctrl"},
      {0x0001DE0C, "mm_dma_s2mm_1_start_queue"},
      {0x0001DF00, "mm_dma_s2mm_status"},
      {0x00012120, "mm_ecc_failing_address"},
      {0x00012110, "mm_ecc_scrubbing_event"},
      {0x00014010, "mm_event_broadcast0"},
      {0x00014014, "mm_event_broadcast1"},
      {0x00014038, "mm_event_broadcast10"},
      {0x0001403C, "mm_event_broadcast11"},
      {0x00014040, "mm_event_broadcast12"},
      {0x00014044, "mm_event_broadcast13"},
      {0x00014048, "mm_event_broadcast14"},
      {0x0001404C, "mm_event_broadcast15"},
      {0x00014018, "mm_event_broadcast2"},
      {0x0001401C, "mm_event_broadcast3"},
      {0x00014020, "mm_event_broadcast4"},
      {0x00014024, "mm_event_broadcast5"},
      {0x00014028, "mm_event_broadcast6"},
      {0x0001402C, "mm_event_broadcast7"},
      {0x00014030, "mm_event_broadcast8"},
      {0x00014034, "mm_event_broadcast9"},
      {0x00014084, "mm_event_broadcast_block_east_clr"},
      {0x00014080, "mm_event_broadcast_block_east_set"},
      {0x00014088, "mm_event_broadcast_block_east_value"},
      {0x00014074, "mm_event_broadcast_block_north_clr"},
      {0x00014070, "mm_event_broadcast_block_north_set"},
      {0x00014078, "mm_event_broadcast_block_north_value"},
      {0x00014054, "mm_event_broadcast_block_south_clr"},
      {0x00014050, "mm_event_broadcast_block_south_set"},
      {0x00014058, "mm_event_broadcast_block_south_value"},
      {0x00014064, "mm_event_broadcast_block_west_clr"},
      {0x00014060, "mm_event_broadcast_block_west_set"},
      {0x00014068, "mm_event_broadcast_block_west_value"},
      {0x00014008, "mm_event_generate"},
      {0x00014500, "mm_event_group_0_enable"},
      {0x00014518, "mm_event_group_broadcast_enable"},
      {0x00014508, "mm_event_group_dma_enable"},
      {0x00014514, "mm_event_group_error_enable"},
      {0x0001450C, "mm_event_group_lock_enable"},
      {0x00014510, "mm_event_group_memory_conflict_enable"},
      {0x0001451C, "mm_event_group_user_event_enable"},
      {0x00014504, "mm_event_group_watchpoint_enable"},
      {0x00014200, "mm_event_status0"},
      {0x00014204, "mm_event_status1"},
      {0x00014208, "mm_event_status2"},
      {0x0001420C, "mm_event_status3"},
      {0x0001E040, "mm_lock0_acquire_nv"},
      {0x0001E060, "mm_lock0_acquire_v0"},
      {0x0001E070, "mm_lock0_acquire_v1"},
      {0x0001E000, "mm_lock0_release_nv"},
      {0x0001E020, "mm_lock0_release_v0"},
      {0x0001E030, "mm_lock0_release_v1"},
      {0x0001E540, "mm_lock10_acquire_nv"},
      {0x0001E560, "mm_lock10_acquire_v0"},
      {0x0001E570, "mm_lock10_acquire_v1"},
      {0x0001E500, "mm_lock10_release_nv"},
      {0x0001E520, "mm_lock10_release_v0"},
      {0x0001E530, "mm_lock10_release_v1"},
      {0x0001E5C0, "mm_lock11_acquire_nv"},
      {0x0001E5E0, "mm_lock11_acquire_v0"},
      {0x0001E5F0, "mm_lock11_acquire_v1"},
      {0x0001E580, "mm_lock11_release_nv"},
      {0x0001E5A0, "mm_lock11_release_v0"},
      {0x0001E5B0, "mm_lock11_release_v1"},
      {0x0001E640, "mm_lock12_acquire_nv"},
      {0x0001E660, "mm_lock12_acquire_v0"},
      {0x0001E670, "mm_lock12_acquire_v1"},
      {0x0001E600, "mm_lock12_release_nv"},
      {0x0001E620, "mm_lock12_release_v0"},
      {0x0001E630, "mm_lock12_release_v1"},
      {0x0001E6C0, "mm_lock13_acquire_nv"},
      {0x0001E6E0, "mm_lock13_acquire_v0"},
      {0x0001E6F0, "mm_lock13_acquire_v1"},
      {0x0001E680, "mm_lock13_release_nv"},
      {0x0001E6A0, "mm_lock13_release_v0"},
      {0x0001E6B0, "mm_lock13_release_v1"},
      {0x0001E740, "mm_lock14_acquire_nv"},
      {0x0001E760, "mm_lock14_acquire_v0"},
      {0x0001E770, "mm_lock14_acquire_v1"},
      {0x0001E700, "mm_lock14_release_nv"},
      {0x0001E720, "mm_lock14_release_v0"},
      {0x0001E730, "mm_lock14_release_v1"},
      {0x0001E7C0, "mm_lock15_acquire_nv"},
      {0x0001E7E0, "mm_lock15_acquire_v0"},
      {0x0001E7F0, "mm_lock15_acquire_v1"},
      {0x0001E780, "mm_lock15_release_nv"},
      {0x0001E7A0, "mm_lock15_release_v0"},
      {0x0001E7B0, "mm_lock15_release_v1"},
      {0x0001E0C0, "mm_lock1_acquire_nv"},
      {0x0001E0E0, "mm_lock1_acquire_v0"},
      {0x0001E0F0, "mm_lock1_acquire_v1"},
      {0x0001E080, "mm_lock1_release_nv"},
      {0x0001E0A0, "mm_lock1_release_v0"},
      {0x0001E0B0, "mm_lock1_release_v1"},
      {0x0001E140, "mm_lock2_acquire_nv"},
      {0x0001E160, "mm_lock2_acquire_v0"},
      {0x0001E170, "mm_lock2_acquire_v1"},
      {0x0001E100, "mm_lock2_release_nv"},
      {0x0001E120, "mm_lock2_release_v0"},
      {0x0001E130, "mm_lock2_release_v1"},
      {0x0001E1C0, "mm_lock3_acquire_nv"},
      {0x0001E1E0, "mm_lock3_acquire_v0"},
      {0x0001E1F0, "mm_lock3_acquire_v1"},
      {0x0001E180, "mm_lock3_release_nv"},
      {0x0001E1A0, "mm_lock3_release_v0"},
      {0x0001E1B0, "mm_lock3_release_v1"},
      {0x0001E240, "mm_lock4_acquire_nv"},
      {0x0001E260, "mm_lock4_acquire_v0"},
      {0x0001E270, "mm_lock4_acquire_v1"},
      {0x0001E200, "mm_lock4_release_nv"},
      {0x0001E220, "mm_lock4_release_v0"},
      {0x0001E230, "mm_lock4_release_v1"},
      {0x0001E2C0, "mm_lock5_acquire_nv"},
      {0x0001E2E0, "mm_lock5_acquire_v0"},
      {0x0001E2F0, "mm_lock5_acquire_v1"},
      {0x0001E280, "mm_lock5_release_nv"},
      {0x0001E2A0, "mm_lock5_release_v0"},
      {0x0001E2B0, "mm_lock5_release_v1"},
      {0x0001E340, "mm_lock6_acquire_nv"},
      {0x0001E360, "mm_lock6_acquire_v0"},
      {0x0001E370, "mm_lock6_acquire_v1"},
      {0x0001E300, "mm_lock6_release_nv"},
      {0x0001E320, "mm_lock6_release_v0"},
      {0x0001E330, "mm_lock6_release_v1"},
      {0x0001E3C0, "mm_lock7_acquire_nv"},
      {0x0001E3E0, "mm_lock7_acquire_v0"},
      {0x0001E3F0, "mm_lock7_acquire_v1"},
      {0x0001E380, "mm_lock7_release_nv"},
      {0x0001E3A0, "mm_lock7_release_v0"},
      {0x0001E3B0, "mm_lock7_release_v1"},
      {0x0001E440, "mm_lock8_acquire_nv"},
      {0x0001E460, "mm_lock8_acquire_v0"},
      {0x0001E470, "mm_lock8_acquire_v1"},
      {0x0001E400, "mm_lock8_release_nv"},
      {0x0001E420, "mm_lock8_release_v0"},
      {0x0001E430, "mm_lock8_release_v1"},
      {0x0001E4C0, "mm_lock9_acquire_nv"},
      {0x0001E4E0, "mm_lock9_acquire_v0"},
      {0x0001E4F0, "mm_lock9_acquire_v1"},
      {0x0001E480, "mm_lock9_release_nv"},
      {0x0001E4A0, "mm_lock9_release_v0"},
      {0x0001E4B0, "mm_lock9_release_v1"},
      {0x0001EF20, "mm_lock_event_value_control_0"},
      {0x0001EF24, "mm_lock_event_value_control_1"},
      {0x00012124, "mm_parity_failing_address"},
      {0x00011000, "mm_performance_control0"},
      {0x00011008, "mm_performance_control1"},
      {0x00011020, "mm_performance_counter0"},
      {0x00011080, "mm_performance_counter0_event_value"},
      {0x00011024, "mm_performance_counter1"},
      {0x00011084, "mm_performance_counter1_event_value"},
      {0x00014210, "mm_reserved0"},
      {0x00014214, "mm_reserved1"},
      {0x00014218, "mm_reserved2"},
      {0x0001421C, "mm_reserved3"},
      {0x00013000, "mm_reset_control"},
      {0x00016000, "mm_spare_reg"},
      {0x00014000, "mm_timer_control"},
      {0x000140FC, "mm_timer_high"},
      {0x000140F8, "mm_timer_low"},
      {0x000140F4, "mm_timer_trig_event_high_value"},
      {0x000140F0, "mm_timer_trig_event_low_value"},
      {0x000140D0, "mm_trace_control0"},
      {0x000140D4, "mm_trace_control1"},
      {0x000140E0, "mm_trace_event0"},
      {0x000140E4, "mm_trace_event1"},
      {0x000140D8, "mm_trace_status"},
      {0x00014100, "mm_watchpoint0"},
      {0x00014104, "mm_watchpoint1"}
   };

   // mem_tile_module registers
   memTileRegValueToName = {
      {0x0, "mem_performance_counter0"}
   };

   // shim_tile_module registers
   shimRegValueToName = {
      {0x00014F00, "shim_all_lock_state_value"},
      {0x00036000, "shim_bisr_cache_ctrl"},
      {0x00036010, "shim_bisr_cache_data0"},
      {0x00036014, "shim_bisr_cache_data1"},
      {0x00036018, "shim_bisr_cache_data2"},
      {0x0003601C, "shim_bisr_cache_data3"},
      {0x00036008, "shim_bisr_cache_status"},
      {0x00036020, "shim_bisr_test_data0"},
      {0x00036024, "shim_bisr_test_data1"},
      {0x00036028, "shim_bisr_test_data2"},
      {0x0003602C, "shim_bisr_test_data3"},
      {0x00036044, "shim_cssd_trigger"},
      {0x00034404, "shim_combo_event_control"},
      {0x00034400, "shim_combo_event_inputs"},
      {0x00036034, "shim_control_packet_handler_status"},
      {0x0001D00C, "shim_dma_bd0_axi_config"},
      {0x0001D000, "shim_dma_bd0_addr_low"},
      {0x0001D004, "shim_dma_bd0_buffer_length"},
      {0x0001D008, "shim_dma_bd0_control"},
      {0x0001D010, "shim_dma_bd0_packet"},
      {0x0001D0D4, "shim_dma_bd10_axi_config"},
      {0x0001D0C8, "shim_dma_bd10_addr_low"},
      {0x0001D0D0, "shim_dma_bd10_buffer_control"},
      {0x0001D0CC, "shim_dma_bd10_buffer_length"},
      {0x0001D0D8, "shim_dma_bd10_packet"},
      {0x0001D0E8, "shim_dma_bd11_axi_config"},
      {0x0001D0DC, "shim_dma_bd11_addr_low"},
      {0x0001D0E4, "shim_dma_bd11_buffer_control"},
      {0x0001D0E0, "shim_dma_bd11_buffer_length"},
      {0x0001D0EC, "shim_dma_bd11_packet"},
      {0x0001D0FC, "shim_dma_bd12_axi_config"},
      {0x0001D0F0, "shim_dma_bd12_addr_low"},
      {0x0001D0F8, "shim_dma_bd12_buffer_control"},
      {0x0001D0F4, "shim_dma_bd12_buffer_length"},
      {0x0001D100, "shim_dma_bd12_packet"},
      {0x0001D110, "shim_dma_bd13_axi_config"},
      {0x0001D104, "shim_dma_bd13_addr_low"},
      {0x0001D10C, "shim_dma_bd13_buffer_control"},
      {0x0001D108, "shim_dma_bd13_buffer_length"},
      {0x0001D114, "shim_dma_bd13_packet"},
      {0x0001D124, "shim_dma_bd14_axi_config"},
      {0x0001D118, "shim_dma_bd14_addr_low"},
      {0x0001D120, "shim_dma_bd14_buffer_control"},
      {0x0001D11C, "shim_dma_bd14_buffer_length"},
      {0x0001D128, "shim_dma_bd14_packet"},
      {0x0001D138, "shim_dma_bd15_axi_config"},
      {0x0001D12C, "shim_dma_bd15_addr_low"},
      {0x0001D134, "shim_dma_bd15_buffer_control"},
      {0x0001D130, "shim_dma_bd15_buffer_length"},
      {0x0001D13C, "shim_dma_bd15_packet"},
      {0x0001D020, "shim_dma_bd1_axi_config"},
      {0x0001D014, "shim_dma_bd1_addr_low"},
      {0x0001D01C, "shim_dma_bd1_buffer_control"},
      {0x0001D018, "shim_dma_bd1_buffer_length"},
      {0x0001D024, "shim_dma_bd1_packet"},
      {0x0001D034, "shim_dma_bd2_axi_config"},
      {0x0001D028, "shim_dma_bd2_addr_low"},
      {0x0001D030, "shim_dma_bd2_buffer_control"},
      {0x0001D02C, "shim_dma_bd2_buffer_length"},
      {0x0001D038, "shim_dma_bd2_packet"},
      {0x0001D048, "shim_dma_bd3_axi_config"},
      {0x0001D03C, "shim_dma_bd3_addr_low"},
      {0x0001D044, "shim_dma_bd3_buffer_control"},
      {0x0001D040, "shim_dma_bd3_buffer_length"},
      {0x0001D04C, "shim_dma_bd3_packet"},
      {0x0001D05C, "shim_dma_bd4_axi_config"},
      {0x0001D050, "shim_dma_bd4_addr_low"},
      {0x0001D058, "shim_dma_bd4_buffer_control"},
      {0x0001D054, "shim_dma_bd4_buffer_length"},
      {0x0001D060, "shim_dma_bd4_packet"},
      {0x0001D070, "shim_dma_bd5_axi_config"},
      {0x0001D064, "shim_dma_bd5_addr_low"},
      {0x0001D06C, "shim_dma_bd5_buffer_control"},
      {0x0001D068, "shim_dma_bd5_buffer_length"},
      {0x0001D074, "shim_dma_bd5_packet"},
      {0x0001D084, "shim_dma_bd6_axi_config"},
      {0x0001D078, "shim_dma_bd6_addr_low"},
      {0x0001D080, "shim_dma_bd6_buffer_control"},
      {0x0001D07C, "shim_dma_bd6_buffer_length"},
      {0x0001D088, "shim_dma_bd6_packet"},
      {0x0001D098, "shim_dma_bd7_axi_config"},
      {0x0001D08C, "shim_dma_bd7_addr_low"},
      {0x0001D094, "shim_dma_bd7_buffer_control"},
      {0x0001D090, "shim_dma_bd7_buffer_length"},
      {0x0001D09C, "shim_dma_bd7_packet"},
      {0x0001D0AC, "shim_dma_bd8_axi_config"},
      {0x0001D0A0, "shim_dma_bd8_addr_low"},
      {0x0001D0A8, "shim_dma_bd8_buffer_control"},
      {0x0001D0A4, "shim_dma_bd8_buffer_length"},
      {0x0001D0B0, "shim_dma_bd8_packet"},
      {0x0001D0C0, "shim_dma_bd9_axi_config"},
      {0x0001D0B4, "shim_dma_bd9_addr_low"},
      {0x0001D0BC, "shim_dma_bd9_buffer_control"},
      {0x0001D0B8, "shim_dma_bd9_buffer_length"},
      {0x0001D0C4, "shim_dma_bd9_packet"},
      {0x0001D150, "shim_dma_mm2s_0_ctrl"},
      {0x0001D154, "shim_dma_mm2s_0_start_queue"},
      {0x0001D158, "shim_dma_mm2s_1_ctrl"},
      {0x0001D15C, "shim_dma_mm2s_1_start_queue"},
      {0x0001D164, "shim_dma_mm2s_status"},
      {0x0001D140, "shim_dma_s2mm_0_ctrl"},
      {0x0001D144, "shim_dma_s2mm_0_start_queue"},
      {0x0001D148, "shim_dma_s2mm_1_ctrl"},
      {0x0001D14C, "shim_dma_s2mm_1_start_queue"},
      {0x0001D160, "shim_dma_s2mm_status"},
      {0x0001F004, "shim_demux_config"},
      {0x00034010, "shim_event_broadcast_a_0"},
      {0x00034038, "shim_event_broadcast_a_10"},
      {0x0003403C, "shim_event_broadcast_a_11"},
      {0x00034040, "shim_event_broadcast_a_12"},
      {0x00034044, "shim_event_broadcast_a_13"},
      {0x00034048, "shim_event_broadcast_a_14"},
      {0x0003404C, "shim_event_broadcast_a_15"},
      {0x00034014, "shim_event_broadcast_a_1"},
      {0x00034018, "shim_event_broadcast_a_2"},
      {0x0003401C, "shim_event_broadcast_a_3"},
      {0x00034020, "shim_event_broadcast_a_4"},
      {0x00034024, "shim_event_broadcast_a_5"},
      {0x00034028, "shim_event_broadcast_a_6"},
      {0x0003402C, "shim_event_broadcast_a_7"},
      {0x00034030, "shim_event_broadcast_a_8"},
      {0x00034034, "shim_event_broadcast_a_9"},
      {0x00034084, "shim_event_broadcast_a_block_east_clr"},
      {0x00034080, "shim_event_broadcast_a_block_east_set"},
      {0x00034088, "shim_event_broadcast_a_block_east_value"},
      {0x00034074, "shim_event_broadcast_a_block_north_clr"},
      {0x00034070, "shim_event_broadcast_a_block_north_set"},
      {0x00034078, "shim_event_broadcast_a_block_north_value"},
      {0x00034054, "shim_event_broadcast_a_block_south_clr"},
      {0x00034050, "shim_event_broadcast_a_block_south_set"},
      {0x00034058, "shim_event_broadcast_a_block_south_value"},
      {0x00034064, "shim_event_broadcast_a_block_west_clr"},
      {0x00034060, "shim_event_broadcast_a_block_west_set"},
      {0x00034068, "shim_event_broadcast_a_block_west_value"},
      {0x000340C4, "shim_event_broadcast_b_block_east_clr"},
      {0x000340C0, "shim_event_broadcast_b_block_east_set"},
      {0x000340C8, "shim_event_broadcast_b_block_east_value"},
      {0x000340B4, "shim_event_broadcast_b_block_north_clr"},
      {0x000340B0, "shim_event_broadcast_b_block_north_set"},
      {0x000340B8, "shim_event_broadcast_b_block_north_value"},
      {0x00034094, "shim_event_broadcast_b_block_south_clr"},
      {0x00034090, "shim_event_broadcast_b_block_south_set"},
      {0x00034098, "shim_event_broadcast_b_block_south_value"},
      {0x000340A4, "shim_event_broadcast_b_block_west_clr"},
      {0x000340A0, "shim_event_broadcast_b_block_west_set"},
      {0x000340A8, "shim_event_broadcast_b_block_west_value"},
      {0x00034008, "shim_event_generate"},
      {0x00034500, "shim_event_group_0_enable"},
      {0x00034514, "shim_event_group_broadcast_a_enable"},
      {0x00034504, "shim_event_group_dma_enable"},
      {0x0003450C, "shim_event_group_errors_enable"},
      {0x00034508, "shim_event_group_lock_enable"},
      {0x00034510, "shim_event_group_stream_switch_enable"},
      {0x00034518, "shim_event_group_user_enable"},
      {0x00034200, "shim_event_status0"},
      {0x00034204, "shim_event_status1"},
      {0x00034208, "shim_event_status2"},
      {0x0003420C, "shim_event_status3"},
      {0x0003501C, "shim_interrupt_controller_1st_level_block_north_in_a_clear"},
      {0x00035018, "shim_interrupt_controller_1st_level_block_north_in_a_set"},
      {0x00035020, "shim_interrupt_controller_1st_level_block_north_in_a_value"},
      {0x0003504C, "shim_interrupt_controller_1st_level_block_north_in_b_clear"},
      {0x00035048, "shim_interrupt_controller_1st_level_block_north_in_b_set"},
      {0x00035050, "shim_interrupt_controller_1st_level_block_north_in_b_value"},
      {0x00035008, "shim_interrupt_controller_1st_level_disable_a"},
      {0x00035038, "shim_interrupt_controller_1st_level_disable_b"},
      {0x00035004, "shim_interrupt_controller_1st_level_enable_a"},
      {0x00035034, "shim_interrupt_controller_1st_level_enable_b"},
      {0x00035014, "shim_interrupt_controller_1st_level_irq_event_a"},
      {0x00035044, "shim_interrupt_controller_1st_level_irq_event_b"},
      {0x00035010, "shim_interrupt_controller_1st_level_irq_no_a"},
      {0x00035040, "shim_interrupt_controller_1st_level_irq_no_b"},
      {0x00035000, "shim_interrupt_controller_1st_level_mask_a"},
      {0x00035030, "shim_interrupt_controller_1st_level_mask_b"},
      {0x0003500C, "shim_interrupt_controller_1st_level_status_a"},
      {0x0003503C, "shim_interrupt_controller_1st_level_status_b"},
      {0x00015008, "shim_interrupt_controller_2nd_level_disable"},
      {0x00015004, "shim_interrupt_controller_2nd_level_enable"},
      {0x00015010, "shim_interrupt_controller_2nd_level_interrupt"},
      {0x00015000, "shim_interrupt_controller_2nd_level_mask"},
      {0x0001500C, "shim_interrupt_controller_2nd_level_status"},
      {0x00014040, "shim_lock0_acquire_nv"},
      {0x00014060, "shim_lock0_acquire_v0"},
      {0x00014070, "shim_lock0_acquire_v1"},
      {0x00014000, "shim_lock0_release_nv"},
      {0x00014020, "shim_lock0_release_v0"},
      {0x00014030, "shim_lock0_release_v1"},
      {0x00014540, "shim_lock10_acquire_nv"},
      {0x00014560, "shim_lock10_acquire_v0"},
      {0x00014570, "shim_lock10_acquire_v1"},
      {0x00014500, "shim_lock10_release_nv"},
      {0x00014520, "shim_lock10_release_v0"},
      {0x00014530, "shim_lock10_release_v1"},
      {0x000145C0, "shim_lock11_acquire_nv"},
      {0x000145E0, "shim_lock11_acquire_v0"},
      {0x000145F0, "shim_lock11_acquire_v1"},
      {0x00014580, "shim_lock11_release_nv"},
      {0x000145A0, "shim_lock11_release_v0"},
      {0x000145B0, "shim_lock11_release_v1"},
      {0x00014640, "shim_lock12_acquire_nv"},
      {0x00014660, "shim_lock12_acquire_v0"},
      {0x00014670, "shim_lock12_acquire_v1"},
      {0x00014600, "shim_lock12_release_nv"},
      {0x00014620, "shim_lock12_release_v0"},
      {0x00014630, "shim_lock12_release_v1"},
      {0x000146C0, "shim_lock13_acquire_nv"},
      {0x000146E0, "shim_lock13_acquire_v0"},
      {0x000146F0, "shim_lock13_acquire_v1"},
      {0x00014680, "shim_lock13_release_nv"},
      {0x000146A0, "shim_lock13_release_v0"},
      {0x000146B0, "shim_lock13_release_v1"},
      {0x00014740, "shim_lock14_acquire_nv"},
      {0x00014760, "shim_lock14_acquire_v0"},
      {0x00014770, "shim_lock14_acquire_v1"},
      {0x00014700, "shim_lock14_release_nv"},
      {0x00014720, "shim_lock14_release_v0"},
      {0x00014730, "shim_lock14_release_v1"},
      {0x000147C0, "shim_lock15_acquire_nv"},
      {0x000147E0, "shim_lock15_acquire_v0"},
      {0x000147F0, "shim_lock15_acquire_v1"},
      {0x00014780, "shim_lock15_release_nv"},
      {0x000147A0, "shim_lock15_release_v0"},
      {0x000147B0, "shim_lock15_release_v1"},
      {0x000140C0, "shim_lock1_acquire_nv"},
      {0x000140E0, "shim_lock1_acquire_v0"},
      {0x000140F0, "shim_lock1_acquire_v1"},
      {0x00014080, "shim_lock1_release_nv"},
      {0x000140A0, "shim_lock1_release_v0"},
      {0x000140B0, "shim_lock1_release_v1"},
      {0x00014140, "shim_lock2_acquire_nv"},
      {0x00014160, "shim_lock2_acquire_v0"},
      {0x00014170, "shim_lock2_acquire_v1"},
      {0x00014100, "shim_lock2_release_nv"},
      {0x00014120, "shim_lock2_release_v0"},
      {0x00014130, "shim_lock2_release_v1"},
      {0x000141C0, "shim_lock3_acquire_nv"},
      {0x000141E0, "shim_lock3_acquire_v0"},
      {0x000141F0, "shim_lock3_acquire_v1"},
      {0x00014180, "shim_lock3_release_nv"},
      {0x000141A0, "shim_lock3_release_v0"},
      {0x000141B0, "shim_lock3_release_v1"},
      {0x00014240, "shim_lock4_acquire_nv"},
      {0x00014260, "shim_lock4_acquire_v0"},
      {0x00014270, "shim_lock4_acquire_v1"},
      {0x00014200, "shim_lock4_release_nv"},
      {0x00014220, "shim_lock4_release_v0"},
      {0x00014230, "shim_lock4_release_v1"},
      {0x000142C0, "shim_lock5_acquire_nv"},
      {0x000142E0, "shim_lock5_acquire_v0"},
      {0x000142F0, "shim_lock5_acquire_v1"},
      {0x00014280, "shim_lock5_release_nv"},
      {0x000142A0, "shim_lock5_release_v0"},
      {0x000142B0, "shim_lock5_release_v1"},
      {0x00014340, "shim_lock6_acquire_nv"},
      {0x00014360, "shim_lock6_acquire_v0"},
      {0x00014370, "shim_lock6_acquire_v1"},
      {0x00014300, "shim_lock6_release_nv"},
      {0x00014320, "shim_lock6_release_v0"},
      {0x00014330, "shim_lock6_release_v1"},
      {0x000143C0, "shim_lock7_acquire_nv"},
      {0x000143E0, "shim_lock7_acquire_v0"},
      {0x000143F0, "shim_lock7_acquire_v1"},
      {0x00014380, "shim_lock7_release_nv"},
      {0x000143A0, "shim_lock7_release_v0"},
      {0x000143B0, "shim_lock7_release_v1"},
      {0x00014440, "shim_lock8_acquire_nv"},
      {0x00014460, "shim_lock8_acquire_v0"},
      {0x00014470, "shim_lock8_acquire_v1"},
      {0x00014400, "shim_lock8_release_nv"},
      {0x00014420, "shim_lock8_release_v0"},
      {0x00014430, "shim_lock8_release_v1"},
      {0x000144C0, "shim_lock9_acquire_nv"},
      {0x000144E0, "shim_lock9_acquire_v0"},
      {0x000144F0, "shim_lock9_acquire_v1"},
      {0x00014480, "shim_lock9_release_nv"},
      {0x000144A0, "shim_lock9_release_v0"},
      {0x000144B0, "shim_lock9_release_v1"},
      {0x00014F20, "shim_lock_event_value_control_0"},
      {0x00014F24, "shim_lock_event_value_control_1"},
      {0x0001E020, "shim_me_aximm_config"},
      {0x0003604C, "shim_me_shim_reset_enable"},
      {0x00036048, "shim_me_tile_column_reset"},
      {0x0001F000, "shim_mux_config"},
      {0x0001E008, "shim_noc_interface_me_to_noc_south2"},
      {0x0001E00C, "shim_noc_interface_me_to_noc_south3"},
      {0x0001E010, "shim_noc_interface_me_to_noc_south4"},
      {0x0001E014, "shim_noc_interface_me_to_noc_south5"},
      {0x0003300C, "shim_pl_interface_downsizer_bypass"},
      {0x00033004, "shim_pl_interface_downsizer_config"},
      {0x00033008, "shim_pl_interface_downsizer_enable"},
      {0x00033000, "shim_pl_interface_upsizer_config"},
      {0x00031020, "shim_performance_counter0"},
      {0x00031080, "shim_performance_counter0_event_value"},
      {0x00031024, "shim_performance_counter1"},
      {0x00031084, "shim_performance_counter1_event_value"},
      {0x00031000, "shim_performance_control0"},
      {0x00031000, "shim_performance_start_stop_0_1"},
      {0x00031008, "shim_performance_control1"},
      {0x00031008, "shim_performance_reset_0_1"},
      {0x00034210, "shim_reserved0"},
      {0x00034214, "shim_reserved1"},
      {0x00034218, "shim_reserved2"},
      {0x0003421C, "shim_reserved3"},
      {0x00016000, "shim_spare_reg"},
      {0x00036050, "shim_spare_reg"},
      {0x0003FF00, "shim_stream_switch_event_port_selection_0"},
      {0x0003FF04, "shim_stream_switch_event_port_selection_1"},
      {0x0003F04C, "shim_stream_switch_master_config_east0"},
      {0x0003F050, "shim_stream_switch_master_config_east1"},
      {0x0003F054, "shim_stream_switch_master_config_east2"},
      {0x0003F058, "shim_stream_switch_master_config_east3"},
      {0x0003F004, "shim_stream_switch_master_config_fifo0"},
      {0x0003F008, "shim_stream_switch_master_config_fifo1"},
      {0x0003F034, "shim_stream_switch_master_config_north0"},
      {0x0003F038, "shim_stream_switch_master_config_north1"},
      {0x0003F03C, "shim_stream_switch_master_config_north2"},
      {0x0003F040, "shim_stream_switch_master_config_north3"},
      {0x0003F044, "shim_stream_switch_master_config_north4"},
      {0x0003F048, "shim_stream_switch_master_config_north5"},
      {0x0003F00C, "shim_stream_switch_master_config_south0"},
      {0x0003F010, "shim_stream_switch_master_config_south1"},
      {0x0003F014, "shim_stream_switch_master_config_south2"},
      {0x0003F018, "shim_stream_switch_master_config_south3"},
      {0x0003F01C, "shim_stream_switch_master_config_south4"},
      {0x0003F020, "shim_stream_switch_master_config_south5"},
      {0x0003F000, "shim_stream_switch_master_config_tile_ctrl"},
      {0x0003F024, "shim_stream_switch_master_config_west0"},
      {0x0003F028, "shim_stream_switch_master_config_west1"},
      {0x0003F02C, "shim_stream_switch_master_config_west2"},
      {0x0003F030, "shim_stream_switch_master_config_west3"},
      {0x0003F14C, "shim_stream_switch_slave_east_0_config"},
      {0x0003F330, "shim_stream_switch_slave_east_0_slot0"},
      {0x0003F334, "shim_stream_switch_slave_east_0_slot1"},
      {0x0003F338, "shim_stream_switch_slave_east_0_slot2"},
      {0x0003F33C, "shim_stream_switch_slave_east_0_slot3"},
      {0x0003F150, "shim_stream_switch_slave_east_1_config"},
      {0x0003F340, "shim_stream_switch_slave_east_1_slot0"},
      {0x0003F344, "shim_stream_switch_slave_east_1_slot1"},
      {0x0003F348, "shim_stream_switch_slave_east_1_slot2"},
      {0x0003F34C, "shim_stream_switch_slave_east_1_slot3"},
      {0x0003F154, "shim_stream_switch_slave_east_2_config"},
      {0x0003F350, "shim_stream_switch_slave_east_2_slot0"},
      {0x0003F354, "shim_stream_switch_slave_east_2_slot1"},
      {0x0003F358, "shim_stream_switch_slave_east_2_slot2"},
      {0x0003F35C, "shim_stream_switch_slave_east_2_slot3"},
      {0x0003F158, "shim_stream_switch_slave_east_3_config"},
      {0x0003F360, "shim_stream_switch_slave_east_3_slot0"},
      {0x0003F364, "shim_stream_switch_slave_east_3_slot1"},
      {0x0003F368, "shim_stream_switch_slave_east_3_slot2"},
      {0x0003F36C, "shim_stream_switch_slave_east_3_slot3"},
      {0x0003F104, "shim_stream_switch_slave_fifo_0_config"},
      {0x0003F210, "shim_stream_switch_slave_fifo_0_slot0"},
      {0x0003F214, "shim_stream_switch_slave_fifo_0_slot1"},
      {0x0003F218, "shim_stream_switch_slave_fifo_0_slot2"},
      {0x0003F21C, "shim_stream_switch_slave_fifo_0_slot3"},
      {0x0003F108, "shim_stream_switch_slave_fifo_1_config"},
      {0x0003F220, "shim_stream_switch_slave_fifo_1_slot0"},
      {0x0003F224, "shim_stream_switch_slave_fifo_1_slot1"},
      {0x0003F228, "shim_stream_switch_slave_fifo_1_slot2"},
      {0x0003F22C, "shim_stream_switch_slave_fifo_1_slot3"},
      {0x0003F13C, "shim_stream_switch_slave_north_0_config"},
      {0x0003F2F0, "shim_stream_switch_slave_north_0_slot0"},
      {0x0003F2F4, "shim_stream_switch_slave_north_0_slot1"},
      {0x0003F2F8, "shim_stream_switch_slave_north_0_slot2"},
      {0x0003F2FC, "shim_stream_switch_slave_north_0_slot3"},
      {0x0003F140, "shim_stream_switch_slave_north_1_config"},
      {0x0003F300, "shim_stream_switch_slave_north_1_slot0"},
      {0x0003F304, "shim_stream_switch_slave_north_1_slot1"},
      {0x0003F308, "shim_stream_switch_slave_north_1_slot2"},
      {0x0003F30C, "shim_stream_switch_slave_north_1_slot3"},
      {0x0003F144, "shim_stream_switch_slave_north_2_config"},
      {0x0003F310, "shim_stream_switch_slave_north_2_slot0"},
      {0x0003F314, "shim_stream_switch_slave_north_2_slot1"},
      {0x0003F318, "shim_stream_switch_slave_north_2_slot2"},
      {0x0003F31C, "shim_stream_switch_slave_north_2_slot3"},
      {0x0003F148, "shim_stream_switch_slave_north_3_config"},
      {0x0003F320, "shim_stream_switch_slave_north_3_slot0"},
      {0x0003F324, "shim_stream_switch_slave_north_3_slot1"},
      {0x0003F328, "shim_stream_switch_slave_north_3_slot2"},
      {0x0003F32C, "shim_stream_switch_slave_north_3_slot3"},
      {0x0003F10C, "shim_stream_switch_slave_south_0_config"},
      {0x0003F230, "shim_stream_switch_slave_south_0_slot0"},
      {0x0003F234, "shim_stream_switch_slave_south_0_slot1"},
      {0x0003F238, "shim_stream_switch_slave_south_0_slot2"},
      {0x0003F23C, "shim_stream_switch_slave_south_0_slot3"},
      {0x0003F110, "shim_stream_switch_slave_south_1_config"},
      {0x0003F240, "shim_stream_switch_slave_south_1_slot0"},
      {0x0003F244, "shim_stream_switch_slave_south_1_slot1"},
      {0x0003F248, "shim_stream_switch_slave_south_1_slot2"},
      {0x0003F24C, "shim_stream_switch_slave_south_1_slot3"},
      {0x0003F114, "shim_stream_switch_slave_south_2_config"},
      {0x0003F250, "shim_stream_switch_slave_south_2_slot0"},
      {0x0003F254, "shim_stream_switch_slave_south_2_slot1"},
      {0x0003F258, "shim_stream_switch_slave_south_2_slot2"},
      {0x0003F25C, "shim_stream_switch_slave_south_2_slot3"},
      {0x0003F118, "shim_stream_switch_slave_south_3_config"},
      {0x0003F260, "shim_stream_switch_slave_south_3_slot0"},
      {0x0003F264, "shim_stream_switch_slave_south_3_slot1"},
      {0x0003F268, "shim_stream_switch_slave_south_3_slot2"},
      {0x0003F26C, "shim_stream_switch_slave_south_3_slot3"},
      {0x0003F11C, "shim_stream_switch_slave_south_4_config"},
      {0x0003F270, "shim_stream_switch_slave_south_4_slot0"},
      {0x0003F274, "shim_stream_switch_slave_south_4_slot1"},
      {0x0003F278, "shim_stream_switch_slave_south_4_slot2"},
      {0x0003F27C, "shim_stream_switch_slave_south_4_slot3"},
      {0x0003F120, "shim_stream_switch_slave_south_5_config"},
      {0x0003F280, "shim_stream_switch_slave_south_5_slot0"},
      {0x0003F284, "shim_stream_switch_slave_south_5_slot1"},
      {0x0003F288, "shim_stream_switch_slave_south_5_slot2"},
      {0x0003F28C, "shim_stream_switch_slave_south_5_slot3"},
      {0x0003F124, "shim_stream_switch_slave_south_6_config"},
      {0x0003F290, "shim_stream_switch_slave_south_6_slot0"},
      {0x0003F294, "shim_stream_switch_slave_south_6_slot1"},
      {0x0003F298, "shim_stream_switch_slave_south_6_slot2"},
      {0x0003F29C, "shim_stream_switch_slave_south_6_slot3"},
      {0x0003F128, "shim_stream_switch_slave_south_7_config"},
      {0x0003F2A0, "shim_stream_switch_slave_south_7_slot0"},
      {0x0003F2A4, "shim_stream_switch_slave_south_7_slot1"},
      {0x0003F2A8, "shim_stream_switch_slave_south_7_slot2"},
      {0x0003F2AC, "shim_stream_switch_slave_south_7_slot3"},
      {0x0003F100, "shim_stream_switch_slave_tile_ctrl_config"},
      {0x0003F200, "shim_stream_switch_slave_tile_ctrl_slot0"},
      {0x0003F204, "shim_stream_switch_slave_tile_ctrl_slot1"},
      {0x0003F208, "shim_stream_switch_slave_tile_ctrl_slot2"},
      {0x0003F20C, "shim_stream_switch_slave_tile_ctrl_slot3"},
      {0x0003F15C, "shim_stream_switch_slave_trace_config"},
      {0x0003F370, "shim_stream_switch_slave_trace_slot0"},
      {0x0003F374, "shim_stream_switch_slave_trace_slot1"},
      {0x0003F378, "shim_stream_switch_slave_trace_slot2"},
      {0x0003F37C, "shim_stream_switch_slave_trace_slot3"},
      {0x0003F12C, "shim_stream_switch_slave_west_0_config"},
      {0x0003F2B0, "shim_stream_switch_slave_west_0_slot0"},
      {0x0003F2B4, "shim_stream_switch_slave_west_0_slot1"},
      {0x0003F2B8, "shim_stream_switch_slave_west_0_slot2"},
      {0x0003F2BC, "shim_stream_switch_slave_west_0_slot3"},
      {0x0003F130, "shim_stream_switch_slave_west_1_config"},
      {0x0003F2C0, "shim_stream_switch_slave_west_1_slot0"},
      {0x0003F2C4, "shim_stream_switch_slave_west_1_slot1"},
      {0x0003F2C8, "shim_stream_switch_slave_west_1_slot2"},
      {0x0003F2CC, "shim_stream_switch_slave_west_1_slot3"},
      {0x0003F134, "shim_stream_switch_slave_west_2_config"},
      {0x0003F2D0, "shim_stream_switch_slave_west_2_slot0"},
      {0x0003F2D4, "shim_stream_switch_slave_west_2_slot1"},
      {0x0003F2D8, "shim_stream_switch_slave_west_2_slot2"},
      {0x0003F2DC, "shim_stream_switch_slave_west_2_slot3"},
      {0x0003F138, "shim_stream_switch_slave_west_3_config"},
      {0x0003F2E0, "shim_stream_switch_slave_west_3_slot0"},
      {0x0003F2E4, "shim_stream_switch_slave_west_3_slot1"},
      {0x0003F2E8, "shim_stream_switch_slave_west_3_slot2"},
      {0x0003F2EC, "shim_stream_switch_slave_west_3_slot3"},
      {0x00036040, "shim_tile_clock_control"},
      {0x00036030, "shim_tile_control"},
      {0x00034000, "shim_timer_control"},
      {0x000340FC, "shim_timer_high"},
      {0x000340F8, "shim_timer_low"},
      {0x000340F4, "shim_timer_trig_event_high_value"},
      {0x000340F0, "shim_timer_trig_event_low_value"},
      {0x000340D0, "shim_trace_control0"},
      {0x000340D4, "shim_trace_control1"},
      {0x000340E0, "shim_trace_event0"},
      {0x000340E4, "shim_trace_event1"},
      {0x000340D8, "shim_trace_status"},
      {0x14, "shim_dma_bd_step_size"},
      {0x8, "shim_dma_s2mm_step_size"}
   };
}

/*
  void populateRegAddrToSizeMap() {
   regAddrToSize[0x00020000] = 128;
   regAddrToSize[0x00024000] = 128;
   regAddrToSize[0x00030100] = 20;
   regAddrToSize[0x00030110] = 20;
   regAddrToSize[0x00030120] = 20;
   regAddrToSize[0x00030130] = 20;
   regAddrToSize[0x00030140] = 20;
   regAddrToSize[0x00030150] = 20;
   regAddrToSize[0x00030160] = 20;
   regAddrToSize[0x00030170] = 20;
   regAddrToSize[0x00030280] = 20;
   regAddrToSize[0x00030290] = 20;
   regAddrToSize[0x000302A0] = 20;
   regAddrToSize[0x000302B0] = 20;
   regAddrToSize[0x000302C0] = 20;
   regAddrToSize[0x000302D0] = 20;
   regAddrToSize[0x000302E0] = 20;
   regAddrToSize[0x000302F0] = 20;
   regAddrToSize[0x00030300] = 20;
   regAddrToSize[0x00030310] = 20;
   regAddrToSize[0x00030320] = 20;
   regAddrToSize[0x00030330] = 20;
   regAddrToSize[0x00030340] = 20;
   regAddrToSize[0x00030350] = 20;
   regAddrToSize[0x00030360] = 20;
   regAddrToSize[0x00030370] = 20;
   regAddrToSize[0x00030380] = 20;
   regAddrToSize[0x00030390] = 20;
   regAddrToSize[0x000303A0] = 20;
   regAddrToSize[0x000303B0] = 20;
   regAddrToSize[0x000303C0] = 20;
   regAddrToSize[0x000303D0] = 20;
   regAddrToSize[0x000303E0] = 20;
   regAddrToSize[0x000303F0] = 20;
   regAddrToSize[0x00030400] = 20;
   regAddrToSize[0x00030410] = 20;
   regAddrToSize[0x00030420] = 20;
   regAddrToSize[0x00030430] = 20;
   regAddrToSize[0x00030480] = 8;
   regAddrToSize[0x00030490] = 8;
   regAddrToSize[0x000304A0] = 8;
   regAddrToSize[0x000304B0] = 8;
   regAddrToSize[0x000304C0] = 8;
   regAddrToSize[0x000304D0] = 8;
   regAddrToSize[0x000304E0] = 8;
   regAddrToSize[0x000304F0] = 8;
   regAddrToSize[0x00030500] = 20;
   regAddrToSize[0x00030510] = 20;
   regAddrToSize[0x00030530] = 128;
   regAddrToSize[0x00030540] = 128;
   regAddrToSize[0x00030550] = 128;
   regAddrToSize[0x00030560] = 128;
   regAddrToSize[0x00030570] = 128;
   regAddrToSize[0x00030580] = 128;
   regAddrToSize[0x00030590] = 128;
   regAddrToSize[0x000305A0] = 128;
   regAddrToSize[0x000305B0] = 128;
   regAddrToSize[0x000305C0] = 128;
   regAddrToSize[0x000305D0] = 128;
   regAddrToSize[0x000305E0] = 128;
   regAddrToSize[0x000305F0] = 128;
   regAddrToSize[0x00030600] = 128;
   regAddrToSize[0x00030610] = 128;
   regAddrToSize[0x00030620] = 128;
   regAddrToSize[0x00030630] = 128;
   regAddrToSize[0x00030640] = 128;
   regAddrToSize[0x00030650] = 128;
   regAddrToSize[0x00030660] = 128;
   regAddrToSize[0x00030670] = 128;
   regAddrToSize[0x00030680] = 128;
   regAddrToSize[0x00030690] = 128;
   regAddrToSize[0x000306A0] = 128;
   regAddrToSize[0x000306B0] = 128;
   regAddrToSize[0x000306C0] = 128;
   regAddrToSize[0x000306D0] = 128;
   regAddrToSize[0x000306E0] = 128;
   regAddrToSize[0x000306F0] = 128;
   regAddrToSize[0x00030700] = 128;
   regAddrToSize[0x00030710] = 128;
   regAddrToSize[0x00030720] = 128;
   regAddrToSize[0x00030730] = 128;
   regAddrToSize[0x00030740] = 128;
   regAddrToSize[0x00030750] = 128;
   regAddrToSize[0x00030760] = 128;
   regAddrToSize[0x00030770] = 128;
   regAddrToSize[0x00030780] = 128;
   regAddrToSize[0x00030790] = 128;
   regAddrToSize[0x000307A0] = 128;
  }
*/
void AIE1UsedRegisters::populateRegAddrToSizeMap() {
    // core_module registers
    coreRegAddrToSize = {
        {0x00020000, 128},
        {0x00024000, 128},
        {0x00030100, 20},
        {0x00030110, 20},
        {0x00030120, 20},
        {0x00030130, 20},
        {0x00030140, 20},
        {0x00030150, 20},
        {0x00030160, 20},
        {0x00030170, 20},
        {0x00030280, 20},
        {0x00030290, 20},
        {0x000302A0, 20},
        {0x000302B0, 20},
        {0x000302C0, 20},
        {0x000302D0, 20},
        {0x000302E0, 20},
        {0x000302F0, 20},
        {0x00030300, 20},
        {0x00030310, 20},
        {0x00030320, 20},
        {0x00030330, 20},
        {0x00030340, 20},
        {0x00030350, 20},
        {0x00030360, 20},
        {0x00030370, 20},
        {0x00030380, 20},
        {0x00030390, 20},
        {0x000303A0, 20},
        {0x000303B0, 20},
        {0x000303C0, 20},
        {0x000303D0, 20},
        {0x000303E0, 20},
        {0x000303F0, 20},
        {0x00030400, 20},
        {0x00030410, 20},
        {0x00030420, 20},
        {0x00030430, 20},
        {0x00030480, 8},
        {0x00030490, 8},
        {0x000304A0, 8},
        {0x000304B0, 8},
        {0x000304C0, 8},
        {0x000304D0, 8},
        {0x000304E0, 8},
        {0x000304F0, 8},
        {0x00030500, 20},
        {0x00030510, 20},
        {0x00030530, 128},
        {0x00030540, 128},
        {0x00030550, 128},
        {0x00030560, 128},
        {0x00030570, 128},
        {0x00030580, 128},
        {0x00030590, 128},
        {0x000305A0, 128},
        {0x000305B0, 128},
        {0x000305C0, 128},
        {0x000305D0, 128},
        {0x000305E0, 128},
        {0x000305F0, 128},
        {0x00030600, 128},
        {0x00030610, 128},
        {0x00030620, 128},
        {0x00030630, 128},
        {0x00030640, 128},
        {0x00030650, 128},
        {0x00030660, 128},
        {0x00030670, 128},
        {0x00030680, 128},
        {0x00030690, 128},
        {0x000306A0, 128},
        {0x000306B0, 128},
        {0x000306C0, 128},
        {0x000306D0, 128},
        {0x000306E0, 128},
        {0x000306F0, 128},
        {0x00030700, 128},
        {0x00030710, 128},
        {0x00030720, 128},
        {0x00030730, 128},
        {0x00030740, 128},
        {0x00030750, 128},
        {0x00030760, 128},
        {0x00030770, 128},
        {0x00030780, 128},
        {0x00030790, 128},
        {0x000307A0, 128}
    };

    // memory_module registers
    memoryRegAddrToSize = {
    };

    // mem_tile_module registers
    memTileRegAddrToSize = {
    };

    // shim_tile_module registers
    shimRegAddrToSize = {
    };
}
}
