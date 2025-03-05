
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

#include "xdp/profile/plugin/aie_debug/generations/aie2_registers.h"
//#include "xdp/profile/plugin/aie_debug/aie_debug_metadata.h"
#include "xdp/profile/plugin/aie_debug/used_registers.h"


namespace xdp {
  void AIE2UsedRegisters::populateProfileRegisters() {

    // Core modules
    core_addresses.emplace(aie2::cm_performance_control0);
    core_addresses.emplace(aie2::cm_performance_control1);
    core_addresses.emplace(aie2::cm_performance_control2);
    core_addresses.emplace(aie2::cm_performance_counter0);
    core_addresses.emplace(aie2::cm_performance_counter1);
    core_addresses.emplace(aie2::cm_performance_counter2);
    core_addresses.emplace(aie2::cm_performance_counter3);
    core_addresses.emplace(aie2::cm_performance_counter0_event_value);
    core_addresses.emplace(aie2::cm_performance_counter1_event_value);
    core_addresses.emplace(aie2::cm_performance_counter2_event_value);
    core_addresses.emplace(aie2::cm_performance_counter3_event_value);

    // Memory modules
    memory_addresses.emplace(aie2::mm_performance_control0);
    memory_addresses.emplace(aie2::mm_performance_control1);
    memory_addresses.emplace(aie2::mm_performance_counter0);
    memory_addresses.emplace(aie2::mm_performance_counter1);
    memory_addresses.emplace(aie2::mm_performance_counter0_event_value);
    memory_addresses.emplace(aie2::mm_performance_counter1_event_value);

    // Interface tiles
    interface_addresses.emplace(aie2::shim_performance_control0);
    interface_addresses.emplace(aie2::shim_performance_control1);
    interface_addresses.emplace(aie2::shim_performance_counter0);
    interface_addresses.emplace(aie2::shim_performance_counter1);
    interface_addresses.emplace(aie2::shim_performance_counter0_event_value);
    interface_addresses.emplace(aie2::shim_performance_counter1_event_value);

    // Memory tiles
    memory_tile_addresses.emplace(aie2::mem_performance_control0);
    memory_tile_addresses.emplace(aie2::mem_performance_control1);
    memory_tile_addresses.emplace(aie2::mem_performance_control2);
    memory_tile_addresses.emplace(aie2::mem_performance_counter0);
    memory_tile_addresses.emplace(aie2::mem_performance_counter1);
    memory_tile_addresses.emplace(aie2::mem_performance_counter2);
    memory_tile_addresses.emplace(aie2::mem_performance_counter3);
    memory_tile_addresses.emplace(aie2::mem_performance_counter0_event_value);
    memory_tile_addresses.emplace(aie2::mem_performance_counter1_event_value);
    memory_tile_addresses.emplace(aie2::mem_performance_counter2_event_value);
    memory_tile_addresses.emplace(aie2::mem_performance_counter3_event_value);
  }

  void AIE2UsedRegisters::populateTraceRegisters() {
    // Core modules
    core_addresses.emplace(aie2::cm_core_status);
    core_addresses.emplace(aie2::cm_trace_control0);
    core_addresses.emplace(aie2::cm_trace_control1);
    core_addresses.emplace(aie2::cm_trace_status);
    core_addresses.emplace(aie2::cm_trace_event0);
    core_addresses.emplace(aie2::cm_trace_event1);
    core_addresses.emplace(aie2::cm_event_status0);
    core_addresses.emplace(aie2::cm_event_status1);
    core_addresses.emplace(aie2::cm_event_status2);
    core_addresses.emplace(aie2::cm_event_status3);
    core_addresses.emplace(aie2::cm_event_broadcast0);
    core_addresses.emplace(aie2::cm_event_broadcast1);
    core_addresses.emplace(aie2::cm_event_broadcast2);
    core_addresses.emplace(aie2::cm_event_broadcast3);
    core_addresses.emplace(aie2::cm_event_broadcast4);
    core_addresses.emplace(aie2::cm_event_broadcast5);
    core_addresses.emplace(aie2::cm_event_broadcast6);
    core_addresses.emplace(aie2::cm_event_broadcast7);
    core_addresses.emplace(aie2::cm_event_broadcast8);
    core_addresses.emplace(aie2::cm_event_broadcast9);
    core_addresses.emplace(aie2::cm_event_broadcast10);
    core_addresses.emplace(aie2::cm_event_broadcast11);
    core_addresses.emplace(aie2::cm_event_broadcast12);
    core_addresses.emplace(aie2::cm_event_broadcast13);
    core_addresses.emplace(aie2::cm_event_broadcast14);
    core_addresses.emplace(aie2::cm_event_broadcast15);
    core_addresses.emplace(aie2::cm_timer_trig_event_low_value);
    core_addresses.emplace(aie2::cm_timer_trig_event_high_value);
    core_addresses.emplace(aie2::cm_timer_low);
    core_addresses.emplace(aie2::cm_timer_high);
    core_addresses.emplace(aie2::cm_edge_detection_event_control);
    core_addresses.emplace(aie2::cm_stream_switch_event_port_selection_0);
    core_addresses.emplace(aie2::cm_stream_switch_event_port_selection_1);

    // Memory modules
    memory_addresses.emplace(aie2::mm_trace_control0);
    memory_addresses.emplace(aie2::mm_trace_control1);
    memory_addresses.emplace(aie2::mm_trace_status);
    memory_addresses.emplace(aie2::mm_trace_event0);
    memory_addresses.emplace(aie2::mm_trace_event1);
    memory_addresses.emplace(aie2::mm_event_status0);
    memory_addresses.emplace(aie2::mm_event_status1);
    memory_addresses.emplace(aie2::mm_event_status2);
    memory_addresses.emplace(aie2::mm_event_status3);
    memory_addresses.emplace(aie2::mm_event_broadcast0);
    memory_addresses.emplace(aie2::mm_event_broadcast1);
    memory_addresses.emplace(aie2::mm_event_broadcast2);
    memory_addresses.emplace(aie2::mm_event_broadcast3);
    memory_addresses.emplace(aie2::mm_event_broadcast4);
    memory_addresses.emplace(aie2::mm_event_broadcast5);
    memory_addresses.emplace(aie2::mm_event_broadcast6);
    memory_addresses.emplace(aie2::mm_event_broadcast7);
    memory_addresses.emplace(aie2::mm_event_broadcast8);
    memory_addresses.emplace(aie2::mm_event_broadcast9);
    memory_addresses.emplace(aie2::mm_event_broadcast10);
    memory_addresses.emplace(aie2::mm_event_broadcast11);
    memory_addresses.emplace(aie2::mm_event_broadcast12);
    memory_addresses.emplace(aie2::mm_event_broadcast13);
    memory_addresses.emplace(aie2::mm_event_broadcast14);
    memory_addresses.emplace(aie2::mm_event_broadcast15);

    // Interface tiles
    interface_addresses.emplace(aie2::shim_trace_control0);
    interface_addresses.emplace(aie2::shim_trace_control1);
    interface_addresses.emplace(aie2::shim_trace_status);
    interface_addresses.emplace(aie2::shim_trace_event0);
    interface_addresses.emplace(aie2::shim_trace_event1);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_0);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_1);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_2);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_3);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_4);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_5);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_6);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_7);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_8);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_9);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_10);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_11);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_12);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_13);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_14);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_15);
    interface_addresses.emplace(aie2::shim_event_status0);
    interface_addresses.emplace(aie2::shim_event_status1);
    interface_addresses.emplace(aie2::shim_event_status2);
    interface_addresses.emplace(aie2::shim_event_status3);
    interface_addresses.emplace(aie2::shim_stream_switch_event_port_selection_0);
    interface_addresses.emplace(aie2::shim_stream_switch_event_port_selection_1);

    // Memory tiles
    memory_tile_addresses.emplace(aie2::mem_trace_control0);
    memory_tile_addresses.emplace(aie2::mem_trace_control1);
    memory_tile_addresses.emplace(aie2::mem_trace_status);
    memory_tile_addresses.emplace(aie2::mem_trace_event0);
    memory_tile_addresses.emplace(aie2::mem_trace_event1);
    memory_tile_addresses.emplace(aie2::mem_dma_event_channel_selection);
    memory_tile_addresses.emplace(aie2::mem_edge_detection_event_control);
    memory_tile_addresses.emplace(aie2::mem_stream_switch_event_port_selection_0);
    memory_tile_addresses.emplace(aie2::mem_stream_switch_event_port_selection_1);
    memory_tile_addresses.emplace(aie2::mem_event_broadcast0);
    memory_tile_addresses.emplace(aie2::mem_event_status0);
    memory_tile_addresses.emplace(aie2::mem_event_status1);
    memory_tile_addresses.emplace(aie2::mem_event_status2);
    memory_tile_addresses.emplace(aie2::mem_event_status3);
    memory_tile_addresses.emplace(aie2::mem_event_status4);
    memory_tile_addresses.emplace(aie2::mem_event_status5);
  }

void AIE2UsedRegisters::populateRegNameToValueMap() {
      regNameToValue["cm_core_amll0_part1"] = aie2::cm_core_amll0_part1;
   regNameToValue["cm_core_amll0_part2"] = aie2::cm_core_amll0_part2;
   regNameToValue["cm_core_amlh0_part1"] = aie2::cm_core_amlh0_part1;
   regNameToValue["cm_core_amlh0_part2"] = aie2::cm_core_amlh0_part2;
   regNameToValue["cm_core_amhl0_part1"] = aie2::cm_core_amhl0_part1;
   regNameToValue["cm_core_amhl0_part2"] = aie2::cm_core_amhl0_part2;
   regNameToValue["cm_core_amhh0_part1"] = aie2::cm_core_amhh0_part1;
   regNameToValue["cm_core_amhh0_part2"] = aie2::cm_core_amhh0_part2;
   regNameToValue["cm_core_amll1_part1"] = aie2::cm_core_amll1_part1;
   regNameToValue["cm_core_amll1_part2"] = aie2::cm_core_amll1_part2;
   regNameToValue["cm_core_amlh1_part1"] = aie2::cm_core_amlh1_part1;
   regNameToValue["cm_core_amlh1_part2"] = aie2::cm_core_amlh1_part2;
   regNameToValue["cm_core_amhl1_part1"] = aie2::cm_core_amhl1_part1;
   regNameToValue["cm_core_amhl1_part2"] = aie2::cm_core_amhl1_part2;
   regNameToValue["cm_core_amhh1_part1"] = aie2::cm_core_amhh1_part1;
   regNameToValue["cm_core_amhh1_part2"] = aie2::cm_core_amhh1_part2;
   regNameToValue["cm_core_amll2_part1"] = aie2::cm_core_amll2_part1;
   regNameToValue["cm_core_amll2_part2"] = aie2::cm_core_amll2_part2;
   regNameToValue["cm_core_amlh2_part1"] = aie2::cm_core_amlh2_part1;
   regNameToValue["cm_core_amlh2_part2"] = aie2::cm_core_amlh2_part2;
   regNameToValue["cm_core_amhl2_part1"] = aie2::cm_core_amhl2_part1;
   regNameToValue["cm_core_amhl2_part2"] = aie2::cm_core_amhl2_part2;
   regNameToValue["cm_core_amhh2_part1"] = aie2::cm_core_amhh2_part1;
   regNameToValue["cm_core_amhh2_part2"] = aie2::cm_core_amhh2_part2;
   regNameToValue["cm_core_amll3_part1"] = aie2::cm_core_amll3_part1;
   regNameToValue["cm_core_amll3_part2"] = aie2::cm_core_amll3_part2;
   regNameToValue["cm_core_amlh3_part1"] = aie2::cm_core_amlh3_part1;
   regNameToValue["cm_core_amlh3_part2"] = aie2::cm_core_amlh3_part2;
   regNameToValue["cm_core_amhl3_part1"] = aie2::cm_core_amhl3_part1;
   regNameToValue["cm_core_amhl3_part2"] = aie2::cm_core_amhl3_part2;
   regNameToValue["cm_core_amhh3_part1"] = aie2::cm_core_amhh3_part1;
   regNameToValue["cm_core_amhh3_part2"] = aie2::cm_core_amhh3_part2;
   regNameToValue["cm_core_amll4_part1"] = aie2::cm_core_amll4_part1;
   regNameToValue["cm_core_amll4_part2"] = aie2::cm_core_amll4_part2;
   regNameToValue["cm_core_amlh4_part1"] = aie2::cm_core_amlh4_part1;
   regNameToValue["cm_core_amlh4_part2"] = aie2::cm_core_amlh4_part2;
   regNameToValue["cm_core_amhl4_part1"] = aie2::cm_core_amhl4_part1;
   regNameToValue["cm_core_amhl4_part2"] = aie2::cm_core_amhl4_part2;
   regNameToValue["cm_core_amhh4_part1"] = aie2::cm_core_amhh4_part1;
   regNameToValue["cm_core_amhh4_part2"] = aie2::cm_core_amhh4_part2;
   regNameToValue["cm_core_amll5_part1"] = aie2::cm_core_amll5_part1;
   regNameToValue["cm_core_amll5_part2"] = aie2::cm_core_amll5_part2;
   regNameToValue["cm_core_amlh5_part1"] = aie2::cm_core_amlh5_part1;
   regNameToValue["cm_core_amlh5_part2"] = aie2::cm_core_amlh5_part2;
   regNameToValue["cm_core_amhl5_part1"] = aie2::cm_core_amhl5_part1;
   regNameToValue["cm_core_amhl5_part2"] = aie2::cm_core_amhl5_part2;
   regNameToValue["cm_core_amhh5_part1"] = aie2::cm_core_amhh5_part1;
   regNameToValue["cm_core_amhh5_part2"] = aie2::cm_core_amhh5_part2;
   regNameToValue["cm_core_amll6_part1"] = aie2::cm_core_amll6_part1;
   regNameToValue["cm_core_amll6_part2"] = aie2::cm_core_amll6_part2;
   regNameToValue["cm_core_amlh6_part1"] = aie2::cm_core_amlh6_part1;
   regNameToValue["cm_core_amlh6_part2"] = aie2::cm_core_amlh6_part2;
   regNameToValue["cm_core_amhl6_part1"] = aie2::cm_core_amhl6_part1;
   regNameToValue["cm_core_amhl6_part2"] = aie2::cm_core_amhl6_part2;
   regNameToValue["cm_core_amhh6_part1"] = aie2::cm_core_amhh6_part1;
   regNameToValue["cm_core_amhh6_part2"] = aie2::cm_core_amhh6_part2;
   regNameToValue["cm_core_amll7_part1"] = aie2::cm_core_amll7_part1;
   regNameToValue["cm_core_amll7_part2"] = aie2::cm_core_amll7_part2;
   regNameToValue["cm_core_amlh7_part1"] = aie2::cm_core_amlh7_part1;
   regNameToValue["cm_core_amlh7_part2"] = aie2::cm_core_amlh7_part2;
   regNameToValue["cm_core_amhl7_part1"] = aie2::cm_core_amhl7_part1;
   regNameToValue["cm_core_amhl7_part2"] = aie2::cm_core_amhl7_part2;
   regNameToValue["cm_core_amhh7_part1"] = aie2::cm_core_amhh7_part1;
   regNameToValue["cm_core_amhh7_part2"] = aie2::cm_core_amhh7_part2;
   regNameToValue["cm_core_amll8_part1"] = aie2::cm_core_amll8_part1;
   regNameToValue["cm_core_amll8_part2"] = aie2::cm_core_amll8_part2;
   regNameToValue["cm_core_amlh8_part1"] = aie2::cm_core_amlh8_part1;
   regNameToValue["cm_core_amlh8_part2"] = aie2::cm_core_amlh8_part2;
   regNameToValue["cm_core_amhl8_part1"] = aie2::cm_core_amhl8_part1;
   regNameToValue["cm_core_amhl8_part2"] = aie2::cm_core_amhl8_part2;
   regNameToValue["cm_core_amhh8_part1"] = aie2::cm_core_amhh8_part1;
   regNameToValue["cm_core_amhh8_part2"] = aie2::cm_core_amhh8_part2;
   regNameToValue["cm_reserved0"] = aie2::cm_reserved0;
   regNameToValue["cm_reserved1"] = aie2::cm_reserved1;
   regNameToValue["cm_reserved2"] = aie2::cm_reserved2;
   regNameToValue["cm_reserved3"] = aie2::cm_reserved3;
   regNameToValue["cm_reserved4"] = aie2::cm_reserved4;
   regNameToValue["cm_reserved5"] = aie2::cm_reserved5;
   regNameToValue["cm_reserved6"] = aie2::cm_reserved6;
   regNameToValue["cm_reserved7"] = aie2::cm_reserved7;
   regNameToValue["cm_reserved8"] = aie2::cm_reserved8;
   regNameToValue["cm_reserved9"] = aie2::cm_reserved9;
   regNameToValue["cm_reserved10"] = aie2::cm_reserved10;
   regNameToValue["cm_reserved11"] = aie2::cm_reserved11;
   regNameToValue["cm_reserved12"] = aie2::cm_reserved12;
   regNameToValue["cm_reserved13"] = aie2::cm_reserved13;
   regNameToValue["cm_reserved14"] = aie2::cm_reserved14;
   regNameToValue["cm_reserved15"] = aie2::cm_reserved15;
   regNameToValue["cm_reserved16"] = aie2::cm_reserved16;
   regNameToValue["cm_reserved17"] = aie2::cm_reserved17;
   regNameToValue["cm_reserved18"] = aie2::cm_reserved18;
   regNameToValue["cm_reserved19"] = aie2::cm_reserved19;
   regNameToValue["cm_reserved20"] = aie2::cm_reserved20;
   regNameToValue["cm_reserved21"] = aie2::cm_reserved21;
   regNameToValue["cm_reserved22"] = aie2::cm_reserved22;
   regNameToValue["cm_reserved23"] = aie2::cm_reserved23;
   regNameToValue["cm_reserved24"] = aie2::cm_reserved24;
   regNameToValue["cm_reserved25"] = aie2::cm_reserved25;
   regNameToValue["cm_reserved26"] = aie2::cm_reserved26;
   regNameToValue["cm_reserved27"] = aie2::cm_reserved27;
   regNameToValue["cm_reserved28"] = aie2::cm_reserved28;
   regNameToValue["cm_reserved29"] = aie2::cm_reserved29;
   regNameToValue["cm_reserved30"] = aie2::cm_reserved30;
   regNameToValue["cm_reserved31"] = aie2::cm_reserved31;
   regNameToValue["cm_reserved32"] = aie2::cm_reserved32;
   regNameToValue["cm_reserved33"] = aie2::cm_reserved33;
   regNameToValue["cm_reserved34"] = aie2::cm_reserved34;
   regNameToValue["cm_reserved35"] = aie2::cm_reserved35;
   regNameToValue["cm_reserved36"] = aie2::cm_reserved36;
   regNameToValue["cm_reserved37"] = aie2::cm_reserved37;
   regNameToValue["cm_reserved38"] = aie2::cm_reserved38;
   regNameToValue["cm_reserved39"] = aie2::cm_reserved39;
   regNameToValue["cm_reserved40"] = aie2::cm_reserved40;
   regNameToValue["cm_reserved41"] = aie2::cm_reserved41;
   regNameToValue["cm_reserved42"] = aie2::cm_reserved42;
   regNameToValue["cm_reserved43"] = aie2::cm_reserved43;
   regNameToValue["cm_reserved44"] = aie2::cm_reserved44;
   regNameToValue["cm_reserved45"] = aie2::cm_reserved45;
   regNameToValue["cm_reserved46"] = aie2::cm_reserved46;
   regNameToValue["cm_reserved47"] = aie2::cm_reserved47;
   regNameToValue["cm_reserved48"] = aie2::cm_reserved48;
   regNameToValue["cm_reserved49"] = aie2::cm_reserved49;
   regNameToValue["cm_reserved50"] = aie2::cm_reserved50;
   regNameToValue["cm_reserved51"] = aie2::cm_reserved51;
   regNameToValue["cm_reserved52"] = aie2::cm_reserved52;
   regNameToValue["cm_reserved53"] = aie2::cm_reserved53;
   regNameToValue["cm_reserved54"] = aie2::cm_reserved54;
   regNameToValue["cm_reserved55"] = aie2::cm_reserved55;
   regNameToValue["cm_core_wl0_part1"] = aie2::cm_core_wl0_part1;
   regNameToValue["cm_core_wl0_part2"] = aie2::cm_core_wl0_part2;
   regNameToValue["cm_core_wh0_part1"] = aie2::cm_core_wh0_part1;
   regNameToValue["cm_core_wh0_part2"] = aie2::cm_core_wh0_part2;
   regNameToValue["cm_core_wl1_part1"] = aie2::cm_core_wl1_part1;
   regNameToValue["cm_core_wl1_part2"] = aie2::cm_core_wl1_part2;
   regNameToValue["cm_core_wh1_part1"] = aie2::cm_core_wh1_part1;
   regNameToValue["cm_core_wh1_part2"] = aie2::cm_core_wh1_part2;
   regNameToValue["cm_core_wl2_part1"] = aie2::cm_core_wl2_part1;
   regNameToValue["cm_core_wl2_part2"] = aie2::cm_core_wl2_part2;
   regNameToValue["cm_core_wh2_part1"] = aie2::cm_core_wh2_part1;
   regNameToValue["cm_core_wh2_part2"] = aie2::cm_core_wh2_part2;
   regNameToValue["cm_core_wl3_part1"] = aie2::cm_core_wl3_part1;
   regNameToValue["cm_core_wl3_part2"] = aie2::cm_core_wl3_part2;
   regNameToValue["cm_core_wh3_part1"] = aie2::cm_core_wh3_part1;
   regNameToValue["cm_core_wh3_part2"] = aie2::cm_core_wh3_part2;
   regNameToValue["cm_core_wl4_part1"] = aie2::cm_core_wl4_part1;
   regNameToValue["cm_core_wl4_part2"] = aie2::cm_core_wl4_part2;
   regNameToValue["cm_core_wh4_part1"] = aie2::cm_core_wh4_part1;
   regNameToValue["cm_core_wh4_part2"] = aie2::cm_core_wh4_part2;
   regNameToValue["cm_core_wl5_part1"] = aie2::cm_core_wl5_part1;
   regNameToValue["cm_core_wl5_part2"] = aie2::cm_core_wl5_part2;
   regNameToValue["cm_core_wh5_part1"] = aie2::cm_core_wh5_part1;
   regNameToValue["cm_core_wh5_part2"] = aie2::cm_core_wh5_part2;
   regNameToValue["cm_core_wl6_part1"] = aie2::cm_core_wl6_part1;
   regNameToValue["cm_core_wl6_part2"] = aie2::cm_core_wl6_part2;
   regNameToValue["cm_core_wh6_part1"] = aie2::cm_core_wh6_part1;
   regNameToValue["cm_core_wh6_part2"] = aie2::cm_core_wh6_part2;
   regNameToValue["cm_core_wl7_part1"] = aie2::cm_core_wl7_part1;
   regNameToValue["cm_core_wl7_part2"] = aie2::cm_core_wl7_part2;
   regNameToValue["cm_core_wh7_part1"] = aie2::cm_core_wh7_part1;
   regNameToValue["cm_core_wh7_part2"] = aie2::cm_core_wh7_part2;
   regNameToValue["cm_core_wl8_part1"] = aie2::cm_core_wl8_part1;
   regNameToValue["cm_core_wl8_part2"] = aie2::cm_core_wl8_part2;
   regNameToValue["cm_core_wh8_part1"] = aie2::cm_core_wh8_part1;
   regNameToValue["cm_core_wh8_part2"] = aie2::cm_core_wh8_part2;
   regNameToValue["cm_core_wl9_part1"] = aie2::cm_core_wl9_part1;
   regNameToValue["cm_core_wl9_part2"] = aie2::cm_core_wl9_part2;
   regNameToValue["cm_core_wh9_part1"] = aie2::cm_core_wh9_part1;
   regNameToValue["cm_core_wh9_part2"] = aie2::cm_core_wh9_part2;
   regNameToValue["cm_core_wl10_part1"] = aie2::cm_core_wl10_part1;
   regNameToValue["cm_core_wl10_part2"] = aie2::cm_core_wl10_part2;
   regNameToValue["cm_core_wh10_part1"] = aie2::cm_core_wh10_part1;
   regNameToValue["cm_core_wh10_part2"] = aie2::cm_core_wh10_part2;
   regNameToValue["cm_core_wl11_part1"] = aie2::cm_core_wl11_part1;
   regNameToValue["cm_core_wl11_part2"] = aie2::cm_core_wl11_part2;
   regNameToValue["cm_core_wh11_part1"] = aie2::cm_core_wh11_part1;
   regNameToValue["cm_core_wh11_part2"] = aie2::cm_core_wh11_part2;
   regNameToValue["cm_reserved56"] = aie2::cm_reserved56;
   regNameToValue["cm_reserved57"] = aie2::cm_reserved57;
   regNameToValue["cm_reserved58"] = aie2::cm_reserved58;
   regNameToValue["cm_reserved59"] = aie2::cm_reserved59;
   regNameToValue["cm_reserved60"] = aie2::cm_reserved60;
   regNameToValue["cm_reserved61"] = aie2::cm_reserved61;
   regNameToValue["cm_reserved62"] = aie2::cm_reserved62;
   regNameToValue["cm_reserved63"] = aie2::cm_reserved63;
   regNameToValue["cm_reserved64"] = aie2::cm_reserved64;
   regNameToValue["cm_reserved65"] = aie2::cm_reserved65;
   regNameToValue["cm_reserved66"] = aie2::cm_reserved66;
   regNameToValue["cm_reserved67"] = aie2::cm_reserved67;
   regNameToValue["cm_reserved68"] = aie2::cm_reserved68;
   regNameToValue["cm_reserved69"] = aie2::cm_reserved69;
   regNameToValue["cm_reserved70"] = aie2::cm_reserved70;
   regNameToValue["cm_reserved71"] = aie2::cm_reserved71;
   regNameToValue["cm_core_r0"] = aie2::cm_core_r0;
   regNameToValue["cm_core_r1"] = aie2::cm_core_r1;
   regNameToValue["cm_core_r2"] = aie2::cm_core_r2;
   regNameToValue["cm_core_r3"] = aie2::cm_core_r3;
   regNameToValue["cm_core_r4"] = aie2::cm_core_r4;
   regNameToValue["cm_core_r5"] = aie2::cm_core_r5;
   regNameToValue["cm_core_r6"] = aie2::cm_core_r6;
   regNameToValue["cm_core_r7"] = aie2::cm_core_r7;
   regNameToValue["cm_core_r8"] = aie2::cm_core_r8;
   regNameToValue["cm_core_r9"] = aie2::cm_core_r9;
   regNameToValue["cm_core_r10"] = aie2::cm_core_r10;
   regNameToValue["cm_core_r11"] = aie2::cm_core_r11;
   regNameToValue["cm_core_r12"] = aie2::cm_core_r12;
   regNameToValue["cm_core_r13"] = aie2::cm_core_r13;
   regNameToValue["cm_core_r14"] = aie2::cm_core_r14;
   regNameToValue["cm_core_r15"] = aie2::cm_core_r15;
   regNameToValue["cm_core_r16"] = aie2::cm_core_r16;
   regNameToValue["cm_core_r17"] = aie2::cm_core_r17;
   regNameToValue["cm_core_r18"] = aie2::cm_core_r18;
   regNameToValue["cm_core_r19"] = aie2::cm_core_r19;
   regNameToValue["cm_core_r20"] = aie2::cm_core_r20;
   regNameToValue["cm_core_r21"] = aie2::cm_core_r21;
   regNameToValue["cm_core_r22"] = aie2::cm_core_r22;
   regNameToValue["cm_core_r23"] = aie2::cm_core_r23;
   regNameToValue["cm_core_r24"] = aie2::cm_core_r24;
   regNameToValue["cm_core_r25"] = aie2::cm_core_r25;
   regNameToValue["cm_core_r26"] = aie2::cm_core_r26;
   regNameToValue["cm_core_r27"] = aie2::cm_core_r27;
   regNameToValue["cm_core_r28"] = aie2::cm_core_r28;
   regNameToValue["cm_core_r29"] = aie2::cm_core_r29;
   regNameToValue["cm_core_r30"] = aie2::cm_core_r30;
   regNameToValue["cm_core_r31"] = aie2::cm_core_r31;
   regNameToValue["cm_core_m0"] = aie2::cm_core_m0;
   regNameToValue["cm_core_m1"] = aie2::cm_core_m1;
   regNameToValue["cm_core_m2"] = aie2::cm_core_m2;
   regNameToValue["cm_core_m3"] = aie2::cm_core_m3;
   regNameToValue["cm_core_m4"] = aie2::cm_core_m4;
   regNameToValue["cm_core_m5"] = aie2::cm_core_m5;
   regNameToValue["cm_core_m6"] = aie2::cm_core_m6;
   regNameToValue["cm_core_m7"] = aie2::cm_core_m7;
   regNameToValue["cm_core_dn0"] = aie2::cm_core_dn0;
   regNameToValue["cm_core_dn1"] = aie2::cm_core_dn1;
   regNameToValue["cm_core_dn2"] = aie2::cm_core_dn2;
   regNameToValue["cm_core_dn3"] = aie2::cm_core_dn3;
   regNameToValue["cm_core_dn4"] = aie2::cm_core_dn4;
   regNameToValue["cm_core_dn5"] = aie2::cm_core_dn5;
   regNameToValue["cm_core_dn6"] = aie2::cm_core_dn6;
   regNameToValue["cm_core_dn7"] = aie2::cm_core_dn7;
   regNameToValue["cm_core_dj0"] = aie2::cm_core_dj0;
   regNameToValue["cm_core_dj1"] = aie2::cm_core_dj1;
   regNameToValue["cm_core_dj2"] = aie2::cm_core_dj2;
   regNameToValue["cm_core_dj3"] = aie2::cm_core_dj3;
   regNameToValue["cm_core_dj4"] = aie2::cm_core_dj4;
   regNameToValue["cm_core_dj5"] = aie2::cm_core_dj5;
   regNameToValue["cm_core_dj6"] = aie2::cm_core_dj6;
   regNameToValue["cm_core_dj7"] = aie2::cm_core_dj7;
   regNameToValue["cm_core_dc0"] = aie2::cm_core_dc0;
   regNameToValue["cm_core_dc1"] = aie2::cm_core_dc1;
   regNameToValue["cm_core_dc2"] = aie2::cm_core_dc2;
   regNameToValue["cm_core_dc3"] = aie2::cm_core_dc3;
   regNameToValue["cm_core_dc4"] = aie2::cm_core_dc4;
   regNameToValue["cm_core_dc5"] = aie2::cm_core_dc5;
   regNameToValue["cm_core_dc6"] = aie2::cm_core_dc6;
   regNameToValue["cm_core_dc7"] = aie2::cm_core_dc7;
   regNameToValue["cm_core_p0"] = aie2::cm_core_p0;
   regNameToValue["cm_core_p1"] = aie2::cm_core_p1;
   regNameToValue["cm_core_p2"] = aie2::cm_core_p2;
   regNameToValue["cm_core_p3"] = aie2::cm_core_p3;
   regNameToValue["cm_core_p4"] = aie2::cm_core_p4;
   regNameToValue["cm_core_p5"] = aie2::cm_core_p5;
   regNameToValue["cm_core_p6"] = aie2::cm_core_p6;
   regNameToValue["cm_core_p7"] = aie2::cm_core_p7;
   regNameToValue["cm_core_s0"] = aie2::cm_core_s0;
   regNameToValue["cm_core_s1"] = aie2::cm_core_s1;
   regNameToValue["cm_core_s2"] = aie2::cm_core_s2;
   regNameToValue["cm_core_s3"] = aie2::cm_core_s3;
   regNameToValue["cm_core_q0"] = aie2::cm_core_q0;
   regNameToValue["cm_core_q1"] = aie2::cm_core_q1;
   regNameToValue["cm_core_q2"] = aie2::cm_core_q2;
   regNameToValue["cm_core_q3"] = aie2::cm_core_q3;
   regNameToValue["cm_program_counter"] = aie2::cm_program_counter;
   regNameToValue["cm_core_fc"] = aie2::cm_core_fc;
   regNameToValue["cm_core_sp"] = aie2::cm_core_sp;
   regNameToValue["cm_core_lr"] = aie2::cm_core_lr;
   regNameToValue["cm_core_ls"] = aie2::cm_core_ls;
   regNameToValue["cm_core_le"] = aie2::cm_core_le;
   regNameToValue["cm_core_lc"] = aie2::cm_core_lc;
   regNameToValue["cm_core_cr"] = aie2::cm_core_cr;
   regNameToValue["cm_core_sr"] = aie2::cm_core_sr;
   regNameToValue["cm_core_dp"] = aie2::cm_core_dp;
   regNameToValue["cm_performance_control0"] = aie2::cm_performance_control0;
   regNameToValue["cm_performance_control1"] = aie2::cm_performance_control1;
   regNameToValue["cm_performance_control2"] = aie2::cm_performance_control2;
   regNameToValue["cm_performance_counter0"] = aie2::cm_performance_counter0;
   regNameToValue["cm_performance_counter1"] = aie2::cm_performance_counter1;
   regNameToValue["cm_performance_counter2"] = aie2::cm_performance_counter2;
   regNameToValue["cm_performance_counter3"] = aie2::cm_performance_counter3;
   regNameToValue["cm_performance_counter0_event_value"] = aie2::cm_performance_counter0_event_value;
   regNameToValue["cm_performance_counter1_event_value"] = aie2::cm_performance_counter1_event_value;
   regNameToValue["cm_performance_counter2_event_value"] = aie2::cm_performance_counter2_event_value;
   regNameToValue["cm_performance_counter3_event_value"] = aie2::cm_performance_counter3_event_value;
   regNameToValue["cm_core_control"] = aie2::cm_core_control;
   regNameToValue["cm_core_status"] = aie2::cm_core_status;
   regNameToValue["cm_enable_events"] = aie2::cm_enable_events;
   regNameToValue["cm_reset_event"] = aie2::cm_reset_event;
   regNameToValue["cm_debug_control0"] = aie2::cm_debug_control0;
   regNameToValue["cm_debug_control1"] = aie2::cm_debug_control1;
   regNameToValue["cm_debug_control2"] = aie2::cm_debug_control2;
   regNameToValue["cm_debug_status"] = aie2::cm_debug_status;
   regNameToValue["cm_pc_event0"] = aie2::cm_pc_event0;
   regNameToValue["cm_pc_event1"] = aie2::cm_pc_event1;
   regNameToValue["cm_pc_event2"] = aie2::cm_pc_event2;
   regNameToValue["cm_pc_event3"] = aie2::cm_pc_event3;
   regNameToValue["cm_error_halt_control"] = aie2::cm_error_halt_control;
   regNameToValue["cm_error_halt_event"] = aie2::cm_error_halt_event;
   regNameToValue["cm_core_processor_bus"] = aie2::cm_core_processor_bus;
   regNameToValue["cm_ecc_control"] = aie2::cm_ecc_control;
   regNameToValue["cm_ecc_scrubbing_event"] = aie2::cm_ecc_scrubbing_event;
   regNameToValue["cm_ecc_failing_address"] = aie2::cm_ecc_failing_address;
   regNameToValue["cm_timer_control"] = aie2::cm_timer_control;
   regNameToValue["cm_event_generate"] = aie2::cm_event_generate;
   regNameToValue["cm_event_broadcast0"] = aie2::cm_event_broadcast0;
   regNameToValue["cm_event_broadcast1"] = aie2::cm_event_broadcast1;
   regNameToValue["cm_event_broadcast2"] = aie2::cm_event_broadcast2;
   regNameToValue["cm_event_broadcast3"] = aie2::cm_event_broadcast3;
   regNameToValue["cm_event_broadcast4"] = aie2::cm_event_broadcast4;
   regNameToValue["cm_event_broadcast5"] = aie2::cm_event_broadcast5;
   regNameToValue["cm_event_broadcast6"] = aie2::cm_event_broadcast6;
   regNameToValue["cm_event_broadcast7"] = aie2::cm_event_broadcast7;
   regNameToValue["cm_event_broadcast8"] = aie2::cm_event_broadcast8;
   regNameToValue["cm_event_broadcast9"] = aie2::cm_event_broadcast9;
   regNameToValue["cm_event_broadcast10"] = aie2::cm_event_broadcast10;
   regNameToValue["cm_event_broadcast11"] = aie2::cm_event_broadcast11;
   regNameToValue["cm_event_broadcast12"] = aie2::cm_event_broadcast12;
   regNameToValue["cm_event_broadcast13"] = aie2::cm_event_broadcast13;
   regNameToValue["cm_event_broadcast14"] = aie2::cm_event_broadcast14;
   regNameToValue["cm_event_broadcast15"] = aie2::cm_event_broadcast15;
   regNameToValue["cm_event_broadcast_block_south_set"] = aie2::cm_event_broadcast_block_south_set;
   regNameToValue["cm_event_broadcast_block_south_clr"] = aie2::cm_event_broadcast_block_south_clr;
   regNameToValue["cm_event_broadcast_block_south_value"] = aie2::cm_event_broadcast_block_south_value;
   regNameToValue["cm_event_broadcast_block_west_set"] = aie2::cm_event_broadcast_block_west_set;
   regNameToValue["cm_event_broadcast_block_west_clr"] = aie2::cm_event_broadcast_block_west_clr;
   regNameToValue["cm_event_broadcast_block_west_value"] = aie2::cm_event_broadcast_block_west_value;
   regNameToValue["cm_event_broadcast_block_north_set"] = aie2::cm_event_broadcast_block_north_set;
   regNameToValue["cm_event_broadcast_block_north_clr"] = aie2::cm_event_broadcast_block_north_clr;
   regNameToValue["cm_event_broadcast_block_north_value"] = aie2::cm_event_broadcast_block_north_value;
   regNameToValue["cm_event_broadcast_block_east_set"] = aie2::cm_event_broadcast_block_east_set;
   regNameToValue["cm_event_broadcast_block_east_clr"] = aie2::cm_event_broadcast_block_east_clr;
   regNameToValue["cm_event_broadcast_block_east_value"] = aie2::cm_event_broadcast_block_east_value;
   regNameToValue["cm_trace_control0"] = aie2::cm_trace_control0;
   regNameToValue["cm_trace_control1"] = aie2::cm_trace_control1;
   regNameToValue["cm_trace_status"] = aie2::cm_trace_status;
   regNameToValue["cm_trace_event0"] = aie2::cm_trace_event0;
   regNameToValue["cm_trace_event1"] = aie2::cm_trace_event1;
   regNameToValue["cm_timer_trig_event_low_value"] = aie2::cm_timer_trig_event_low_value;
   regNameToValue["cm_timer_trig_event_high_value"] = aie2::cm_timer_trig_event_high_value;
   regNameToValue["cm_timer_low"] = aie2::cm_timer_low;
   regNameToValue["cm_timer_high"] = aie2::cm_timer_high;
   regNameToValue["cm_event_status0"] = aie2::cm_event_status0;
   regNameToValue["cm_event_status1"] = aie2::cm_event_status1;
   regNameToValue["cm_event_status2"] = aie2::cm_event_status2;
   regNameToValue["cm_event_status3"] = aie2::cm_event_status3;
   regNameToValue["cm_combo_event_inputs"] = aie2::cm_combo_event_inputs;
   regNameToValue["cm_combo_event_control"] = aie2::cm_combo_event_control;
   regNameToValue["cm_edge_detection_event_control"] = aie2::cm_edge_detection_event_control;
   regNameToValue["cm_event_group_0_enable"] = aie2::cm_event_group_0_enable;
   regNameToValue["cm_event_group_pc_enable"] = aie2::cm_event_group_pc_enable;
   regNameToValue["cm_event_group_core_stall_enable"] = aie2::cm_event_group_core_stall_enable;
   regNameToValue["cm_event_group_core_program_flow_enable"] = aie2::cm_event_group_core_program_flow_enable;
   regNameToValue["cm_event_group_errors0_enable"] = aie2::cm_event_group_errors0_enable;
   regNameToValue["cm_event_group_errors1_enable"] = aie2::cm_event_group_errors1_enable;
   regNameToValue["cm_event_group_stream_switch_enable"] = aie2::cm_event_group_stream_switch_enable;
   regNameToValue["cm_event_group_broadcast_enable"] = aie2::cm_event_group_broadcast_enable;
   regNameToValue["cm_event_group_user_event_enable"] = aie2::cm_event_group_user_event_enable;
   regNameToValue["cm_tile_control"] = aie2::cm_tile_control;
   regNameToValue["cm_cssd_trigger"] = aie2::cm_cssd_trigger;
   regNameToValue["cm_spare_reg"] = aie2::cm_spare_reg;
   regNameToValue["cm_accumulator_control"] = aie2::cm_accumulator_control;
   regNameToValue["cm_memory_control"] = aie2::cm_memory_control;
   regNameToValue["cm_stream_switch_master_config_aie_core0"] = aie2::cm_stream_switch_master_config_aie_core0;
   regNameToValue["cm_stream_switch_master_config_dma0"] = aie2::cm_stream_switch_master_config_dma0;
   regNameToValue["cm_stream_switch_master_config_dma1"] = aie2::cm_stream_switch_master_config_dma1;
   regNameToValue["cm_stream_switch_master_config_tile_ctrl"] = aie2::cm_stream_switch_master_config_tile_ctrl;
   regNameToValue["cm_stream_switch_master_config_fifo0"] = aie2::cm_stream_switch_master_config_fifo0;
   regNameToValue["cm_stream_switch_master_config_south0"] = aie2::cm_stream_switch_master_config_south0;
   regNameToValue["cm_stream_switch_master_config_south1"] = aie2::cm_stream_switch_master_config_south1;
   regNameToValue["cm_stream_switch_master_config_south2"] = aie2::cm_stream_switch_master_config_south2;
   regNameToValue["cm_stream_switch_master_config_south3"] = aie2::cm_stream_switch_master_config_south3;
   regNameToValue["cm_stream_switch_master_config_west0"] = aie2::cm_stream_switch_master_config_west0;
   regNameToValue["cm_stream_switch_master_config_west1"] = aie2::cm_stream_switch_master_config_west1;
   regNameToValue["cm_stream_switch_master_config_west2"] = aie2::cm_stream_switch_master_config_west2;
   regNameToValue["cm_stream_switch_master_config_west3"] = aie2::cm_stream_switch_master_config_west3;
   regNameToValue["cm_stream_switch_master_config_north0"] = aie2::cm_stream_switch_master_config_north0;
   regNameToValue["cm_stream_switch_master_config_north1"] = aie2::cm_stream_switch_master_config_north1;
   regNameToValue["cm_stream_switch_master_config_north2"] = aie2::cm_stream_switch_master_config_north2;
   regNameToValue["cm_stream_switch_master_config_north3"] = aie2::cm_stream_switch_master_config_north3;
   regNameToValue["cm_stream_switch_master_config_north4"] = aie2::cm_stream_switch_master_config_north4;
   regNameToValue["cm_stream_switch_master_config_north5"] = aie2::cm_stream_switch_master_config_north5;
   regNameToValue["cm_stream_switch_master_config_east0"] = aie2::cm_stream_switch_master_config_east0;
   regNameToValue["cm_stream_switch_master_config_east1"] = aie2::cm_stream_switch_master_config_east1;
   regNameToValue["cm_stream_switch_master_config_east2"] = aie2::cm_stream_switch_master_config_east2;
   regNameToValue["cm_stream_switch_master_config_east3"] = aie2::cm_stream_switch_master_config_east3;
   regNameToValue["cm_stream_switch_slave_config_aie_core0"] = aie2::cm_stream_switch_slave_config_aie_core0;
   regNameToValue["cm_stream_switch_slave_config_dma_0"] = aie2::cm_stream_switch_slave_config_dma_0;
   regNameToValue["cm_stream_switch_slave_config_dma_1"] = aie2::cm_stream_switch_slave_config_dma_1;
   regNameToValue["cm_stream_switch_slave_config_tile_ctrl"] = aie2::cm_stream_switch_slave_config_tile_ctrl;
   regNameToValue["cm_stream_switch_slave_config_fifo_0"] = aie2::cm_stream_switch_slave_config_fifo_0;
   regNameToValue["cm_stream_switch_slave_config_south_0"] = aie2::cm_stream_switch_slave_config_south_0;
   regNameToValue["cm_stream_switch_slave_config_south_1"] = aie2::cm_stream_switch_slave_config_south_1;
   regNameToValue["cm_stream_switch_slave_config_south_2"] = aie2::cm_stream_switch_slave_config_south_2;
   regNameToValue["cm_stream_switch_slave_config_south_3"] = aie2::cm_stream_switch_slave_config_south_3;
   regNameToValue["cm_stream_switch_slave_config_south_4"] = aie2::cm_stream_switch_slave_config_south_4;
   regNameToValue["cm_stream_switch_slave_config_south_5"] = aie2::cm_stream_switch_slave_config_south_5;
   regNameToValue["cm_stream_switch_slave_config_west_0"] = aie2::cm_stream_switch_slave_config_west_0;
   regNameToValue["cm_stream_switch_slave_config_west_1"] = aie2::cm_stream_switch_slave_config_west_1;
   regNameToValue["cm_stream_switch_slave_config_west_2"] = aie2::cm_stream_switch_slave_config_west_2;
   regNameToValue["cm_stream_switch_slave_config_west_3"] = aie2::cm_stream_switch_slave_config_west_3;
   regNameToValue["cm_stream_switch_slave_config_north_0"] = aie2::cm_stream_switch_slave_config_north_0;
   regNameToValue["cm_stream_switch_slave_config_north_1"] = aie2::cm_stream_switch_slave_config_north_1;
   regNameToValue["cm_stream_switch_slave_config_north_2"] = aie2::cm_stream_switch_slave_config_north_2;
   regNameToValue["cm_stream_switch_slave_config_north_3"] = aie2::cm_stream_switch_slave_config_north_3;
   regNameToValue["cm_stream_switch_slave_config_east_0"] = aie2::cm_stream_switch_slave_config_east_0;
   regNameToValue["cm_stream_switch_slave_config_east_1"] = aie2::cm_stream_switch_slave_config_east_1;
   regNameToValue["cm_stream_switch_slave_config_east_2"] = aie2::cm_stream_switch_slave_config_east_2;
   regNameToValue["cm_stream_switch_slave_config_east_3"] = aie2::cm_stream_switch_slave_config_east_3;
   regNameToValue["cm_stream_switch_slave_config_aie_trace"] = aie2::cm_stream_switch_slave_config_aie_trace;
   regNameToValue["cm_stream_switch_slave_config_mem_trace"] = aie2::cm_stream_switch_slave_config_mem_trace;
   regNameToValue["cm_stream_switch_slave_aie_core0_slot0"] = aie2::cm_stream_switch_slave_aie_core0_slot0;
   regNameToValue["cm_stream_switch_slave_aie_core0_slot1"] = aie2::cm_stream_switch_slave_aie_core0_slot1;
   regNameToValue["cm_stream_switch_slave_aie_core0_slot2"] = aie2::cm_stream_switch_slave_aie_core0_slot2;
   regNameToValue["cm_stream_switch_slave_aie_core0_slot3"] = aie2::cm_stream_switch_slave_aie_core0_slot3;
   regNameToValue["cm_stream_switch_slave_dma_0_slot0"] = aie2::cm_stream_switch_slave_dma_0_slot0;
   regNameToValue["cm_stream_switch_slave_dma_0_slot1"] = aie2::cm_stream_switch_slave_dma_0_slot1;
   regNameToValue["cm_stream_switch_slave_dma_0_slot2"] = aie2::cm_stream_switch_slave_dma_0_slot2;
   regNameToValue["cm_stream_switch_slave_dma_0_slot3"] = aie2::cm_stream_switch_slave_dma_0_slot3;
   regNameToValue["cm_stream_switch_slave_dma_1_slot0"] = aie2::cm_stream_switch_slave_dma_1_slot0;
   regNameToValue["cm_stream_switch_slave_dma_1_slot1"] = aie2::cm_stream_switch_slave_dma_1_slot1;
   regNameToValue["cm_stream_switch_slave_dma_1_slot2"] = aie2::cm_stream_switch_slave_dma_1_slot2;
   regNameToValue["cm_stream_switch_slave_dma_1_slot3"] = aie2::cm_stream_switch_slave_dma_1_slot3;
   regNameToValue["cm_stream_switch_slave_tile_ctrl_slot0"] = aie2::cm_stream_switch_slave_tile_ctrl_slot0;
   regNameToValue["cm_stream_switch_slave_tile_ctrl_slot1"] = aie2::cm_stream_switch_slave_tile_ctrl_slot1;
   regNameToValue["cm_stream_switch_slave_tile_ctrl_slot2"] = aie2::cm_stream_switch_slave_tile_ctrl_slot2;
   regNameToValue["cm_stream_switch_slave_tile_ctrl_slot3"] = aie2::cm_stream_switch_slave_tile_ctrl_slot3;
   regNameToValue["cm_stream_switch_slave_fifo_0_slot0"] = aie2::cm_stream_switch_slave_fifo_0_slot0;
   regNameToValue["cm_stream_switch_slave_fifo_0_slot1"] = aie2::cm_stream_switch_slave_fifo_0_slot1;
   regNameToValue["cm_stream_switch_slave_fifo_0_slot2"] = aie2::cm_stream_switch_slave_fifo_0_slot2;
   regNameToValue["cm_stream_switch_slave_fifo_0_slot3"] = aie2::cm_stream_switch_slave_fifo_0_slot3;
   regNameToValue["cm_stream_switch_slave_south_0_slot0"] = aie2::cm_stream_switch_slave_south_0_slot0;
   regNameToValue["cm_stream_switch_slave_south_0_slot1"] = aie2::cm_stream_switch_slave_south_0_slot1;
   regNameToValue["cm_stream_switch_slave_south_0_slot2"] = aie2::cm_stream_switch_slave_south_0_slot2;
   regNameToValue["cm_stream_switch_slave_south_0_slot3"] = aie2::cm_stream_switch_slave_south_0_slot3;
   regNameToValue["cm_stream_switch_slave_south_1_slot0"] = aie2::cm_stream_switch_slave_south_1_slot0;
   regNameToValue["cm_stream_switch_slave_south_1_slot1"] = aie2::cm_stream_switch_slave_south_1_slot1;
   regNameToValue["cm_stream_switch_slave_south_1_slot2"] = aie2::cm_stream_switch_slave_south_1_slot2;
   regNameToValue["cm_stream_switch_slave_south_1_slot3"] = aie2::cm_stream_switch_slave_south_1_slot3;
   regNameToValue["cm_stream_switch_slave_south_2_slot0"] = aie2::cm_stream_switch_slave_south_2_slot0;
   regNameToValue["cm_stream_switch_slave_south_2_slot1"] = aie2::cm_stream_switch_slave_south_2_slot1;
   regNameToValue["cm_stream_switch_slave_south_2_slot2"] = aie2::cm_stream_switch_slave_south_2_slot2;
   regNameToValue["cm_stream_switch_slave_south_2_slot3"] = aie2::cm_stream_switch_slave_south_2_slot3;
   regNameToValue["cm_stream_switch_slave_south_3_slot0"] = aie2::cm_stream_switch_slave_south_3_slot0;
   regNameToValue["cm_stream_switch_slave_south_3_slot1"] = aie2::cm_stream_switch_slave_south_3_slot1;
   regNameToValue["cm_stream_switch_slave_south_3_slot2"] = aie2::cm_stream_switch_slave_south_3_slot2;
   regNameToValue["cm_stream_switch_slave_south_3_slot3"] = aie2::cm_stream_switch_slave_south_3_slot3;
   regNameToValue["cm_stream_switch_slave_south_4_slot0"] = aie2::cm_stream_switch_slave_south_4_slot0;
   regNameToValue["cm_stream_switch_slave_south_4_slot1"] = aie2::cm_stream_switch_slave_south_4_slot1;
   regNameToValue["cm_stream_switch_slave_south_4_slot2"] = aie2::cm_stream_switch_slave_south_4_slot2;
   regNameToValue["cm_stream_switch_slave_south_4_slot3"] = aie2::cm_stream_switch_slave_south_4_slot3;
   regNameToValue["cm_stream_switch_slave_south_5_slot0"] = aie2::cm_stream_switch_slave_south_5_slot0;
   regNameToValue["cm_stream_switch_slave_south_5_slot1"] = aie2::cm_stream_switch_slave_south_5_slot1;
   regNameToValue["cm_stream_switch_slave_south_5_slot2"] = aie2::cm_stream_switch_slave_south_5_slot2;
   regNameToValue["cm_stream_switch_slave_south_5_slot3"] = aie2::cm_stream_switch_slave_south_5_slot3;
   regNameToValue["cm_stream_switch_slave_west_0_slot0"] = aie2::cm_stream_switch_slave_west_0_slot0;
   regNameToValue["cm_stream_switch_slave_west_0_slot1"] = aie2::cm_stream_switch_slave_west_0_slot1;
   regNameToValue["cm_stream_switch_slave_west_0_slot2"] = aie2::cm_stream_switch_slave_west_0_slot2;
   regNameToValue["cm_stream_switch_slave_west_0_slot3"] = aie2::cm_stream_switch_slave_west_0_slot3;
   regNameToValue["cm_stream_switch_slave_west_1_slot0"] = aie2::cm_stream_switch_slave_west_1_slot0;
   regNameToValue["cm_stream_switch_slave_west_1_slot1"] = aie2::cm_stream_switch_slave_west_1_slot1;
   regNameToValue["cm_stream_switch_slave_west_1_slot2"] = aie2::cm_stream_switch_slave_west_1_slot2;
   regNameToValue["cm_stream_switch_slave_west_1_slot3"] = aie2::cm_stream_switch_slave_west_1_slot3;
   regNameToValue["cm_stream_switch_slave_west_2_slot0"] = aie2::cm_stream_switch_slave_west_2_slot0;
   regNameToValue["cm_stream_switch_slave_west_2_slot1"] = aie2::cm_stream_switch_slave_west_2_slot1;
   regNameToValue["cm_stream_switch_slave_west_2_slot2"] = aie2::cm_stream_switch_slave_west_2_slot2;
   regNameToValue["cm_stream_switch_slave_west_2_slot3"] = aie2::cm_stream_switch_slave_west_2_slot3;
   regNameToValue["cm_stream_switch_slave_west_3_slot0"] = aie2::cm_stream_switch_slave_west_3_slot0;
   regNameToValue["cm_stream_switch_slave_west_3_slot1"] = aie2::cm_stream_switch_slave_west_3_slot1;
   regNameToValue["cm_stream_switch_slave_west_3_slot2"] = aie2::cm_stream_switch_slave_west_3_slot2;
   regNameToValue["cm_stream_switch_slave_west_3_slot3"] = aie2::cm_stream_switch_slave_west_3_slot3;
   regNameToValue["cm_stream_switch_slave_north_0_slot0"] = aie2::cm_stream_switch_slave_north_0_slot0;
   regNameToValue["cm_stream_switch_slave_north_0_slot1"] = aie2::cm_stream_switch_slave_north_0_slot1;
   regNameToValue["cm_stream_switch_slave_north_0_slot2"] = aie2::cm_stream_switch_slave_north_0_slot2;
   regNameToValue["cm_stream_switch_slave_north_0_slot3"] = aie2::cm_stream_switch_slave_north_0_slot3;
   regNameToValue["cm_stream_switch_slave_north_1_slot0"] = aie2::cm_stream_switch_slave_north_1_slot0;
   regNameToValue["cm_stream_switch_slave_north_1_slot1"] = aie2::cm_stream_switch_slave_north_1_slot1;
   regNameToValue["cm_stream_switch_slave_north_1_slot2"] = aie2::cm_stream_switch_slave_north_1_slot2;
   regNameToValue["cm_stream_switch_slave_north_1_slot3"] = aie2::cm_stream_switch_slave_north_1_slot3;
   regNameToValue["cm_stream_switch_slave_north_2_slot0"] = aie2::cm_stream_switch_slave_north_2_slot0;
   regNameToValue["cm_stream_switch_slave_north_2_slot1"] = aie2::cm_stream_switch_slave_north_2_slot1;
   regNameToValue["cm_stream_switch_slave_north_2_slot2"] = aie2::cm_stream_switch_slave_north_2_slot2;
   regNameToValue["cm_stream_switch_slave_north_2_slot3"] = aie2::cm_stream_switch_slave_north_2_slot3;
   regNameToValue["cm_stream_switch_slave_north_3_slot0"] = aie2::cm_stream_switch_slave_north_3_slot0;
   regNameToValue["cm_stream_switch_slave_north_3_slot1"] = aie2::cm_stream_switch_slave_north_3_slot1;
   regNameToValue["cm_stream_switch_slave_north_3_slot2"] = aie2::cm_stream_switch_slave_north_3_slot2;
   regNameToValue["cm_stream_switch_slave_north_3_slot3"] = aie2::cm_stream_switch_slave_north_3_slot3;
   regNameToValue["cm_stream_switch_slave_east_0_slot0"] = aie2::cm_stream_switch_slave_east_0_slot0;
   regNameToValue["cm_stream_switch_slave_east_0_slot1"] = aie2::cm_stream_switch_slave_east_0_slot1;
   regNameToValue["cm_stream_switch_slave_east_0_slot2"] = aie2::cm_stream_switch_slave_east_0_slot2;
   regNameToValue["cm_stream_switch_slave_east_0_slot3"] = aie2::cm_stream_switch_slave_east_0_slot3;
   regNameToValue["cm_stream_switch_slave_east_1_slot0"] = aie2::cm_stream_switch_slave_east_1_slot0;
   regNameToValue["cm_stream_switch_slave_east_1_slot1"] = aie2::cm_stream_switch_slave_east_1_slot1;
   regNameToValue["cm_stream_switch_slave_east_1_slot2"] = aie2::cm_stream_switch_slave_east_1_slot2;
   regNameToValue["cm_stream_switch_slave_east_1_slot3"] = aie2::cm_stream_switch_slave_east_1_slot3;
   regNameToValue["cm_stream_switch_slave_east_2_slot0"] = aie2::cm_stream_switch_slave_east_2_slot0;
   regNameToValue["cm_stream_switch_slave_east_2_slot1"] = aie2::cm_stream_switch_slave_east_2_slot1;
   regNameToValue["cm_stream_switch_slave_east_2_slot2"] = aie2::cm_stream_switch_slave_east_2_slot2;
   regNameToValue["cm_stream_switch_slave_east_2_slot3"] = aie2::cm_stream_switch_slave_east_2_slot3;
   regNameToValue["cm_stream_switch_slave_east_3_slot0"] = aie2::cm_stream_switch_slave_east_3_slot0;
   regNameToValue["cm_stream_switch_slave_east_3_slot1"] = aie2::cm_stream_switch_slave_east_3_slot1;
   regNameToValue["cm_stream_switch_slave_east_3_slot2"] = aie2::cm_stream_switch_slave_east_3_slot2;
   regNameToValue["cm_stream_switch_slave_east_3_slot3"] = aie2::cm_stream_switch_slave_east_3_slot3;
   regNameToValue["cm_stream_switch_slave_aie_trace_slot0"] = aie2::cm_stream_switch_slave_aie_trace_slot0;
   regNameToValue["cm_stream_switch_slave_aie_trace_slot1"] = aie2::cm_stream_switch_slave_aie_trace_slot1;
   regNameToValue["cm_stream_switch_slave_aie_trace_slot2"] = aie2::cm_stream_switch_slave_aie_trace_slot2;
   regNameToValue["cm_stream_switch_slave_aie_trace_slot3"] = aie2::cm_stream_switch_slave_aie_trace_slot3;
   regNameToValue["cm_stream_switch_slave_mem_trace_slot0"] = aie2::cm_stream_switch_slave_mem_trace_slot0;
   regNameToValue["cm_stream_switch_slave_mem_trace_slot1"] = aie2::cm_stream_switch_slave_mem_trace_slot1;
   regNameToValue["cm_stream_switch_slave_mem_trace_slot2"] = aie2::cm_stream_switch_slave_mem_trace_slot2;
   regNameToValue["cm_stream_switch_slave_mem_trace_slot3"] = aie2::cm_stream_switch_slave_mem_trace_slot3;
   regNameToValue["cm_stream_switch_deterministic_merge_arb0_slave0_1"] = aie2::cm_stream_switch_deterministic_merge_arb0_slave0_1;
   regNameToValue["cm_stream_switch_deterministic_merge_arb0_slave2_3"] = aie2::cm_stream_switch_deterministic_merge_arb0_slave2_3;
   regNameToValue["cm_stream_switch_deterministic_merge_arb0_ctrl"] = aie2::cm_stream_switch_deterministic_merge_arb0_ctrl;
   regNameToValue["cm_stream_switch_deterministic_merge_arb1_slave0_1"] = aie2::cm_stream_switch_deterministic_merge_arb1_slave0_1;
   regNameToValue["cm_stream_switch_deterministic_merge_arb1_slave2_3"] = aie2::cm_stream_switch_deterministic_merge_arb1_slave2_3;
   regNameToValue["cm_stream_switch_deterministic_merge_arb1_ctrl"] = aie2::cm_stream_switch_deterministic_merge_arb1_ctrl;
   regNameToValue["cm_stream_switch_event_port_selection_0"] = aie2::cm_stream_switch_event_port_selection_0;
   regNameToValue["cm_stream_switch_event_port_selection_1"] = aie2::cm_stream_switch_event_port_selection_1;
   regNameToValue["cm_stream_switch_parity_status"] = aie2::cm_stream_switch_parity_status;
   regNameToValue["cm_stream_switch_parity_injection"] = aie2::cm_stream_switch_parity_injection;
   regNameToValue["cm_tile_control_packet_handler_status"] = aie2::cm_tile_control_packet_handler_status;
   regNameToValue["cm_stream_switch_adaptive_clock_gate_status"] = aie2::cm_stream_switch_adaptive_clock_gate_status;
   regNameToValue["cm_stream_switch_adaptive_clock_gate_abort_period"] = aie2::cm_stream_switch_adaptive_clock_gate_abort_period;
   regNameToValue["cm_module_clock_control"] = aie2::cm_module_clock_control;
   regNameToValue["cm_module_reset_control"] = aie2::cm_module_reset_control;
   regNameToValue["mm_checkbit_error_generation"] = aie2::mm_checkbit_error_generation;
   regNameToValue["mm_combo_event_control"] = aie2::mm_combo_event_control;
   regNameToValue["mm_combo_event_inputs"] = aie2::mm_combo_event_inputs;
   regNameToValue["mm_dma_bd0_0"] = aie2::mm_dma_bd0_0;
   regNameToValue["mm_dma_bd0_1"] = aie2::mm_dma_bd0_1;
   regNameToValue["mm_dma_bd0_2"] = aie2::mm_dma_bd0_2;
   regNameToValue["mm_dma_bd0_3"] = aie2::mm_dma_bd0_3;
   regNameToValue["mm_dma_bd0_4"] = aie2::mm_dma_bd0_4;
   regNameToValue["mm_dma_bd0_5"] = aie2::mm_dma_bd0_5;
   regNameToValue["mm_dma_bd10_0"] = aie2::mm_dma_bd10_0;
   regNameToValue["mm_dma_bd10_1"] = aie2::mm_dma_bd10_1;
   regNameToValue["mm_dma_bd10_2"] = aie2::mm_dma_bd10_2;
   regNameToValue["mm_dma_bd10_3"] = aie2::mm_dma_bd10_3;
   regNameToValue["mm_dma_bd10_4"] = aie2::mm_dma_bd10_4;
   regNameToValue["mm_dma_bd10_5"] = aie2::mm_dma_bd10_5;
   regNameToValue["mm_dma_bd11_0"] = aie2::mm_dma_bd11_0;
   regNameToValue["mm_dma_bd11_1"] = aie2::mm_dma_bd11_1;
   regNameToValue["mm_dma_bd11_2"] = aie2::mm_dma_bd11_2;
   regNameToValue["mm_dma_bd11_3"] = aie2::mm_dma_bd11_3;
   regNameToValue["mm_dma_bd11_4"] = aie2::mm_dma_bd11_4;
   regNameToValue["mm_dma_bd11_5"] = aie2::mm_dma_bd11_5;
   regNameToValue["mm_dma_bd12_0"] = aie2::mm_dma_bd12_0;
   regNameToValue["mm_dma_bd12_1"] = aie2::mm_dma_bd12_1;
   regNameToValue["mm_dma_bd12_2"] = aie2::mm_dma_bd12_2;
   regNameToValue["mm_dma_bd12_3"] = aie2::mm_dma_bd12_3;
   regNameToValue["mm_dma_bd12_4"] = aie2::mm_dma_bd12_4;
   regNameToValue["mm_dma_bd12_5"] = aie2::mm_dma_bd12_5;
   regNameToValue["mm_dma_bd13_0"] = aie2::mm_dma_bd13_0;
   regNameToValue["mm_dma_bd13_1"] = aie2::mm_dma_bd13_1;
   regNameToValue["mm_dma_bd13_2"] = aie2::mm_dma_bd13_2;
   regNameToValue["mm_dma_bd13_3"] = aie2::mm_dma_bd13_3;
   regNameToValue["mm_dma_bd13_4"] = aie2::mm_dma_bd13_4;
   regNameToValue["mm_dma_bd13_5"] = aie2::mm_dma_bd13_5;
   regNameToValue["mm_dma_bd14_0"] = aie2::mm_dma_bd14_0;
   regNameToValue["mm_dma_bd14_1"] = aie2::mm_dma_bd14_1;
   regNameToValue["mm_dma_bd14_2"] = aie2::mm_dma_bd14_2;
   regNameToValue["mm_dma_bd14_3"] = aie2::mm_dma_bd14_3;
   regNameToValue["mm_dma_bd14_4"] = aie2::mm_dma_bd14_4;
   regNameToValue["mm_dma_bd14_5"] = aie2::mm_dma_bd14_5;
   regNameToValue["mm_dma_bd15_0"] = aie2::mm_dma_bd15_0;
   regNameToValue["mm_dma_bd15_1"] = aie2::mm_dma_bd15_1;
   regNameToValue["mm_dma_bd15_2"] = aie2::mm_dma_bd15_2;
   regNameToValue["mm_dma_bd15_3"] = aie2::mm_dma_bd15_3;
   regNameToValue["mm_dma_bd15_4"] = aie2::mm_dma_bd15_4;
   regNameToValue["mm_dma_bd15_5"] = aie2::mm_dma_bd15_5;
   regNameToValue["mm_dma_bd1_0"] = aie2::mm_dma_bd1_0;
   regNameToValue["mm_dma_bd1_1"] = aie2::mm_dma_bd1_1;
   regNameToValue["mm_dma_bd1_2"] = aie2::mm_dma_bd1_2;
   regNameToValue["mm_dma_bd1_3"] = aie2::mm_dma_bd1_3;
   regNameToValue["mm_dma_bd1_4"] = aie2::mm_dma_bd1_4;
   regNameToValue["mm_dma_bd1_5"] = aie2::mm_dma_bd1_5;
   regNameToValue["mm_dma_bd2_0"] = aie2::mm_dma_bd2_0;
   regNameToValue["mm_dma_bd2_1"] = aie2::mm_dma_bd2_1;
   regNameToValue["mm_dma_bd2_2"] = aie2::mm_dma_bd2_2;
   regNameToValue["mm_dma_bd2_3"] = aie2::mm_dma_bd2_3;
   regNameToValue["mm_dma_bd2_4"] = aie2::mm_dma_bd2_4;
   regNameToValue["mm_dma_bd2_5"] = aie2::mm_dma_bd2_5;
   regNameToValue["mm_dma_bd3_0"] = aie2::mm_dma_bd3_0;
   regNameToValue["mm_dma_bd3_1"] = aie2::mm_dma_bd3_1;
   regNameToValue["mm_dma_bd3_2"] = aie2::mm_dma_bd3_2;
   regNameToValue["mm_dma_bd3_3"] = aie2::mm_dma_bd3_3;
   regNameToValue["mm_dma_bd3_4"] = aie2::mm_dma_bd3_4;
   regNameToValue["mm_dma_bd3_5"] = aie2::mm_dma_bd3_5;
   regNameToValue["mm_dma_bd4_0"] = aie2::mm_dma_bd4_0;
   regNameToValue["mm_dma_bd4_1"] = aie2::mm_dma_bd4_1;
   regNameToValue["mm_dma_bd4_2"] = aie2::mm_dma_bd4_2;
   regNameToValue["mm_dma_bd4_3"] = aie2::mm_dma_bd4_3;
   regNameToValue["mm_dma_bd4_4"] = aie2::mm_dma_bd4_4;
   regNameToValue["mm_dma_bd4_5"] = aie2::mm_dma_bd4_5;
   regNameToValue["mm_dma_bd5_0"] = aie2::mm_dma_bd5_0;
   regNameToValue["mm_dma_bd5_1"] = aie2::mm_dma_bd5_1;
   regNameToValue["mm_dma_bd5_2"] = aie2::mm_dma_bd5_2;
   regNameToValue["mm_dma_bd5_3"] = aie2::mm_dma_bd5_3;
   regNameToValue["mm_dma_bd5_4"] = aie2::mm_dma_bd5_4;
   regNameToValue["mm_dma_bd5_5"] = aie2::mm_dma_bd5_5;
   regNameToValue["mm_dma_bd6_0"] = aie2::mm_dma_bd6_0;
   regNameToValue["mm_dma_bd6_1"] = aie2::mm_dma_bd6_1;
   regNameToValue["mm_dma_bd6_2"] = aie2::mm_dma_bd6_2;
   regNameToValue["mm_dma_bd6_3"] = aie2::mm_dma_bd6_3;
   regNameToValue["mm_dma_bd6_4"] = aie2::mm_dma_bd6_4;
   regNameToValue["mm_dma_bd6_5"] = aie2::mm_dma_bd6_5;
   regNameToValue["mm_dma_bd7_0"] = aie2::mm_dma_bd7_0;
   regNameToValue["mm_dma_bd7_1"] = aie2::mm_dma_bd7_1;
   regNameToValue["mm_dma_bd7_2"] = aie2::mm_dma_bd7_2;
   regNameToValue["mm_dma_bd7_3"] = aie2::mm_dma_bd7_3;
   regNameToValue["mm_dma_bd7_4"] = aie2::mm_dma_bd7_4;
   regNameToValue["mm_dma_bd7_5"] = aie2::mm_dma_bd7_5;
   regNameToValue["mm_dma_bd8_0"] = aie2::mm_dma_bd8_0;
   regNameToValue["mm_dma_bd8_1"] = aie2::mm_dma_bd8_1;
   regNameToValue["mm_dma_bd8_2"] = aie2::mm_dma_bd8_2;
   regNameToValue["mm_dma_bd8_3"] = aie2::mm_dma_bd8_3;
   regNameToValue["mm_dma_bd8_4"] = aie2::mm_dma_bd8_4;
   regNameToValue["mm_dma_bd8_5"] = aie2::mm_dma_bd8_5;
   regNameToValue["mm_dma_bd9_0"] = aie2::mm_dma_bd9_0;
   regNameToValue["mm_dma_bd9_1"] = aie2::mm_dma_bd9_1;
   regNameToValue["mm_dma_bd9_2"] = aie2::mm_dma_bd9_2;
   regNameToValue["mm_dma_bd9_3"] = aie2::mm_dma_bd9_3;
   regNameToValue["mm_dma_bd9_4"] = aie2::mm_dma_bd9_4;
   regNameToValue["mm_dma_bd9_5"] = aie2::mm_dma_bd9_5;
   regNameToValue["mm_dma_mm2s_0_ctrl"] = aie2::mm_dma_mm2s_0_ctrl;
   regNameToValue["mm_dma_mm2s_0_start_queue"] = aie2::mm_dma_mm2s_0_start_queue;
   regNameToValue["mm_dma_mm2s_1_ctrl"] = aie2::mm_dma_mm2s_1_ctrl;
   regNameToValue["mm_dma_mm2s_1_start_queue"] = aie2::mm_dma_mm2s_1_start_queue;
   regNameToValue["mm_dma_mm2s_status_0"] = aie2::mm_dma_mm2s_status_0;
   regNameToValue["mm_dma_mm2s_status_1"] = aie2::mm_dma_mm2s_status_1;
   regNameToValue["mm_dma_s2mm_0_ctrl"] = aie2::mm_dma_s2mm_0_ctrl;
   regNameToValue["mm_dma_s2mm_0_start_queue"] = aie2::mm_dma_s2mm_0_start_queue;
   regNameToValue["mm_dma_s2mm_1_ctrl"] = aie2::mm_dma_s2mm_1_ctrl;
   regNameToValue["mm_dma_s2mm_1_start_queue"] = aie2::mm_dma_s2mm_1_start_queue;
   regNameToValue["mm_dma_s2mm_current_write_count_0"] = aie2::mm_dma_s2mm_current_write_count_0;
   regNameToValue["mm_dma_s2mm_current_write_count_1"] = aie2::mm_dma_s2mm_current_write_count_1;
   regNameToValue["mm_dma_s2mm_fot_count_fifo_pop_0"] = aie2::mm_dma_s2mm_fot_count_fifo_pop_0;
   regNameToValue["mm_dma_s2mm_fot_count_fifo_pop_1"] = aie2::mm_dma_s2mm_fot_count_fifo_pop_1;
   regNameToValue["mm_dma_s2mm_status_0"] = aie2::mm_dma_s2mm_status_0;
   regNameToValue["mm_dma_s2mm_status_1"] = aie2::mm_dma_s2mm_status_1;
   regNameToValue["mm_ecc_failing_address"] = aie2::mm_ecc_failing_address;
   regNameToValue["mm_ecc_scrubbing_event"] = aie2::mm_ecc_scrubbing_event;
   regNameToValue["mm_edge_detection_event_control"] = aie2::mm_edge_detection_event_control;
   regNameToValue["mm_event_broadcast0"] = aie2::mm_event_broadcast0;
   regNameToValue["mm_event_broadcast1"] = aie2::mm_event_broadcast1;
   regNameToValue["mm_event_broadcast10"] = aie2::mm_event_broadcast10;
   regNameToValue["mm_event_broadcast11"] = aie2::mm_event_broadcast11;
   regNameToValue["mm_event_broadcast12"] = aie2::mm_event_broadcast12;
   regNameToValue["mm_event_broadcast13"] = aie2::mm_event_broadcast13;
   regNameToValue["mm_event_broadcast14"] = aie2::mm_event_broadcast14;
   regNameToValue["mm_event_broadcast15"] = aie2::mm_event_broadcast15;
   regNameToValue["mm_event_broadcast2"] = aie2::mm_event_broadcast2;
   regNameToValue["mm_event_broadcast3"] = aie2::mm_event_broadcast3;
   regNameToValue["mm_event_broadcast4"] = aie2::mm_event_broadcast4;
   regNameToValue["mm_event_broadcast5"] = aie2::mm_event_broadcast5;
   regNameToValue["mm_event_broadcast6"] = aie2::mm_event_broadcast6;
   regNameToValue["mm_event_broadcast7"] = aie2::mm_event_broadcast7;
   regNameToValue["mm_event_broadcast8"] = aie2::mm_event_broadcast8;
   regNameToValue["mm_event_broadcast9"] = aie2::mm_event_broadcast9;
   regNameToValue["mm_event_broadcast_block_east_clr"] = aie2::mm_event_broadcast_block_east_clr;
   regNameToValue["mm_event_broadcast_block_east_set"] = aie2::mm_event_broadcast_block_east_set;
   regNameToValue["mm_event_broadcast_block_east_value"] = aie2::mm_event_broadcast_block_east_value;
   regNameToValue["mm_event_broadcast_block_north_clr"] = aie2::mm_event_broadcast_block_north_clr;
   regNameToValue["mm_event_broadcast_block_north_set"] = aie2::mm_event_broadcast_block_north_set;
   regNameToValue["mm_event_broadcast_block_north_value"] = aie2::mm_event_broadcast_block_north_value;
   regNameToValue["mm_event_broadcast_block_south_clr"] = aie2::mm_event_broadcast_block_south_clr;
   regNameToValue["mm_event_broadcast_block_south_set"] = aie2::mm_event_broadcast_block_south_set;
   regNameToValue["mm_event_broadcast_block_south_value"] = aie2::mm_event_broadcast_block_south_value;
   regNameToValue["mm_event_broadcast_block_west_clr"] = aie2::mm_event_broadcast_block_west_clr;
   regNameToValue["mm_event_broadcast_block_west_set"] = aie2::mm_event_broadcast_block_west_set;
   regNameToValue["mm_event_broadcast_block_west_value"] = aie2::mm_event_broadcast_block_west_value;
   regNameToValue["mm_event_generate"] = aie2::mm_event_generate;
   regNameToValue["mm_event_group_0_enable"] = aie2::mm_event_group_0_enable;
   regNameToValue["mm_event_group_broadcast_enable"] = aie2::mm_event_group_broadcast_enable;
   regNameToValue["mm_event_group_dma_enable"] = aie2::mm_event_group_dma_enable;
   regNameToValue["mm_event_group_error_enable"] = aie2::mm_event_group_error_enable;
   regNameToValue["mm_event_group_lock_enable"] = aie2::mm_event_group_lock_enable;
   regNameToValue["mm_event_group_memory_conflict_enable"] = aie2::mm_event_group_memory_conflict_enable;
   regNameToValue["mm_event_group_user_event_enable"] = aie2::mm_event_group_user_event_enable;
   regNameToValue["mm_event_group_watchpoint_enable"] = aie2::mm_event_group_watchpoint_enable;
   regNameToValue["mm_event_status0"] = aie2::mm_event_status0;
   regNameToValue["mm_event_status1"] = aie2::mm_event_status1;
   regNameToValue["mm_event_status2"] = aie2::mm_event_status2;
   regNameToValue["mm_event_status3"] = aie2::mm_event_status3;
   regNameToValue["mm_lock0_value"] = aie2::mm_lock0_value;
   regNameToValue["mm_lock10_value"] = aie2::mm_lock10_value;
   regNameToValue["mm_lock11_value"] = aie2::mm_lock11_value;
   regNameToValue["mm_lock12_value"] = aie2::mm_lock12_value;
   regNameToValue["mm_lock13_value"] = aie2::mm_lock13_value;
   regNameToValue["mm_lock14_value"] = aie2::mm_lock14_value;
   regNameToValue["mm_lock15_value"] = aie2::mm_lock15_value;
   regNameToValue["mm_lock1_value"] = aie2::mm_lock1_value;
   regNameToValue["mm_lock2_value"] = aie2::mm_lock2_value;
   regNameToValue["mm_lock3_value"] = aie2::mm_lock3_value;
   regNameToValue["mm_lock4_value"] = aie2::mm_lock4_value;
   regNameToValue["mm_lock5_value"] = aie2::mm_lock5_value;
   regNameToValue["mm_lock6_value"] = aie2::mm_lock6_value;
   regNameToValue["mm_lock7_value"] = aie2::mm_lock7_value;
   regNameToValue["mm_lock8_value"] = aie2::mm_lock8_value;
   regNameToValue["mm_lock9_value"] = aie2::mm_lock9_value;
   regNameToValue["mm_lock_request"] = aie2::mm_lock_request;
   regNameToValue["mm_locks_event_selection_0"] = aie2::mm_locks_event_selection_0;
   regNameToValue["mm_locks_event_selection_1"] = aie2::mm_locks_event_selection_1;
   regNameToValue["mm_locks_event_selection_2"] = aie2::mm_locks_event_selection_2;
   regNameToValue["mm_locks_event_selection_3"] = aie2::mm_locks_event_selection_3;
   regNameToValue["mm_locks_event_selection_4"] = aie2::mm_locks_event_selection_4;
   regNameToValue["mm_locks_event_selection_5"] = aie2::mm_locks_event_selection_5;
   regNameToValue["mm_locks_event_selection_6"] = aie2::mm_locks_event_selection_6;
   regNameToValue["mm_locks_event_selection_7"] = aie2::mm_locks_event_selection_7;
   regNameToValue["mm_locks_overflow"] = aie2::mm_locks_overflow;
   regNameToValue["mm_locks_underflow"] = aie2::mm_locks_underflow;
   regNameToValue["mm_memory_control"] = aie2::mm_memory_control;
   regNameToValue["mm_parity_failing_address"] = aie2::mm_parity_failing_address;
   regNameToValue["mm_performance_control0"] = aie2::mm_performance_control0;
   regNameToValue["mm_performance_control1"] = aie2::mm_performance_control1;
   regNameToValue["mm_performance_counter0"] = aie2::mm_performance_counter0;
   regNameToValue["mm_performance_counter0_event_value"] = aie2::mm_performance_counter0_event_value;
   regNameToValue["mm_performance_counter1"] = aie2::mm_performance_counter1;
   regNameToValue["mm_performance_counter1_event_value"] = aie2::mm_performance_counter1_event_value;
   regNameToValue["mm_reserved0"] = aie2::mm_reserved0;
   regNameToValue["mm_reserved1"] = aie2::mm_reserved1;
   regNameToValue["mm_reserved2"] = aie2::mm_reserved2;
   regNameToValue["mm_reserved3"] = aie2::mm_reserved3;
   regNameToValue["mm_spare_reg"] = aie2::mm_spare_reg;
   regNameToValue["mm_timer_control"] = aie2::mm_timer_control;
   regNameToValue["mm_timer_high"] = aie2::mm_timer_high;
   regNameToValue["mm_timer_low"] = aie2::mm_timer_low;
   regNameToValue["mm_timer_trig_event_high_value"] = aie2::mm_timer_trig_event_high_value;
   regNameToValue["mm_timer_trig_event_low_value"] = aie2::mm_timer_trig_event_low_value;
   regNameToValue["mm_trace_control0"] = aie2::mm_trace_control0;
   regNameToValue["mm_trace_control1"] = aie2::mm_trace_control1;
   regNameToValue["mm_trace_event0"] = aie2::mm_trace_event0;
   regNameToValue["mm_trace_event1"] = aie2::mm_trace_event1;
   regNameToValue["mm_trace_status"] = aie2::mm_trace_status;
   regNameToValue["mm_watchpoint0"] = aie2::mm_watchpoint0;
   regNameToValue["mm_watchpoint1"] = aie2::mm_watchpoint1;
   regNameToValue["mem_cssd_trigger"] = aie2::mem_cssd_trigger;
   regNameToValue["mem_checkbit_error_generation"] = aie2::mem_checkbit_error_generation;
   regNameToValue["mem_combo_event_control"] = aie2::mem_combo_event_control;
   regNameToValue["mem_combo_event_inputs"] = aie2::mem_combo_event_inputs;
   regNameToValue["mem_dma_bd0_0"] = aie2::mem_dma_bd0_0;
   regNameToValue["mem_dma_bd0_1"] = aie2::mem_dma_bd0_1;
   regNameToValue["mem_dma_bd0_2"] = aie2::mem_dma_bd0_2;
   regNameToValue["mem_dma_bd0_3"] = aie2::mem_dma_bd0_3;
   regNameToValue["mem_dma_bd0_4"] = aie2::mem_dma_bd0_4;
   regNameToValue["mem_dma_bd0_5"] = aie2::mem_dma_bd0_5;
   regNameToValue["mem_dma_bd0_6"] = aie2::mem_dma_bd0_6;
   regNameToValue["mem_dma_bd0_7"] = aie2::mem_dma_bd0_7;
   regNameToValue["mem_dma_bd10_0"] = aie2::mem_dma_bd10_0;
   regNameToValue["mem_dma_bd10_1"] = aie2::mem_dma_bd10_1;
   regNameToValue["mem_dma_bd10_2"] = aie2::mem_dma_bd10_2;
   regNameToValue["mem_dma_bd10_3"] = aie2::mem_dma_bd10_3;
   regNameToValue["mem_dma_bd10_4"] = aie2::mem_dma_bd10_4;
   regNameToValue["mem_dma_bd10_5"] = aie2::mem_dma_bd10_5;
   regNameToValue["mem_dma_bd10_6"] = aie2::mem_dma_bd10_6;
   regNameToValue["mem_dma_bd10_7"] = aie2::mem_dma_bd10_7;
   regNameToValue["mem_dma_bd11_0"] = aie2::mem_dma_bd11_0;
   regNameToValue["mem_dma_bd11_1"] = aie2::mem_dma_bd11_1;
   regNameToValue["mem_dma_bd11_2"] = aie2::mem_dma_bd11_2;
   regNameToValue["mem_dma_bd11_3"] = aie2::mem_dma_bd11_3;
   regNameToValue["mem_dma_bd11_4"] = aie2::mem_dma_bd11_4;
   regNameToValue["mem_dma_bd11_5"] = aie2::mem_dma_bd11_5;
   regNameToValue["mem_dma_bd11_6"] = aie2::mem_dma_bd11_6;
   regNameToValue["mem_dma_bd11_7"] = aie2::mem_dma_bd11_7;
   regNameToValue["mem_dma_bd12_0"] = aie2::mem_dma_bd12_0;
   regNameToValue["mem_dma_bd12_1"] = aie2::mem_dma_bd12_1;
   regNameToValue["mem_dma_bd12_2"] = aie2::mem_dma_bd12_2;
   regNameToValue["mem_dma_bd12_3"] = aie2::mem_dma_bd12_3;
   regNameToValue["mem_dma_bd12_4"] = aie2::mem_dma_bd12_4;
   regNameToValue["mem_dma_bd12_5"] = aie2::mem_dma_bd12_5;
   regNameToValue["mem_dma_bd12_6"] = aie2::mem_dma_bd12_6;
   regNameToValue["mem_dma_bd12_7"] = aie2::mem_dma_bd12_7;
   regNameToValue["mem_dma_bd13_0"] = aie2::mem_dma_bd13_0;
   regNameToValue["mem_dma_bd13_1"] = aie2::mem_dma_bd13_1;
   regNameToValue["mem_dma_bd13_2"] = aie2::mem_dma_bd13_2;
   regNameToValue["mem_dma_bd13_3"] = aie2::mem_dma_bd13_3;
   regNameToValue["mem_dma_bd13_4"] = aie2::mem_dma_bd13_4;
   regNameToValue["mem_dma_bd13_5"] = aie2::mem_dma_bd13_5;
   regNameToValue["mem_dma_bd13_6"] = aie2::mem_dma_bd13_6;
   regNameToValue["mem_dma_bd13_7"] = aie2::mem_dma_bd13_7;
   regNameToValue["mem_dma_bd14_0"] = aie2::mem_dma_bd14_0;
   regNameToValue["mem_dma_bd14_1"] = aie2::mem_dma_bd14_1;
   regNameToValue["mem_dma_bd14_2"] = aie2::mem_dma_bd14_2;
   regNameToValue["mem_dma_bd14_3"] = aie2::mem_dma_bd14_3;
   regNameToValue["mem_dma_bd14_4"] = aie2::mem_dma_bd14_4;
   regNameToValue["mem_dma_bd14_5"] = aie2::mem_dma_bd14_5;
   regNameToValue["mem_dma_bd14_6"] = aie2::mem_dma_bd14_6;
   regNameToValue["mem_dma_bd14_7"] = aie2::mem_dma_bd14_7;
   regNameToValue["mem_dma_bd15_0"] = aie2::mem_dma_bd15_0;
   regNameToValue["mem_dma_bd15_1"] = aie2::mem_dma_bd15_1;
   regNameToValue["mem_dma_bd15_2"] = aie2::mem_dma_bd15_2;
   regNameToValue["mem_dma_bd15_3"] = aie2::mem_dma_bd15_3;
   regNameToValue["mem_dma_bd15_4"] = aie2::mem_dma_bd15_4;
   regNameToValue["mem_dma_bd15_5"] = aie2::mem_dma_bd15_5;
   regNameToValue["mem_dma_bd15_6"] = aie2::mem_dma_bd15_6;
   regNameToValue["mem_dma_bd15_7"] = aie2::mem_dma_bd15_7;
   regNameToValue["mem_dma_bd16_0"] = aie2::mem_dma_bd16_0;
   regNameToValue["mem_dma_bd16_1"] = aie2::mem_dma_bd16_1;
   regNameToValue["mem_dma_bd16_2"] = aie2::mem_dma_bd16_2;
   regNameToValue["mem_dma_bd16_3"] = aie2::mem_dma_bd16_3;
   regNameToValue["mem_dma_bd16_4"] = aie2::mem_dma_bd16_4;
   regNameToValue["mem_dma_bd16_5"] = aie2::mem_dma_bd16_5;
   regNameToValue["mem_dma_bd16_6"] = aie2::mem_dma_bd16_6;
   regNameToValue["mem_dma_bd16_7"] = aie2::mem_dma_bd16_7;
   regNameToValue["mem_dma_bd17_0"] = aie2::mem_dma_bd17_0;
   regNameToValue["mem_dma_bd17_1"] = aie2::mem_dma_bd17_1;
   regNameToValue["mem_dma_bd17_2"] = aie2::mem_dma_bd17_2;
   regNameToValue["mem_dma_bd17_3"] = aie2::mem_dma_bd17_3;
   regNameToValue["mem_dma_bd17_4"] = aie2::mem_dma_bd17_4;
   regNameToValue["mem_dma_bd17_5"] = aie2::mem_dma_bd17_5;
   regNameToValue["mem_dma_bd17_6"] = aie2::mem_dma_bd17_6;
   regNameToValue["mem_dma_bd17_7"] = aie2::mem_dma_bd17_7;
   regNameToValue["mem_dma_bd18_0"] = aie2::mem_dma_bd18_0;
   regNameToValue["mem_dma_bd18_1"] = aie2::mem_dma_bd18_1;
   regNameToValue["mem_dma_bd18_2"] = aie2::mem_dma_bd18_2;
   regNameToValue["mem_dma_bd18_3"] = aie2::mem_dma_bd18_3;
   regNameToValue["mem_dma_bd18_4"] = aie2::mem_dma_bd18_4;
   regNameToValue["mem_dma_bd18_5"] = aie2::mem_dma_bd18_5;
   regNameToValue["mem_dma_bd18_6"] = aie2::mem_dma_bd18_6;
   regNameToValue["mem_dma_bd18_7"] = aie2::mem_dma_bd18_7;
   regNameToValue["mem_dma_bd19_0"] = aie2::mem_dma_bd19_0;
   regNameToValue["mem_dma_bd19_1"] = aie2::mem_dma_bd19_1;
   regNameToValue["mem_dma_bd19_2"] = aie2::mem_dma_bd19_2;
   regNameToValue["mem_dma_bd19_3"] = aie2::mem_dma_bd19_3;
   regNameToValue["mem_dma_bd19_4"] = aie2::mem_dma_bd19_4;
   regNameToValue["mem_dma_bd19_5"] = aie2::mem_dma_bd19_5;
   regNameToValue["mem_dma_bd19_6"] = aie2::mem_dma_bd19_6;
   regNameToValue["mem_dma_bd19_7"] = aie2::mem_dma_bd19_7;
   regNameToValue["mem_dma_bd1_0"] = aie2::mem_dma_bd1_0;
   regNameToValue["mem_dma_bd1_1"] = aie2::mem_dma_bd1_1;
   regNameToValue["mem_dma_bd1_2"] = aie2::mem_dma_bd1_2;
   regNameToValue["mem_dma_bd1_3"] = aie2::mem_dma_bd1_3;
   regNameToValue["mem_dma_bd1_4"] = aie2::mem_dma_bd1_4;
   regNameToValue["mem_dma_bd1_5"] = aie2::mem_dma_bd1_5;
   regNameToValue["mem_dma_bd1_6"] = aie2::mem_dma_bd1_6;
   regNameToValue["mem_dma_bd1_7"] = aie2::mem_dma_bd1_7;
   regNameToValue["mem_dma_bd20_0"] = aie2::mem_dma_bd20_0;
   regNameToValue["mem_dma_bd20_1"] = aie2::mem_dma_bd20_1;
   regNameToValue["mem_dma_bd20_2"] = aie2::mem_dma_bd20_2;
   regNameToValue["mem_dma_bd20_3"] = aie2::mem_dma_bd20_3;
   regNameToValue["mem_dma_bd20_4"] = aie2::mem_dma_bd20_4;
   regNameToValue["mem_dma_bd20_5"] = aie2::mem_dma_bd20_5;
   regNameToValue["mem_dma_bd20_6"] = aie2::mem_dma_bd20_6;
   regNameToValue["mem_dma_bd20_7"] = aie2::mem_dma_bd20_7;
   regNameToValue["mem_dma_bd21_0"] = aie2::mem_dma_bd21_0;
   regNameToValue["mem_dma_bd21_1"] = aie2::mem_dma_bd21_1;
   regNameToValue["mem_dma_bd21_2"] = aie2::mem_dma_bd21_2;
   regNameToValue["mem_dma_bd21_3"] = aie2::mem_dma_bd21_3;
   regNameToValue["mem_dma_bd21_4"] = aie2::mem_dma_bd21_4;
   regNameToValue["mem_dma_bd21_5"] = aie2::mem_dma_bd21_5;
   regNameToValue["mem_dma_bd21_6"] = aie2::mem_dma_bd21_6;
   regNameToValue["mem_dma_bd21_7"] = aie2::mem_dma_bd21_7;
   regNameToValue["mem_dma_bd22_0"] = aie2::mem_dma_bd22_0;
   regNameToValue["mem_dma_bd22_1"] = aie2::mem_dma_bd22_1;
   regNameToValue["mem_dma_bd22_2"] = aie2::mem_dma_bd22_2;
   regNameToValue["mem_dma_bd22_3"] = aie2::mem_dma_bd22_3;
   regNameToValue["mem_dma_bd22_4"] = aie2::mem_dma_bd22_4;
   regNameToValue["mem_dma_bd22_5"] = aie2::mem_dma_bd22_5;
   regNameToValue["mem_dma_bd22_6"] = aie2::mem_dma_bd22_6;
   regNameToValue["mem_dma_bd22_7"] = aie2::mem_dma_bd22_7;
   regNameToValue["mem_dma_bd23_0"] = aie2::mem_dma_bd23_0;
   regNameToValue["mem_dma_bd23_1"] = aie2::mem_dma_bd23_1;
   regNameToValue["mem_dma_bd23_2"] = aie2::mem_dma_bd23_2;
   regNameToValue["mem_dma_bd23_3"] = aie2::mem_dma_bd23_3;
   regNameToValue["mem_dma_bd23_4"] = aie2::mem_dma_bd23_4;
   regNameToValue["mem_dma_bd23_5"] = aie2::mem_dma_bd23_5;
   regNameToValue["mem_dma_bd23_6"] = aie2::mem_dma_bd23_6;
   regNameToValue["mem_dma_bd23_7"] = aie2::mem_dma_bd23_7;
   regNameToValue["mem_dma_bd24_0"] = aie2::mem_dma_bd24_0;
   regNameToValue["mem_dma_bd24_1"] = aie2::mem_dma_bd24_1;
   regNameToValue["mem_dma_bd24_2"] = aie2::mem_dma_bd24_2;
   regNameToValue["mem_dma_bd24_3"] = aie2::mem_dma_bd24_3;
   regNameToValue["mem_dma_bd24_4"] = aie2::mem_dma_bd24_4;
   regNameToValue["mem_dma_bd24_5"] = aie2::mem_dma_bd24_5;
   regNameToValue["mem_dma_bd24_6"] = aie2::mem_dma_bd24_6;
   regNameToValue["mem_dma_bd24_7"] = aie2::mem_dma_bd24_7;
   regNameToValue["mem_dma_bd25_0"] = aie2::mem_dma_bd25_0;
   regNameToValue["mem_dma_bd25_1"] = aie2::mem_dma_bd25_1;
   regNameToValue["mem_dma_bd25_2"] = aie2::mem_dma_bd25_2;
   regNameToValue["mem_dma_bd25_3"] = aie2::mem_dma_bd25_3;
   regNameToValue["mem_dma_bd25_4"] = aie2::mem_dma_bd25_4;
   regNameToValue["mem_dma_bd25_5"] = aie2::mem_dma_bd25_5;
   regNameToValue["mem_dma_bd25_6"] = aie2::mem_dma_bd25_6;
   regNameToValue["mem_dma_bd25_7"] = aie2::mem_dma_bd25_7;
   regNameToValue["mem_dma_bd26_0"] = aie2::mem_dma_bd26_0;
   regNameToValue["mem_dma_bd26_1"] = aie2::mem_dma_bd26_1;
   regNameToValue["mem_dma_bd26_2"] = aie2::mem_dma_bd26_2;
   regNameToValue["mem_dma_bd26_3"] = aie2::mem_dma_bd26_3;
   regNameToValue["mem_dma_bd26_4"] = aie2::mem_dma_bd26_4;
   regNameToValue["mem_dma_bd26_5"] = aie2::mem_dma_bd26_5;
   regNameToValue["mem_dma_bd26_6"] = aie2::mem_dma_bd26_6;
   regNameToValue["mem_dma_bd26_7"] = aie2::mem_dma_bd26_7;
   regNameToValue["mem_dma_bd27_0"] = aie2::mem_dma_bd27_0;
   regNameToValue["mem_dma_bd27_1"] = aie2::mem_dma_bd27_1;
   regNameToValue["mem_dma_bd27_2"] = aie2::mem_dma_bd27_2;
   regNameToValue["mem_dma_bd27_3"] = aie2::mem_dma_bd27_3;
   regNameToValue["mem_dma_bd27_4"] = aie2::mem_dma_bd27_4;
   regNameToValue["mem_dma_bd27_5"] = aie2::mem_dma_bd27_5;
   regNameToValue["mem_dma_bd27_6"] = aie2::mem_dma_bd27_6;
   regNameToValue["mem_dma_bd27_7"] = aie2::mem_dma_bd27_7;
   regNameToValue["mem_dma_bd28_0"] = aie2::mem_dma_bd28_0;
   regNameToValue["mem_dma_bd28_1"] = aie2::mem_dma_bd28_1;
   regNameToValue["mem_dma_bd28_2"] = aie2::mem_dma_bd28_2;
   regNameToValue["mem_dma_bd28_3"] = aie2::mem_dma_bd28_3;
   regNameToValue["mem_dma_bd28_4"] = aie2::mem_dma_bd28_4;
   regNameToValue["mem_dma_bd28_5"] = aie2::mem_dma_bd28_5;
   regNameToValue["mem_dma_bd28_6"] = aie2::mem_dma_bd28_6;
   regNameToValue["mem_dma_bd28_7"] = aie2::mem_dma_bd28_7;
   regNameToValue["mem_dma_bd29_0"] = aie2::mem_dma_bd29_0;
   regNameToValue["mem_dma_bd29_1"] = aie2::mem_dma_bd29_1;
   regNameToValue["mem_dma_bd29_2"] = aie2::mem_dma_bd29_2;
   regNameToValue["mem_dma_bd29_3"] = aie2::mem_dma_bd29_3;
   regNameToValue["mem_dma_bd29_4"] = aie2::mem_dma_bd29_4;
   regNameToValue["mem_dma_bd29_5"] = aie2::mem_dma_bd29_5;
   regNameToValue["mem_dma_bd29_6"] = aie2::mem_dma_bd29_6;
   regNameToValue["mem_dma_bd29_7"] = aie2::mem_dma_bd29_7;
   regNameToValue["mem_dma_bd2_0"] = aie2::mem_dma_bd2_0;
   regNameToValue["mem_dma_bd2_1"] = aie2::mem_dma_bd2_1;
   regNameToValue["mem_dma_bd2_2"] = aie2::mem_dma_bd2_2;
   regNameToValue["mem_dma_bd2_3"] = aie2::mem_dma_bd2_3;
   regNameToValue["mem_dma_bd2_4"] = aie2::mem_dma_bd2_4;
   regNameToValue["mem_dma_bd2_5"] = aie2::mem_dma_bd2_5;
   regNameToValue["mem_dma_bd2_6"] = aie2::mem_dma_bd2_6;
   regNameToValue["mem_dma_bd2_7"] = aie2::mem_dma_bd2_7;
   regNameToValue["mem_dma_bd30_0"] = aie2::mem_dma_bd30_0;
   regNameToValue["mem_dma_bd30_1"] = aie2::mem_dma_bd30_1;
   regNameToValue["mem_dma_bd30_2"] = aie2::mem_dma_bd30_2;
   regNameToValue["mem_dma_bd30_3"] = aie2::mem_dma_bd30_3;
   regNameToValue["mem_dma_bd30_4"] = aie2::mem_dma_bd30_4;
   regNameToValue["mem_dma_bd30_5"] = aie2::mem_dma_bd30_5;
   regNameToValue["mem_dma_bd30_6"] = aie2::mem_dma_bd30_6;
   regNameToValue["mem_dma_bd30_7"] = aie2::mem_dma_bd30_7;
   regNameToValue["mem_dma_bd31_0"] = aie2::mem_dma_bd31_0;
   regNameToValue["mem_dma_bd31_1"] = aie2::mem_dma_bd31_1;
   regNameToValue["mem_dma_bd31_2"] = aie2::mem_dma_bd31_2;
   regNameToValue["mem_dma_bd31_3"] = aie2::mem_dma_bd31_3;
   regNameToValue["mem_dma_bd31_4"] = aie2::mem_dma_bd31_4;
   regNameToValue["mem_dma_bd31_5"] = aie2::mem_dma_bd31_5;
   regNameToValue["mem_dma_bd31_6"] = aie2::mem_dma_bd31_6;
   regNameToValue["mem_dma_bd31_7"] = aie2::mem_dma_bd31_7;
   regNameToValue["mem_dma_bd32_0"] = aie2::mem_dma_bd32_0;
   regNameToValue["mem_dma_bd32_1"] = aie2::mem_dma_bd32_1;
   regNameToValue["mem_dma_bd32_2"] = aie2::mem_dma_bd32_2;
   regNameToValue["mem_dma_bd32_3"] = aie2::mem_dma_bd32_3;
   regNameToValue["mem_dma_bd32_4"] = aie2::mem_dma_bd32_4;
   regNameToValue["mem_dma_bd32_5"] = aie2::mem_dma_bd32_5;
   regNameToValue["mem_dma_bd32_6"] = aie2::mem_dma_bd32_6;
   regNameToValue["mem_dma_bd32_7"] = aie2::mem_dma_bd32_7;
   regNameToValue["mem_dma_bd33_0"] = aie2::mem_dma_bd33_0;
   regNameToValue["mem_dma_bd33_1"] = aie2::mem_dma_bd33_1;
   regNameToValue["mem_dma_bd33_2"] = aie2::mem_dma_bd33_2;
   regNameToValue["mem_dma_bd33_3"] = aie2::mem_dma_bd33_3;
   regNameToValue["mem_dma_bd33_4"] = aie2::mem_dma_bd33_4;
   regNameToValue["mem_dma_bd33_5"] = aie2::mem_dma_bd33_5;
   regNameToValue["mem_dma_bd33_6"] = aie2::mem_dma_bd33_6;
   regNameToValue["mem_dma_bd33_7"] = aie2::mem_dma_bd33_7;
   regNameToValue["mem_dma_bd34_0"] = aie2::mem_dma_bd34_0;
   regNameToValue["mem_dma_bd34_1"] = aie2::mem_dma_bd34_1;
   regNameToValue["mem_dma_bd34_2"] = aie2::mem_dma_bd34_2;
   regNameToValue["mem_dma_bd34_3"] = aie2::mem_dma_bd34_3;
   regNameToValue["mem_dma_bd34_4"] = aie2::mem_dma_bd34_4;
   regNameToValue["mem_dma_bd34_5"] = aie2::mem_dma_bd34_5;
   regNameToValue["mem_dma_bd34_6"] = aie2::mem_dma_bd34_6;
   regNameToValue["mem_dma_bd34_7"] = aie2::mem_dma_bd34_7;
   regNameToValue["mem_dma_bd35_0"] = aie2::mem_dma_bd35_0;
   regNameToValue["mem_dma_bd35_1"] = aie2::mem_dma_bd35_1;
   regNameToValue["mem_dma_bd35_2"] = aie2::mem_dma_bd35_2;
   regNameToValue["mem_dma_bd35_3"] = aie2::mem_dma_bd35_3;
   regNameToValue["mem_dma_bd35_4"] = aie2::mem_dma_bd35_4;
   regNameToValue["mem_dma_bd35_5"] = aie2::mem_dma_bd35_5;
   regNameToValue["mem_dma_bd35_6"] = aie2::mem_dma_bd35_6;
   regNameToValue["mem_dma_bd35_7"] = aie2::mem_dma_bd35_7;
   regNameToValue["mem_dma_bd36_0"] = aie2::mem_dma_bd36_0;
   regNameToValue["mem_dma_bd36_1"] = aie2::mem_dma_bd36_1;
   regNameToValue["mem_dma_bd36_2"] = aie2::mem_dma_bd36_2;
   regNameToValue["mem_dma_bd36_3"] = aie2::mem_dma_bd36_3;
   regNameToValue["mem_dma_bd36_4"] = aie2::mem_dma_bd36_4;
   regNameToValue["mem_dma_bd36_5"] = aie2::mem_dma_bd36_5;
   regNameToValue["mem_dma_bd36_6"] = aie2::mem_dma_bd36_6;
   regNameToValue["mem_dma_bd36_7"] = aie2::mem_dma_bd36_7;
   regNameToValue["mem_dma_bd37_0"] = aie2::mem_dma_bd37_0;
   regNameToValue["mem_dma_bd37_1"] = aie2::mem_dma_bd37_1;
   regNameToValue["mem_dma_bd37_2"] = aie2::mem_dma_bd37_2;
   regNameToValue["mem_dma_bd37_3"] = aie2::mem_dma_bd37_3;
   regNameToValue["mem_dma_bd37_4"] = aie2::mem_dma_bd37_4;
   regNameToValue["mem_dma_bd37_5"] = aie2::mem_dma_bd37_5;
   regNameToValue["mem_dma_bd37_6"] = aie2::mem_dma_bd37_6;
   regNameToValue["mem_dma_bd37_7"] = aie2::mem_dma_bd37_7;
   regNameToValue["mem_dma_bd38_0"] = aie2::mem_dma_bd38_0;
   regNameToValue["mem_dma_bd38_1"] = aie2::mem_dma_bd38_1;
   regNameToValue["mem_dma_bd38_2"] = aie2::mem_dma_bd38_2;
   regNameToValue["mem_dma_bd38_3"] = aie2::mem_dma_bd38_3;
   regNameToValue["mem_dma_bd38_4"] = aie2::mem_dma_bd38_4;
   regNameToValue["mem_dma_bd38_5"] = aie2::mem_dma_bd38_5;
   regNameToValue["mem_dma_bd38_6"] = aie2::mem_dma_bd38_6;
   regNameToValue["mem_dma_bd38_7"] = aie2::mem_dma_bd38_7;
   regNameToValue["mem_dma_bd39_0"] = aie2::mem_dma_bd39_0;
   regNameToValue["mem_dma_bd39_1"] = aie2::mem_dma_bd39_1;
   regNameToValue["mem_dma_bd39_2"] = aie2::mem_dma_bd39_2;
   regNameToValue["mem_dma_bd39_3"] = aie2::mem_dma_bd39_3;
   regNameToValue["mem_dma_bd39_4"] = aie2::mem_dma_bd39_4;
   regNameToValue["mem_dma_bd39_5"] = aie2::mem_dma_bd39_5;
   regNameToValue["mem_dma_bd39_6"] = aie2::mem_dma_bd39_6;
   regNameToValue["mem_dma_bd39_7"] = aie2::mem_dma_bd39_7;
   regNameToValue["mem_dma_bd3_0"] = aie2::mem_dma_bd3_0;
   regNameToValue["mem_dma_bd3_1"] = aie2::mem_dma_bd3_1;
   regNameToValue["mem_dma_bd3_2"] = aie2::mem_dma_bd3_2;
   regNameToValue["mem_dma_bd3_3"] = aie2::mem_dma_bd3_3;
   regNameToValue["mem_dma_bd3_4"] = aie2::mem_dma_bd3_4;
   regNameToValue["mem_dma_bd3_5"] = aie2::mem_dma_bd3_5;
   regNameToValue["mem_dma_bd3_6"] = aie2::mem_dma_bd3_6;
   regNameToValue["mem_dma_bd3_7"] = aie2::mem_dma_bd3_7;
   regNameToValue["mem_dma_bd40_0"] = aie2::mem_dma_bd40_0;
   regNameToValue["mem_dma_bd40_1"] = aie2::mem_dma_bd40_1;
   regNameToValue["mem_dma_bd40_2"] = aie2::mem_dma_bd40_2;
   regNameToValue["mem_dma_bd40_3"] = aie2::mem_dma_bd40_3;
   regNameToValue["mem_dma_bd40_4"] = aie2::mem_dma_bd40_4;
   regNameToValue["mem_dma_bd40_5"] = aie2::mem_dma_bd40_5;
   regNameToValue["mem_dma_bd40_6"] = aie2::mem_dma_bd40_6;
   regNameToValue["mem_dma_bd40_7"] = aie2::mem_dma_bd40_7;
   regNameToValue["mem_dma_bd41_0"] = aie2::mem_dma_bd41_0;
   regNameToValue["mem_dma_bd41_1"] = aie2::mem_dma_bd41_1;
   regNameToValue["mem_dma_bd41_2"] = aie2::mem_dma_bd41_2;
   regNameToValue["mem_dma_bd41_3"] = aie2::mem_dma_bd41_3;
   regNameToValue["mem_dma_bd41_4"] = aie2::mem_dma_bd41_4;
   regNameToValue["mem_dma_bd41_5"] = aie2::mem_dma_bd41_5;
   regNameToValue["mem_dma_bd41_6"] = aie2::mem_dma_bd41_6;
   regNameToValue["mem_dma_bd41_7"] = aie2::mem_dma_bd41_7;
   regNameToValue["mem_dma_bd42_0"] = aie2::mem_dma_bd42_0;
   regNameToValue["mem_dma_bd42_1"] = aie2::mem_dma_bd42_1;
   regNameToValue["mem_dma_bd42_2"] = aie2::mem_dma_bd42_2;
   regNameToValue["mem_dma_bd42_3"] = aie2::mem_dma_bd42_3;
   regNameToValue["mem_dma_bd42_4"] = aie2::mem_dma_bd42_4;
   regNameToValue["mem_dma_bd42_5"] = aie2::mem_dma_bd42_5;
   regNameToValue["mem_dma_bd42_6"] = aie2::mem_dma_bd42_6;
   regNameToValue["mem_dma_bd42_7"] = aie2::mem_dma_bd42_7;
   regNameToValue["mem_dma_bd43_0"] = aie2::mem_dma_bd43_0;
   regNameToValue["mem_dma_bd43_1"] = aie2::mem_dma_bd43_1;
   regNameToValue["mem_dma_bd43_2"] = aie2::mem_dma_bd43_2;
   regNameToValue["mem_dma_bd43_3"] = aie2::mem_dma_bd43_3;
   regNameToValue["mem_dma_bd43_4"] = aie2::mem_dma_bd43_4;
   regNameToValue["mem_dma_bd43_5"] = aie2::mem_dma_bd43_5;
   regNameToValue["mem_dma_bd43_6"] = aie2::mem_dma_bd43_6;
   regNameToValue["mem_dma_bd43_7"] = aie2::mem_dma_bd43_7;
   regNameToValue["mem_dma_bd44_0"] = aie2::mem_dma_bd44_0;
   regNameToValue["mem_dma_bd44_1"] = aie2::mem_dma_bd44_1;
   regNameToValue["mem_dma_bd44_2"] = aie2::mem_dma_bd44_2;
   regNameToValue["mem_dma_bd44_3"] = aie2::mem_dma_bd44_3;
   regNameToValue["mem_dma_bd44_4"] = aie2::mem_dma_bd44_4;
   regNameToValue["mem_dma_bd44_5"] = aie2::mem_dma_bd44_5;
   regNameToValue["mem_dma_bd44_6"] = aie2::mem_dma_bd44_6;
   regNameToValue["mem_dma_bd44_7"] = aie2::mem_dma_bd44_7;
   regNameToValue["mem_dma_bd45_0"] = aie2::mem_dma_bd45_0;
   regNameToValue["mem_dma_bd45_1"] = aie2::mem_dma_bd45_1;
   regNameToValue["mem_dma_bd45_2"] = aie2::mem_dma_bd45_2;
   regNameToValue["mem_dma_bd45_3"] = aie2::mem_dma_bd45_3;
   regNameToValue["mem_dma_bd45_4"] = aie2::mem_dma_bd45_4;
   regNameToValue["mem_dma_bd45_5"] = aie2::mem_dma_bd45_5;
   regNameToValue["mem_dma_bd45_6"] = aie2::mem_dma_bd45_6;
   regNameToValue["mem_dma_bd45_7"] = aie2::mem_dma_bd45_7;
   regNameToValue["mem_dma_bd46_0"] = aie2::mem_dma_bd46_0;
   regNameToValue["mem_dma_bd46_1"] = aie2::mem_dma_bd46_1;
   regNameToValue["mem_dma_bd46_2"] = aie2::mem_dma_bd46_2;
   regNameToValue["mem_dma_bd46_3"] = aie2::mem_dma_bd46_3;
   regNameToValue["mem_dma_bd46_4"] = aie2::mem_dma_bd46_4;
   regNameToValue["mem_dma_bd46_5"] = aie2::mem_dma_bd46_5;
   regNameToValue["mem_dma_bd46_6"] = aie2::mem_dma_bd46_6;
   regNameToValue["mem_dma_bd46_7"] = aie2::mem_dma_bd46_7;
   regNameToValue["mem_dma_bd47_0"] = aie2::mem_dma_bd47_0;
   regNameToValue["mem_dma_bd47_1"] = aie2::mem_dma_bd47_1;
   regNameToValue["mem_dma_bd47_2"] = aie2::mem_dma_bd47_2;
   regNameToValue["mem_dma_bd47_3"] = aie2::mem_dma_bd47_3;
   regNameToValue["mem_dma_bd47_4"] = aie2::mem_dma_bd47_4;
   regNameToValue["mem_dma_bd47_5"] = aie2::mem_dma_bd47_5;
   regNameToValue["mem_dma_bd47_6"] = aie2::mem_dma_bd47_6;
   regNameToValue["mem_dma_bd47_7"] = aie2::mem_dma_bd47_7;
   regNameToValue["mem_dma_bd4_0"] = aie2::mem_dma_bd4_0;
   regNameToValue["mem_dma_bd4_1"] = aie2::mem_dma_bd4_1;
   regNameToValue["mem_dma_bd4_2"] = aie2::mem_dma_bd4_2;
   regNameToValue["mem_dma_bd4_3"] = aie2::mem_dma_bd4_3;
   regNameToValue["mem_dma_bd4_4"] = aie2::mem_dma_bd4_4;
   regNameToValue["mem_dma_bd4_5"] = aie2::mem_dma_bd4_5;
   regNameToValue["mem_dma_bd4_6"] = aie2::mem_dma_bd4_6;
   regNameToValue["mem_dma_bd4_7"] = aie2::mem_dma_bd4_7;
   regNameToValue["mem_dma_bd5_0"] = aie2::mem_dma_bd5_0;
   regNameToValue["mem_dma_bd5_1"] = aie2::mem_dma_bd5_1;
   regNameToValue["mem_dma_bd5_2"] = aie2::mem_dma_bd5_2;
   regNameToValue["mem_dma_bd5_3"] = aie2::mem_dma_bd5_3;
   regNameToValue["mem_dma_bd5_4"] = aie2::mem_dma_bd5_4;
   regNameToValue["mem_dma_bd5_5"] = aie2::mem_dma_bd5_5;
   regNameToValue["mem_dma_bd5_6"] = aie2::mem_dma_bd5_6;
   regNameToValue["mem_dma_bd5_7"] = aie2::mem_dma_bd5_7;
   regNameToValue["mem_dma_bd6_0"] = aie2::mem_dma_bd6_0;
   regNameToValue["mem_dma_bd6_1"] = aie2::mem_dma_bd6_1;
   regNameToValue["mem_dma_bd6_2"] = aie2::mem_dma_bd6_2;
   regNameToValue["mem_dma_bd6_3"] = aie2::mem_dma_bd6_3;
   regNameToValue["mem_dma_bd6_4"] = aie2::mem_dma_bd6_4;
   regNameToValue["mem_dma_bd6_5"] = aie2::mem_dma_bd6_5;
   regNameToValue["mem_dma_bd6_6"] = aie2::mem_dma_bd6_6;
   regNameToValue["mem_dma_bd6_7"] = aie2::mem_dma_bd6_7;
   regNameToValue["mem_dma_bd7_0"] = aie2::mem_dma_bd7_0;
   regNameToValue["mem_dma_bd7_1"] = aie2::mem_dma_bd7_1;
   regNameToValue["mem_dma_bd7_2"] = aie2::mem_dma_bd7_2;
   regNameToValue["mem_dma_bd7_3"] = aie2::mem_dma_bd7_3;
   regNameToValue["mem_dma_bd7_4"] = aie2::mem_dma_bd7_4;
   regNameToValue["mem_dma_bd7_5"] = aie2::mem_dma_bd7_5;
   regNameToValue["mem_dma_bd7_6"] = aie2::mem_dma_bd7_6;
   regNameToValue["mem_dma_bd7_7"] = aie2::mem_dma_bd7_7;
   regNameToValue["mem_dma_bd8_0"] = aie2::mem_dma_bd8_0;
   regNameToValue["mem_dma_bd8_1"] = aie2::mem_dma_bd8_1;
   regNameToValue["mem_dma_bd8_2"] = aie2::mem_dma_bd8_2;
   regNameToValue["mem_dma_bd8_3"] = aie2::mem_dma_bd8_3;
   regNameToValue["mem_dma_bd8_4"] = aie2::mem_dma_bd8_4;
   regNameToValue["mem_dma_bd8_5"] = aie2::mem_dma_bd8_5;
   regNameToValue["mem_dma_bd8_6"] = aie2::mem_dma_bd8_6;
   regNameToValue["mem_dma_bd8_7"] = aie2::mem_dma_bd8_7;
   regNameToValue["mem_dma_bd9_0"] = aie2::mem_dma_bd9_0;
   regNameToValue["mem_dma_bd9_1"] = aie2::mem_dma_bd9_1;
   regNameToValue["mem_dma_bd9_2"] = aie2::mem_dma_bd9_2;
   regNameToValue["mem_dma_bd9_3"] = aie2::mem_dma_bd9_3;
   regNameToValue["mem_dma_bd9_4"] = aie2::mem_dma_bd9_4;
   regNameToValue["mem_dma_bd9_5"] = aie2::mem_dma_bd9_5;
   regNameToValue["mem_dma_bd9_6"] = aie2::mem_dma_bd9_6;
   regNameToValue["mem_dma_bd9_7"] = aie2::mem_dma_bd9_7;
   regNameToValue["mem_dma_event_channel_selection"] = aie2::mem_dma_event_channel_selection;
   regNameToValue["mem_dma_mm2s_0_ctrl"] = aie2::mem_dma_mm2s_0_ctrl;
   regNameToValue["mem_dma_mm2s_0_start_queue"] = aie2::mem_dma_mm2s_0_start_queue;
   regNameToValue["mem_dma_mm2s_1_ctrl"] = aie2::mem_dma_mm2s_1_ctrl;
   regNameToValue["mem_dma_mm2s_1_start_queue"] = aie2::mem_dma_mm2s_1_start_queue;
   regNameToValue["mem_dma_mm2s_2_ctrl"] = aie2::mem_dma_mm2s_2_ctrl;
   regNameToValue["mem_dma_mm2s_2_start_queue"] = aie2::mem_dma_mm2s_2_start_queue;
   regNameToValue["mem_dma_mm2s_3_ctrl"] = aie2::mem_dma_mm2s_3_ctrl;
   regNameToValue["mem_dma_mm2s_3_start_queue"] = aie2::mem_dma_mm2s_3_start_queue;
   regNameToValue["mem_dma_mm2s_4_ctrl"] = aie2::mem_dma_mm2s_4_ctrl;
   regNameToValue["mem_dma_mm2s_4_start_queue"] = aie2::mem_dma_mm2s_4_start_queue;
   regNameToValue["mem_dma_mm2s_5_ctrl"] = aie2::mem_dma_mm2s_5_ctrl;
   regNameToValue["mem_dma_mm2s_5_start_queue"] = aie2::mem_dma_mm2s_5_start_queue;
   regNameToValue["mem_dma_mm2s_status_0"] = aie2::mem_dma_mm2s_status_0;
   regNameToValue["mem_dma_mm2s_status_1"] = aie2::mem_dma_mm2s_status_1;
   regNameToValue["mem_dma_mm2s_status_2"] = aie2::mem_dma_mm2s_status_2;
   regNameToValue["mem_dma_mm2s_status_3"] = aie2::mem_dma_mm2s_status_3;
   regNameToValue["mem_dma_mm2s_status_4"] = aie2::mem_dma_mm2s_status_4;
   regNameToValue["mem_dma_mm2s_status_5"] = aie2::mem_dma_mm2s_status_5;
   regNameToValue["mem_dma_s2mm_0_ctrl"] = aie2::mem_dma_s2mm_0_ctrl;
   regNameToValue["mem_dma_s2mm_0_start_queue"] = aie2::mem_dma_s2mm_0_start_queue;
   regNameToValue["mem_dma_s2mm_1_ctrl"] = aie2::mem_dma_s2mm_1_ctrl;
   regNameToValue["mem_dma_s2mm_1_start_queue"] = aie2::mem_dma_s2mm_1_start_queue;
   regNameToValue["mem_dma_s2mm_2_ctrl"] = aie2::mem_dma_s2mm_2_ctrl;
   regNameToValue["mem_dma_s2mm_2_start_queue"] = aie2::mem_dma_s2mm_2_start_queue;
   regNameToValue["mem_dma_s2mm_3_ctrl"] = aie2::mem_dma_s2mm_3_ctrl;
   regNameToValue["mem_dma_s2mm_3_start_queue"] = aie2::mem_dma_s2mm_3_start_queue;
   regNameToValue["mem_dma_s2mm_4_ctrl"] = aie2::mem_dma_s2mm_4_ctrl;
   regNameToValue["mem_dma_s2mm_4_start_queue"] = aie2::mem_dma_s2mm_4_start_queue;
   regNameToValue["mem_dma_s2mm_5_ctrl"] = aie2::mem_dma_s2mm_5_ctrl;
   regNameToValue["mem_dma_s2mm_5_start_queue"] = aie2::mem_dma_s2mm_5_start_queue;
   regNameToValue["mem_dma_s2mm_current_write_count_0"] = aie2::mem_dma_s2mm_current_write_count_0;
   regNameToValue["mem_dma_s2mm_current_write_count_1"] = aie2::mem_dma_s2mm_current_write_count_1;
   regNameToValue["mem_dma_s2mm_current_write_count_2"] = aie2::mem_dma_s2mm_current_write_count_2;
   regNameToValue["mem_dma_s2mm_current_write_count_3"] = aie2::mem_dma_s2mm_current_write_count_3;
   regNameToValue["mem_dma_s2mm_current_write_count_4"] = aie2::mem_dma_s2mm_current_write_count_4;
   regNameToValue["mem_dma_s2mm_current_write_count_5"] = aie2::mem_dma_s2mm_current_write_count_5;
   regNameToValue["mem_dma_s2mm_fot_count_fifo_pop_0"] = aie2::mem_dma_s2mm_fot_count_fifo_pop_0;
   regNameToValue["mem_dma_s2mm_fot_count_fifo_pop_1"] = aie2::mem_dma_s2mm_fot_count_fifo_pop_1;
   regNameToValue["mem_dma_s2mm_fot_count_fifo_pop_2"] = aie2::mem_dma_s2mm_fot_count_fifo_pop_2;
   regNameToValue["mem_dma_s2mm_fot_count_fifo_pop_3"] = aie2::mem_dma_s2mm_fot_count_fifo_pop_3;
   regNameToValue["mem_dma_s2mm_fot_count_fifo_pop_4"] = aie2::mem_dma_s2mm_fot_count_fifo_pop_4;
   regNameToValue["mem_dma_s2mm_fot_count_fifo_pop_5"] = aie2::mem_dma_s2mm_fot_count_fifo_pop_5;
   regNameToValue["mem_dma_s2mm_status_0"] = aie2::mem_dma_s2mm_status_0;
   regNameToValue["mem_dma_s2mm_status_1"] = aie2::mem_dma_s2mm_status_1;
   regNameToValue["mem_dma_s2mm_status_2"] = aie2::mem_dma_s2mm_status_2;
   regNameToValue["mem_dma_s2mm_status_3"] = aie2::mem_dma_s2mm_status_3;
   regNameToValue["mem_dma_s2mm_status_4"] = aie2::mem_dma_s2mm_status_4;
   regNameToValue["mem_dma_s2mm_status_5"] = aie2::mem_dma_s2mm_status_5;
   regNameToValue["mem_ecc_failing_address"] = aie2::mem_ecc_failing_address;
   regNameToValue["mem_ecc_scrubbing_event"] = aie2::mem_ecc_scrubbing_event;
   regNameToValue["mem_edge_detection_event_control"] = aie2::mem_edge_detection_event_control;
   regNameToValue["mem_event_broadcast0"] = aie2::mem_event_broadcast0;
   regNameToValue["mem_event_broadcast1"] = aie2::mem_event_broadcast1;
   regNameToValue["mem_event_broadcast10"] = aie2::mem_event_broadcast10;
   regNameToValue["mem_event_broadcast11"] = aie2::mem_event_broadcast11;
   regNameToValue["mem_event_broadcast12"] = aie2::mem_event_broadcast12;
   regNameToValue["mem_event_broadcast13"] = aie2::mem_event_broadcast13;
   regNameToValue["mem_event_broadcast14"] = aie2::mem_event_broadcast14;
   regNameToValue["mem_event_broadcast15"] = aie2::mem_event_broadcast15;
   regNameToValue["mem_event_broadcast2"] = aie2::mem_event_broadcast2;
   regNameToValue["mem_event_broadcast3"] = aie2::mem_event_broadcast3;
   regNameToValue["mem_event_broadcast4"] = aie2::mem_event_broadcast4;
   regNameToValue["mem_event_broadcast5"] = aie2::mem_event_broadcast5;
   regNameToValue["mem_event_broadcast6"] = aie2::mem_event_broadcast6;
   regNameToValue["mem_event_broadcast7"] = aie2::mem_event_broadcast7;
   regNameToValue["mem_event_broadcast8"] = aie2::mem_event_broadcast8;
   regNameToValue["mem_event_broadcast9"] = aie2::mem_event_broadcast9;
   regNameToValue["mem_event_broadcast_a_block_east_clr"] = aie2::mem_event_broadcast_a_block_east_clr;
   regNameToValue["mem_event_broadcast_a_block_east_set"] = aie2::mem_event_broadcast_a_block_east_set;
   regNameToValue["mem_event_broadcast_a_block_east_value"] = aie2::mem_event_broadcast_a_block_east_value;
   regNameToValue["mem_event_broadcast_a_block_north_clr"] = aie2::mem_event_broadcast_a_block_north_clr;
   regNameToValue["mem_event_broadcast_a_block_north_set"] = aie2::mem_event_broadcast_a_block_north_set;
   regNameToValue["mem_event_broadcast_a_block_north_value"] = aie2::mem_event_broadcast_a_block_north_value;
   regNameToValue["mem_event_broadcast_a_block_south_clr"] = aie2::mem_event_broadcast_a_block_south_clr;
   regNameToValue["mem_event_broadcast_a_block_south_set"] = aie2::mem_event_broadcast_a_block_south_set;
   regNameToValue["mem_event_broadcast_a_block_south_value"] = aie2::mem_event_broadcast_a_block_south_value;
   regNameToValue["mem_event_broadcast_a_block_west_clr"] = aie2::mem_event_broadcast_a_block_west_clr;
   regNameToValue["mem_event_broadcast_a_block_west_set"] = aie2::mem_event_broadcast_a_block_west_set;
   regNameToValue["mem_event_broadcast_a_block_west_value"] = aie2::mem_event_broadcast_a_block_west_value;
   regNameToValue["mem_event_broadcast_b_block_east_clr"] = aie2::mem_event_broadcast_b_block_east_clr;
   regNameToValue["mem_event_broadcast_b_block_east_set"] = aie2::mem_event_broadcast_b_block_east_set;
   regNameToValue["mem_event_broadcast_b_block_east_value"] = aie2::mem_event_broadcast_b_block_east_value;
   regNameToValue["mem_event_broadcast_b_block_north_clr"] = aie2::mem_event_broadcast_b_block_north_clr;
   regNameToValue["mem_event_broadcast_b_block_north_set"] = aie2::mem_event_broadcast_b_block_north_set;
   regNameToValue["mem_event_broadcast_b_block_north_value"] = aie2::mem_event_broadcast_b_block_north_value;
   regNameToValue["mem_event_broadcast_b_block_south_clr"] = aie2::mem_event_broadcast_b_block_south_clr;
   regNameToValue["mem_event_broadcast_b_block_south_set"] = aie2::mem_event_broadcast_b_block_south_set;
   regNameToValue["mem_event_broadcast_b_block_south_value"] = aie2::mem_event_broadcast_b_block_south_value;
   regNameToValue["mem_event_broadcast_b_block_west_clr"] = aie2::mem_event_broadcast_b_block_west_clr;
   regNameToValue["mem_event_broadcast_b_block_west_set"] = aie2::mem_event_broadcast_b_block_west_set;
   regNameToValue["mem_event_broadcast_b_block_west_value"] = aie2::mem_event_broadcast_b_block_west_value;
   regNameToValue["mem_event_generate"] = aie2::mem_event_generate;
   regNameToValue["mem_event_group_0_enable"] = aie2::mem_event_group_0_enable;
   regNameToValue["mem_event_group_broadcast_enable"] = aie2::mem_event_group_broadcast_enable;
   regNameToValue["mem_event_group_dma_enable"] = aie2::mem_event_group_dma_enable;
   regNameToValue["mem_event_group_error_enable"] = aie2::mem_event_group_error_enable;
   regNameToValue["mem_event_group_lock_enable"] = aie2::mem_event_group_lock_enable;
   regNameToValue["mem_event_group_memory_conflict_enable"] = aie2::mem_event_group_memory_conflict_enable;
   regNameToValue["mem_event_group_stream_switch_enable"] = aie2::mem_event_group_stream_switch_enable;
   regNameToValue["mem_event_group_user_event_enable"] = aie2::mem_event_group_user_event_enable;
   regNameToValue["mem_event_group_watchpoint_enable"] = aie2::mem_event_group_watchpoint_enable;
   regNameToValue["mem_event_status0"] = aie2::mem_event_status0;
   regNameToValue["mem_event_status1"] = aie2::mem_event_status1;
   regNameToValue["mem_event_status2"] = aie2::mem_event_status2;
   regNameToValue["mem_event_status3"] = aie2::mem_event_status3;
   regNameToValue["mem_event_status4"] = aie2::mem_event_status4;
   regNameToValue["mem_event_status5"] = aie2::mem_event_status5;
   regNameToValue["mem_lock0_value"] = aie2::mem_lock0_value;
   regNameToValue["mem_lock10_value"] = aie2::mem_lock10_value;
   regNameToValue["mem_lock11_value"] = aie2::mem_lock11_value;
   regNameToValue["mem_lock12_value"] = aie2::mem_lock12_value;
   regNameToValue["mem_lock13_value"] = aie2::mem_lock13_value;
   regNameToValue["mem_lock14_value"] = aie2::mem_lock14_value;
   regNameToValue["mem_lock15_value"] = aie2::mem_lock15_value;
   regNameToValue["mem_lock16_value"] = aie2::mem_lock16_value;
   regNameToValue["mem_lock17_value"] = aie2::mem_lock17_value;
   regNameToValue["mem_lock18_value"] = aie2::mem_lock18_value;
   regNameToValue["mem_lock19_value"] = aie2::mem_lock19_value;
   regNameToValue["mem_lock1_value"] = aie2::mem_lock1_value;
   regNameToValue["mem_lock20_value"] = aie2::mem_lock20_value;
   regNameToValue["mem_lock21_value"] = aie2::mem_lock21_value;
   regNameToValue["mem_lock22_value"] = aie2::mem_lock22_value;
   regNameToValue["mem_lock23_value"] = aie2::mem_lock23_value;
   regNameToValue["mem_lock24_value"] = aie2::mem_lock24_value;
   regNameToValue["mem_lock25_value"] = aie2::mem_lock25_value;
   regNameToValue["mem_lock26_value"] = aie2::mem_lock26_value;
   regNameToValue["mem_lock27_value"] = aie2::mem_lock27_value;
   regNameToValue["mem_lock28_value"] = aie2::mem_lock28_value;
   regNameToValue["mem_lock29_value"] = aie2::mem_lock29_value;
   regNameToValue["mem_lock2_value"] = aie2::mem_lock2_value;
   regNameToValue["mem_lock30_value"] = aie2::mem_lock30_value;
   regNameToValue["mem_lock31_value"] = aie2::mem_lock31_value;
   regNameToValue["mem_lock32_value"] = aie2::mem_lock32_value;
   regNameToValue["mem_lock33_value"] = aie2::mem_lock33_value;
   regNameToValue["mem_lock34_value"] = aie2::mem_lock34_value;
   regNameToValue["mem_lock35_value"] = aie2::mem_lock35_value;
   regNameToValue["mem_lock36_value"] = aie2::mem_lock36_value;
   regNameToValue["mem_lock37_value"] = aie2::mem_lock37_value;
   regNameToValue["mem_lock38_value"] = aie2::mem_lock38_value;
   regNameToValue["mem_lock39_value"] = aie2::mem_lock39_value;
   regNameToValue["mem_lock3_value"] = aie2::mem_lock3_value;
   regNameToValue["mem_lock40_value"] = aie2::mem_lock40_value;
   regNameToValue["mem_lock41_value"] = aie2::mem_lock41_value;
   regNameToValue["mem_lock42_value"] = aie2::mem_lock42_value;
   regNameToValue["mem_lock43_value"] = aie2::mem_lock43_value;
   regNameToValue["mem_lock44_value"] = aie2::mem_lock44_value;
   regNameToValue["mem_lock45_value"] = aie2::mem_lock45_value;
   regNameToValue["mem_lock46_value"] = aie2::mem_lock46_value;
   regNameToValue["mem_lock47_value"] = aie2::mem_lock47_value;
   regNameToValue["mem_lock48_value"] = aie2::mem_lock48_value;
   regNameToValue["mem_lock49_value"] = aie2::mem_lock49_value;
   regNameToValue["mem_lock4_value"] = aie2::mem_lock4_value;
   regNameToValue["mem_lock50_value"] = aie2::mem_lock50_value;
   regNameToValue["mem_lock51_value"] = aie2::mem_lock51_value;
   regNameToValue["mem_lock52_value"] = aie2::mem_lock52_value;
   regNameToValue["mem_lock53_value"] = aie2::mem_lock53_value;
   regNameToValue["mem_lock54_value"] = aie2::mem_lock54_value;
   regNameToValue["mem_lock55_value"] = aie2::mem_lock55_value;
   regNameToValue["mem_lock56_value"] = aie2::mem_lock56_value;
   regNameToValue["mem_lock57_value"] = aie2::mem_lock57_value;
   regNameToValue["mem_lock58_value"] = aie2::mem_lock58_value;
   regNameToValue["mem_lock59_value"] = aie2::mem_lock59_value;
   regNameToValue["mem_lock5_value"] = aie2::mem_lock5_value;
   regNameToValue["mem_lock60_value"] = aie2::mem_lock60_value;
   regNameToValue["mem_lock61_value"] = aie2::mem_lock61_value;
   regNameToValue["mem_lock62_value"] = aie2::mem_lock62_value;
   regNameToValue["mem_lock63_value"] = aie2::mem_lock63_value;
   regNameToValue["mem_lock6_value"] = aie2::mem_lock6_value;
   regNameToValue["mem_lock7_value"] = aie2::mem_lock7_value;
   regNameToValue["mem_lock8_value"] = aie2::mem_lock8_value;
   regNameToValue["mem_lock9_value"] = aie2::mem_lock9_value;
   regNameToValue["mem_lock_request"] = aie2::mem_lock_request;
   regNameToValue["mem_locks_event_selection_0"] = aie2::mem_locks_event_selection_0;
   regNameToValue["mem_locks_event_selection_1"] = aie2::mem_locks_event_selection_1;
   regNameToValue["mem_locks_event_selection_2"] = aie2::mem_locks_event_selection_2;
   regNameToValue["mem_locks_event_selection_3"] = aie2::mem_locks_event_selection_3;
   regNameToValue["mem_locks_event_selection_4"] = aie2::mem_locks_event_selection_4;
   regNameToValue["mem_locks_event_selection_5"] = aie2::mem_locks_event_selection_5;
   regNameToValue["mem_locks_event_selection_6"] = aie2::mem_locks_event_selection_6;
   regNameToValue["mem_locks_event_selection_7"] = aie2::mem_locks_event_selection_7;
   regNameToValue["mem_locks_overflow_0"] = aie2::mem_locks_overflow_0;
   regNameToValue["mem_locks_overflow_1"] = aie2::mem_locks_overflow_1;
   regNameToValue["mem_locks_underflow_0"] = aie2::mem_locks_underflow_0;
   regNameToValue["mem_locks_underflow_1"] = aie2::mem_locks_underflow_1;
   regNameToValue["mem_memory_control"] = aie2::mem_memory_control;
   regNameToValue["mem_module_clock_control"] = aie2::mem_module_clock_control;
   regNameToValue["mem_module_reset_control"] = aie2::mem_module_reset_control;
   regNameToValue["mem_performance_control0"] = aie2::mem_performance_control0;
   regNameToValue["mem_performance_control1"] = aie2::mem_performance_control1;
   regNameToValue["mem_performance_control2"] = aie2::mem_performance_control2;
   regNameToValue["mem_performance_counter0"] = aie2::mem_performance_counter0;
   regNameToValue["mem_performance_counter0_event_value"] = aie2::mem_performance_counter0_event_value;
   regNameToValue["mem_performance_counter1"] = aie2::mem_performance_counter1;
   regNameToValue["mem_performance_counter1_event_value"] = aie2::mem_performance_counter1_event_value;
   regNameToValue["mem_performance_counter2"] = aie2::mem_performance_counter2;
   regNameToValue["mem_performance_counter2_event_value"] = aie2::mem_performance_counter2_event_value;
   regNameToValue["mem_performance_counter3"] = aie2::mem_performance_counter3;
   regNameToValue["mem_performance_counter3_event_value"] = aie2::mem_performance_counter3_event_value;
   regNameToValue["mem_reserved0"] = aie2::mem_reserved0;
   regNameToValue["mem_reserved1"] = aie2::mem_reserved1;
   regNameToValue["mem_reserved2"] = aie2::mem_reserved2;
   regNameToValue["mem_reserved3"] = aie2::mem_reserved3;
   regNameToValue["mem_spare_reg"] = aie2::mem_spare_reg;
   regNameToValue["mem_stream_switch_adaptive_clock_gate_abort_period"] = aie2::mem_stream_switch_adaptive_clock_gate_abort_period;
   regNameToValue["mem_stream_switch_adaptive_clock_gate_status"] = aie2::mem_stream_switch_adaptive_clock_gate_status;
   regNameToValue["mem_stream_switch_deterministic_merge_arb0_ctrl"] = aie2::mem_stream_switch_deterministic_merge_arb0_ctrl;
   regNameToValue["mem_stream_switch_deterministic_merge_arb0_slave0_1"] = aie2::mem_stream_switch_deterministic_merge_arb0_slave0_1;
   regNameToValue["mem_stream_switch_deterministic_merge_arb0_slave2_3"] = aie2::mem_stream_switch_deterministic_merge_arb0_slave2_3;
   regNameToValue["mem_stream_switch_deterministic_merge_arb1_ctrl"] = aie2::mem_stream_switch_deterministic_merge_arb1_ctrl;
   regNameToValue["mem_stream_switch_deterministic_merge_arb1_slave0_1"] = aie2::mem_stream_switch_deterministic_merge_arb1_slave0_1;
   regNameToValue["mem_stream_switch_deterministic_merge_arb1_slave2_3"] = aie2::mem_stream_switch_deterministic_merge_arb1_slave2_3;
   regNameToValue["mem_stream_switch_event_port_selection_0"] = aie2::mem_stream_switch_event_port_selection_0;
   regNameToValue["mem_stream_switch_event_port_selection_1"] = aie2::mem_stream_switch_event_port_selection_1;
   regNameToValue["mem_stream_switch_master_config_dma0"] = aie2::mem_stream_switch_master_config_dma0;
   regNameToValue["mem_stream_switch_master_config_dma1"] = aie2::mem_stream_switch_master_config_dma1;
   regNameToValue["mem_stream_switch_master_config_dma2"] = aie2::mem_stream_switch_master_config_dma2;
   regNameToValue["mem_stream_switch_master_config_dma3"] = aie2::mem_stream_switch_master_config_dma3;
   regNameToValue["mem_stream_switch_master_config_dma4"] = aie2::mem_stream_switch_master_config_dma4;
   regNameToValue["mem_stream_switch_master_config_dma5"] = aie2::mem_stream_switch_master_config_dma5;
   regNameToValue["mem_stream_switch_master_config_north0"] = aie2::mem_stream_switch_master_config_north0;
   regNameToValue["mem_stream_switch_master_config_north1"] = aie2::mem_stream_switch_master_config_north1;
   regNameToValue["mem_stream_switch_master_config_north2"] = aie2::mem_stream_switch_master_config_north2;
   regNameToValue["mem_stream_switch_master_config_north3"] = aie2::mem_stream_switch_master_config_north3;
   regNameToValue["mem_stream_switch_master_config_north4"] = aie2::mem_stream_switch_master_config_north4;
   regNameToValue["mem_stream_switch_master_config_north5"] = aie2::mem_stream_switch_master_config_north5;
   regNameToValue["mem_stream_switch_master_config_south0"] = aie2::mem_stream_switch_master_config_south0;
   regNameToValue["mem_stream_switch_master_config_south1"] = aie2::mem_stream_switch_master_config_south1;
   regNameToValue["mem_stream_switch_master_config_south2"] = aie2::mem_stream_switch_master_config_south2;
   regNameToValue["mem_stream_switch_master_config_south3"] = aie2::mem_stream_switch_master_config_south3;
   regNameToValue["mem_stream_switch_master_config_tile_ctrl"] = aie2::mem_stream_switch_master_config_tile_ctrl;
   regNameToValue["mem_stream_switch_parity_injection"] = aie2::mem_stream_switch_parity_injection;
   regNameToValue["mem_stream_switch_parity_status"] = aie2::mem_stream_switch_parity_status;
   regNameToValue["mem_stream_switch_slave_config_dma_0"] = aie2::mem_stream_switch_slave_config_dma_0;
   regNameToValue["mem_stream_switch_slave_config_dma_1"] = aie2::mem_stream_switch_slave_config_dma_1;
   regNameToValue["mem_stream_switch_slave_config_dma_2"] = aie2::mem_stream_switch_slave_config_dma_2;
   regNameToValue["mem_stream_switch_slave_config_dma_3"] = aie2::mem_stream_switch_slave_config_dma_3;
   regNameToValue["mem_stream_switch_slave_config_dma_4"] = aie2::mem_stream_switch_slave_config_dma_4;
   regNameToValue["mem_stream_switch_slave_config_dma_5"] = aie2::mem_stream_switch_slave_config_dma_5;
   regNameToValue["mem_stream_switch_slave_config_north_0"] = aie2::mem_stream_switch_slave_config_north_0;
   regNameToValue["mem_stream_switch_slave_config_north_1"] = aie2::mem_stream_switch_slave_config_north_1;
   regNameToValue["mem_stream_switch_slave_config_north_2"] = aie2::mem_stream_switch_slave_config_north_2;
   regNameToValue["mem_stream_switch_slave_config_north_3"] = aie2::mem_stream_switch_slave_config_north_3;
   regNameToValue["mem_stream_switch_slave_config_south_0"] = aie2::mem_stream_switch_slave_config_south_0;
   regNameToValue["mem_stream_switch_slave_config_south_1"] = aie2::mem_stream_switch_slave_config_south_1;
   regNameToValue["mem_stream_switch_slave_config_south_2"] = aie2::mem_stream_switch_slave_config_south_2;
   regNameToValue["mem_stream_switch_slave_config_south_3"] = aie2::mem_stream_switch_slave_config_south_3;
   regNameToValue["mem_stream_switch_slave_config_south_4"] = aie2::mem_stream_switch_slave_config_south_4;
   regNameToValue["mem_stream_switch_slave_config_south_5"] = aie2::mem_stream_switch_slave_config_south_5;
   regNameToValue["mem_stream_switch_slave_config_tile_ctrl"] = aie2::mem_stream_switch_slave_config_tile_ctrl;
   regNameToValue["mem_stream_switch_slave_config_trace"] = aie2::mem_stream_switch_slave_config_trace;
   regNameToValue["mem_stream_switch_slave_dma_0_slot0"] = aie2::mem_stream_switch_slave_dma_0_slot0;
   regNameToValue["mem_stream_switch_slave_dma_0_slot1"] = aie2::mem_stream_switch_slave_dma_0_slot1;
   regNameToValue["mem_stream_switch_slave_dma_0_slot2"] = aie2::mem_stream_switch_slave_dma_0_slot2;
   regNameToValue["mem_stream_switch_slave_dma_0_slot3"] = aie2::mem_stream_switch_slave_dma_0_slot3;
   regNameToValue["mem_stream_switch_slave_dma_1_slot0"] = aie2::mem_stream_switch_slave_dma_1_slot0;
   regNameToValue["mem_stream_switch_slave_dma_1_slot1"] = aie2::mem_stream_switch_slave_dma_1_slot1;
   regNameToValue["mem_stream_switch_slave_dma_1_slot2"] = aie2::mem_stream_switch_slave_dma_1_slot2;
   regNameToValue["mem_stream_switch_slave_dma_1_slot3"] = aie2::mem_stream_switch_slave_dma_1_slot3;
   regNameToValue["mem_stream_switch_slave_dma_2_slot0"] = aie2::mem_stream_switch_slave_dma_2_slot0;
   regNameToValue["mem_stream_switch_slave_dma_2_slot1"] = aie2::mem_stream_switch_slave_dma_2_slot1;
   regNameToValue["mem_stream_switch_slave_dma_2_slot2"] = aie2::mem_stream_switch_slave_dma_2_slot2;
   regNameToValue["mem_stream_switch_slave_dma_2_slot3"] = aie2::mem_stream_switch_slave_dma_2_slot3;
   regNameToValue["mem_stream_switch_slave_dma_3_slot0"] = aie2::mem_stream_switch_slave_dma_3_slot0;
   regNameToValue["mem_stream_switch_slave_dma_3_slot1"] = aie2::mem_stream_switch_slave_dma_3_slot1;
   regNameToValue["mem_stream_switch_slave_dma_3_slot2"] = aie2::mem_stream_switch_slave_dma_3_slot2;
   regNameToValue["mem_stream_switch_slave_dma_3_slot3"] = aie2::mem_stream_switch_slave_dma_3_slot3;
   regNameToValue["mem_stream_switch_slave_dma_4_slot0"] = aie2::mem_stream_switch_slave_dma_4_slot0;
   regNameToValue["mem_stream_switch_slave_dma_4_slot1"] = aie2::mem_stream_switch_slave_dma_4_slot1;
   regNameToValue["mem_stream_switch_slave_dma_4_slot2"] = aie2::mem_stream_switch_slave_dma_4_slot2;
   regNameToValue["mem_stream_switch_slave_dma_4_slot3"] = aie2::mem_stream_switch_slave_dma_4_slot3;
   regNameToValue["mem_stream_switch_slave_dma_5_slot0"] = aie2::mem_stream_switch_slave_dma_5_slot0;
   regNameToValue["mem_stream_switch_slave_dma_5_slot1"] = aie2::mem_stream_switch_slave_dma_5_slot1;
   regNameToValue["mem_stream_switch_slave_dma_5_slot2"] = aie2::mem_stream_switch_slave_dma_5_slot2;
   regNameToValue["mem_stream_switch_slave_dma_5_slot3"] = aie2::mem_stream_switch_slave_dma_5_slot3;
   regNameToValue["mem_stream_switch_slave_north_0_slot0"] = aie2::mem_stream_switch_slave_north_0_slot0;
   regNameToValue["mem_stream_switch_slave_north_0_slot1"] = aie2::mem_stream_switch_slave_north_0_slot1;
   regNameToValue["mem_stream_switch_slave_north_0_slot2"] = aie2::mem_stream_switch_slave_north_0_slot2;
   regNameToValue["mem_stream_switch_slave_north_0_slot3"] = aie2::mem_stream_switch_slave_north_0_slot3;
   regNameToValue["mem_stream_switch_slave_north_1_slot0"] = aie2::mem_stream_switch_slave_north_1_slot0;
   regNameToValue["mem_stream_switch_slave_north_1_slot1"] = aie2::mem_stream_switch_slave_north_1_slot1;
   regNameToValue["mem_stream_switch_slave_north_1_slot2"] = aie2::mem_stream_switch_slave_north_1_slot2;
   regNameToValue["mem_stream_switch_slave_north_1_slot3"] = aie2::mem_stream_switch_slave_north_1_slot3;
   regNameToValue["mem_stream_switch_slave_north_2_slot0"] = aie2::mem_stream_switch_slave_north_2_slot0;
   regNameToValue["mem_stream_switch_slave_north_2_slot1"] = aie2::mem_stream_switch_slave_north_2_slot1;
   regNameToValue["mem_stream_switch_slave_north_2_slot2"] = aie2::mem_stream_switch_slave_north_2_slot2;
   regNameToValue["mem_stream_switch_slave_north_2_slot3"] = aie2::mem_stream_switch_slave_north_2_slot3;
   regNameToValue["mem_stream_switch_slave_north_3_slot0"] = aie2::mem_stream_switch_slave_north_3_slot0;
   regNameToValue["mem_stream_switch_slave_north_3_slot1"] = aie2::mem_stream_switch_slave_north_3_slot1;
   regNameToValue["mem_stream_switch_slave_north_3_slot2"] = aie2::mem_stream_switch_slave_north_3_slot2;
   regNameToValue["mem_stream_switch_slave_north_3_slot3"] = aie2::mem_stream_switch_slave_north_3_slot3;
   regNameToValue["mem_stream_switch_slave_south_0_slot0"] = aie2::mem_stream_switch_slave_south_0_slot0;
   regNameToValue["mem_stream_switch_slave_south_0_slot1"] = aie2::mem_stream_switch_slave_south_0_slot1;
   regNameToValue["mem_stream_switch_slave_south_0_slot2"] = aie2::mem_stream_switch_slave_south_0_slot2;
   regNameToValue["mem_stream_switch_slave_south_0_slot3"] = aie2::mem_stream_switch_slave_south_0_slot3;
   regNameToValue["mem_stream_switch_slave_south_1_slot0"] = aie2::mem_stream_switch_slave_south_1_slot0;
   regNameToValue["mem_stream_switch_slave_south_1_slot1"] = aie2::mem_stream_switch_slave_south_1_slot1;
   regNameToValue["mem_stream_switch_slave_south_1_slot2"] = aie2::mem_stream_switch_slave_south_1_slot2;
   regNameToValue["mem_stream_switch_slave_south_1_slot3"] = aie2::mem_stream_switch_slave_south_1_slot3;
   regNameToValue["mem_stream_switch_slave_south_2_slot0"] = aie2::mem_stream_switch_slave_south_2_slot0;
   regNameToValue["mem_stream_switch_slave_south_2_slot1"] = aie2::mem_stream_switch_slave_south_2_slot1;
   regNameToValue["mem_stream_switch_slave_south_2_slot2"] = aie2::mem_stream_switch_slave_south_2_slot2;
   regNameToValue["mem_stream_switch_slave_south_2_slot3"] = aie2::mem_stream_switch_slave_south_2_slot3;
   regNameToValue["mem_stream_switch_slave_south_3_slot0"] = aie2::mem_stream_switch_slave_south_3_slot0;
   regNameToValue["mem_stream_switch_slave_south_3_slot1"] = aie2::mem_stream_switch_slave_south_3_slot1;
   regNameToValue["mem_stream_switch_slave_south_3_slot2"] = aie2::mem_stream_switch_slave_south_3_slot2;
   regNameToValue["mem_stream_switch_slave_south_3_slot3"] = aie2::mem_stream_switch_slave_south_3_slot3;
   regNameToValue["mem_stream_switch_slave_south_4_slot0"] = aie2::mem_stream_switch_slave_south_4_slot0;
   regNameToValue["mem_stream_switch_slave_south_4_slot1"] = aie2::mem_stream_switch_slave_south_4_slot1;
   regNameToValue["mem_stream_switch_slave_south_4_slot2"] = aie2::mem_stream_switch_slave_south_4_slot2;
   regNameToValue["mem_stream_switch_slave_south_4_slot3"] = aie2::mem_stream_switch_slave_south_4_slot3;
   regNameToValue["mem_stream_switch_slave_south_5_slot0"] = aie2::mem_stream_switch_slave_south_5_slot0;
   regNameToValue["mem_stream_switch_slave_south_5_slot1"] = aie2::mem_stream_switch_slave_south_5_slot1;
   regNameToValue["mem_stream_switch_slave_south_5_slot2"] = aie2::mem_stream_switch_slave_south_5_slot2;
   regNameToValue["mem_stream_switch_slave_south_5_slot3"] = aie2::mem_stream_switch_slave_south_5_slot3;
   regNameToValue["mem_stream_switch_slave_tile_ctrl_slot0"] = aie2::mem_stream_switch_slave_tile_ctrl_slot0;
   regNameToValue["mem_stream_switch_slave_tile_ctrl_slot1"] = aie2::mem_stream_switch_slave_tile_ctrl_slot1;
   regNameToValue["mem_stream_switch_slave_tile_ctrl_slot2"] = aie2::mem_stream_switch_slave_tile_ctrl_slot2;
   regNameToValue["mem_stream_switch_slave_tile_ctrl_slot3"] = aie2::mem_stream_switch_slave_tile_ctrl_slot3;
   regNameToValue["mem_stream_switch_slave_trace_slot0"] = aie2::mem_stream_switch_slave_trace_slot0;
   regNameToValue["mem_stream_switch_slave_trace_slot1"] = aie2::mem_stream_switch_slave_trace_slot1;
   regNameToValue["mem_stream_switch_slave_trace_slot2"] = aie2::mem_stream_switch_slave_trace_slot2;
   regNameToValue["mem_stream_switch_slave_trace_slot3"] = aie2::mem_stream_switch_slave_trace_slot3;
   regNameToValue["mem_tile_control"] = aie2::mem_tile_control;
   regNameToValue["mem_tile_control_packet_handler_status"] = aie2::mem_tile_control_packet_handler_status;
   regNameToValue["mem_timer_control"] = aie2::mem_timer_control;
   regNameToValue["mem_timer_high"] = aie2::mem_timer_high;
   regNameToValue["mem_timer_low"] = aie2::mem_timer_low;
   regNameToValue["mem_timer_trig_event_high_value"] = aie2::mem_timer_trig_event_high_value;
   regNameToValue["mem_timer_trig_event_low_value"] = aie2::mem_timer_trig_event_low_value;
   regNameToValue["mem_trace_control0"] = aie2::mem_trace_control0;
   regNameToValue["mem_trace_control1"] = aie2::mem_trace_control1;
   regNameToValue["mem_trace_event0"] = aie2::mem_trace_event0;
   regNameToValue["mem_trace_event1"] = aie2::mem_trace_event1;
   regNameToValue["mem_trace_status"] = aie2::mem_trace_status;
   regNameToValue["mem_watchpoint0"] = aie2::mem_watchpoint0;
   regNameToValue["mem_watchpoint1"] = aie2::mem_watchpoint1;
   regNameToValue["mem_watchpoint2"] = aie2::mem_watchpoint2;
   regNameToValue["mem_watchpoint3"] = aie2::mem_watchpoint3;
   regNameToValue["shim_bisr_cache_ctrl"] = aie2::shim_bisr_cache_ctrl;
   regNameToValue["shim_bisr_cache_data0"] = aie2::shim_bisr_cache_data0;
   regNameToValue["shim_bisr_cache_data1"] = aie2::shim_bisr_cache_data1;
   regNameToValue["shim_bisr_cache_data2"] = aie2::shim_bisr_cache_data2;
   regNameToValue["shim_bisr_cache_data3"] = aie2::shim_bisr_cache_data3;
   regNameToValue["shim_bisr_cache_status"] = aie2::shim_bisr_cache_status;
   regNameToValue["shim_bisr_test_data0"] = aie2::shim_bisr_test_data0;
   regNameToValue["shim_bisr_test_data1"] = aie2::shim_bisr_test_data1;
   regNameToValue["shim_bisr_test_data2"] = aie2::shim_bisr_test_data2;
   regNameToValue["shim_bisr_test_data3"] = aie2::shim_bisr_test_data3;
   regNameToValue["shim_cssd_trigger"] = aie2::shim_cssd_trigger;
   regNameToValue["shim_column_clock_control"] = aie2::shim_column_clock_control;
   regNameToValue["shim_column_reset_control"] = aie2::shim_column_reset_control;
   regNameToValue["shim_combo_event_control"] = aie2::shim_combo_event_control;
   regNameToValue["shim_combo_event_inputs"] = aie2::shim_combo_event_inputs;
   regNameToValue["shim_control_packet_handler_status"] = aie2::shim_control_packet_handler_status;
   regNameToValue["shim_dma_bd0_0"] = aie2::shim_dma_bd0_0;
   regNameToValue["shim_dma_bd0_1"] = aie2::shim_dma_bd0_1;
   regNameToValue["shim_dma_bd0_2"] = aie2::shim_dma_bd0_2;
   regNameToValue["shim_dma_bd0_3"] = aie2::shim_dma_bd0_3;
   regNameToValue["shim_dma_bd0_4"] = aie2::shim_dma_bd0_4;
   regNameToValue["shim_dma_bd0_5"] = aie2::shim_dma_bd0_5;
   regNameToValue["shim_dma_bd0_6"] = aie2::shim_dma_bd0_6;
   regNameToValue["shim_dma_bd0_7"] = aie2::shim_dma_bd0_7;
   regNameToValue["shim_dma_bd10_0"] = aie2::shim_dma_bd10_0;
   regNameToValue["shim_dma_bd10_1"] = aie2::shim_dma_bd10_1;
   regNameToValue["shim_dma_bd10_2"] = aie2::shim_dma_bd10_2;
   regNameToValue["shim_dma_bd10_3"] = aie2::shim_dma_bd10_3;
   regNameToValue["shim_dma_bd10_4"] = aie2::shim_dma_bd10_4;
   regNameToValue["shim_dma_bd10_5"] = aie2::shim_dma_bd10_5;
   regNameToValue["shim_dma_bd10_6"] = aie2::shim_dma_bd10_6;
   regNameToValue["shim_dma_bd10_7"] = aie2::shim_dma_bd10_7;
   regNameToValue["shim_dma_bd11_0"] = aie2::shim_dma_bd11_0;
   regNameToValue["shim_dma_bd11_1"] = aie2::shim_dma_bd11_1;
   regNameToValue["shim_dma_bd11_2"] = aie2::shim_dma_bd11_2;
   regNameToValue["shim_dma_bd11_3"] = aie2::shim_dma_bd11_3;
   regNameToValue["shim_dma_bd11_4"] = aie2::shim_dma_bd11_4;
   regNameToValue["shim_dma_bd11_5"] = aie2::shim_dma_bd11_5;
   regNameToValue["shim_dma_bd11_6"] = aie2::shim_dma_bd11_6;
   regNameToValue["shim_dma_bd11_7"] = aie2::shim_dma_bd11_7;
   regNameToValue["shim_dma_bd12_0"] = aie2::shim_dma_bd12_0;
   regNameToValue["shim_dma_bd12_1"] = aie2::shim_dma_bd12_1;
   regNameToValue["shim_dma_bd12_2"] = aie2::shim_dma_bd12_2;
   regNameToValue["shim_dma_bd12_3"] = aie2::shim_dma_bd12_3;
   regNameToValue["shim_dma_bd12_4"] = aie2::shim_dma_bd12_4;
   regNameToValue["shim_dma_bd12_5"] = aie2::shim_dma_bd12_5;
   regNameToValue["shim_dma_bd12_6"] = aie2::shim_dma_bd12_6;
   regNameToValue["shim_dma_bd12_7"] = aie2::shim_dma_bd12_7;
   regNameToValue["shim_dma_bd13_0"] = aie2::shim_dma_bd13_0;
   regNameToValue["shim_dma_bd13_1"] = aie2::shim_dma_bd13_1;
   regNameToValue["shim_dma_bd13_2"] = aie2::shim_dma_bd13_2;
   regNameToValue["shim_dma_bd13_3"] = aie2::shim_dma_bd13_3;
   regNameToValue["shim_dma_bd13_4"] = aie2::shim_dma_bd13_4;
   regNameToValue["shim_dma_bd13_5"] = aie2::shim_dma_bd13_5;
   regNameToValue["shim_dma_bd13_6"] = aie2::shim_dma_bd13_6;
   regNameToValue["shim_dma_bd13_7"] = aie2::shim_dma_bd13_7;
   regNameToValue["shim_dma_bd14_0"] = aie2::shim_dma_bd14_0;
   regNameToValue["shim_dma_bd14_1"] = aie2::shim_dma_bd14_1;
   regNameToValue["shim_dma_bd14_2"] = aie2::shim_dma_bd14_2;
   regNameToValue["shim_dma_bd14_3"] = aie2::shim_dma_bd14_3;
   regNameToValue["shim_dma_bd14_4"] = aie2::shim_dma_bd14_4;
   regNameToValue["shim_dma_bd14_5"] = aie2::shim_dma_bd14_5;
   regNameToValue["shim_dma_bd14_6"] = aie2::shim_dma_bd14_6;
   regNameToValue["shim_dma_bd14_7"] = aie2::shim_dma_bd14_7;
   regNameToValue["shim_dma_bd15_0"] = aie2::shim_dma_bd15_0;
   regNameToValue["shim_dma_bd15_1"] = aie2::shim_dma_bd15_1;
   regNameToValue["shim_dma_bd15_2"] = aie2::shim_dma_bd15_2;
   regNameToValue["shim_dma_bd15_3"] = aie2::shim_dma_bd15_3;
   regNameToValue["shim_dma_bd15_4"] = aie2::shim_dma_bd15_4;
   regNameToValue["shim_dma_bd15_5"] = aie2::shim_dma_bd15_5;
   regNameToValue["shim_dma_bd15_6"] = aie2::shim_dma_bd15_6;
   regNameToValue["shim_dma_bd15_7"] = aie2::shim_dma_bd15_7;
   regNameToValue["shim_dma_bd1_0"] = aie2::shim_dma_bd1_0;
   regNameToValue["shim_dma_bd1_1"] = aie2::shim_dma_bd1_1;
   regNameToValue["shim_dma_bd1_2"] = aie2::shim_dma_bd1_2;
   regNameToValue["shim_dma_bd1_3"] = aie2::shim_dma_bd1_3;
   regNameToValue["shim_dma_bd1_4"] = aie2::shim_dma_bd1_4;
   regNameToValue["shim_dma_bd1_5"] = aie2::shim_dma_bd1_5;
   regNameToValue["shim_dma_bd1_6"] = aie2::shim_dma_bd1_6;
   regNameToValue["shim_dma_bd1_7"] = aie2::shim_dma_bd1_7;
   regNameToValue["shim_dma_bd2_0"] = aie2::shim_dma_bd2_0;
   regNameToValue["shim_dma_bd2_1"] = aie2::shim_dma_bd2_1;
   regNameToValue["shim_dma_bd2_2"] = aie2::shim_dma_bd2_2;
   regNameToValue["shim_dma_bd2_3"] = aie2::shim_dma_bd2_3;
   regNameToValue["shim_dma_bd2_4"] = aie2::shim_dma_bd2_4;
   regNameToValue["shim_dma_bd2_5"] = aie2::shim_dma_bd2_5;
   regNameToValue["shim_dma_bd2_6"] = aie2::shim_dma_bd2_6;
   regNameToValue["shim_dma_bd2_7"] = aie2::shim_dma_bd2_7;
   regNameToValue["shim_dma_bd3_0"] = aie2::shim_dma_bd3_0;
   regNameToValue["shim_dma_bd3_1"] = aie2::shim_dma_bd3_1;
   regNameToValue["shim_dma_bd3_2"] = aie2::shim_dma_bd3_2;
   regNameToValue["shim_dma_bd3_3"] = aie2::shim_dma_bd3_3;
   regNameToValue["shim_dma_bd3_4"] = aie2::shim_dma_bd3_4;
   regNameToValue["shim_dma_bd3_5"] = aie2::shim_dma_bd3_5;
   regNameToValue["shim_dma_bd3_6"] = aie2::shim_dma_bd3_6;
   regNameToValue["shim_dma_bd3_7"] = aie2::shim_dma_bd3_7;
   regNameToValue["shim_dma_bd4_0"] = aie2::shim_dma_bd4_0;
   regNameToValue["shim_dma_bd4_1"] = aie2::shim_dma_bd4_1;
   regNameToValue["shim_dma_bd4_2"] = aie2::shim_dma_bd4_2;
   regNameToValue["shim_dma_bd4_3"] = aie2::shim_dma_bd4_3;
   regNameToValue["shim_dma_bd4_4"] = aie2::shim_dma_bd4_4;
   regNameToValue["shim_dma_bd4_5"] = aie2::shim_dma_bd4_5;
   regNameToValue["shim_dma_bd4_6"] = aie2::shim_dma_bd4_6;
   regNameToValue["shim_dma_bd4_7"] = aie2::shim_dma_bd4_7;
   regNameToValue["shim_dma_bd5_0"] = aie2::shim_dma_bd5_0;
   regNameToValue["shim_dma_bd5_1"] = aie2::shim_dma_bd5_1;
   regNameToValue["shim_dma_bd5_2"] = aie2::shim_dma_bd5_2;
   regNameToValue["shim_dma_bd5_3"] = aie2::shim_dma_bd5_3;
   regNameToValue["shim_dma_bd5_4"] = aie2::shim_dma_bd5_4;
   regNameToValue["shim_dma_bd5_5"] = aie2::shim_dma_bd5_5;
   regNameToValue["shim_dma_bd5_6"] = aie2::shim_dma_bd5_6;
   regNameToValue["shim_dma_bd5_7"] = aie2::shim_dma_bd5_7;
   regNameToValue["shim_dma_bd6_0"] = aie2::shim_dma_bd6_0;
   regNameToValue["shim_dma_bd6_1"] = aie2::shim_dma_bd6_1;
   regNameToValue["shim_dma_bd6_2"] = aie2::shim_dma_bd6_2;
   regNameToValue["shim_dma_bd6_3"] = aie2::shim_dma_bd6_3;
   regNameToValue["shim_dma_bd6_4"] = aie2::shim_dma_bd6_4;
   regNameToValue["shim_dma_bd6_5"] = aie2::shim_dma_bd6_5;
   regNameToValue["shim_dma_bd6_6"] = aie2::shim_dma_bd6_6;
   regNameToValue["shim_dma_bd6_7"] = aie2::shim_dma_bd6_7;
   regNameToValue["shim_dma_bd7_0"] = aie2::shim_dma_bd7_0;
   regNameToValue["shim_dma_bd7_1"] = aie2::shim_dma_bd7_1;
   regNameToValue["shim_dma_bd7_2"] = aie2::shim_dma_bd7_2;
   regNameToValue["shim_dma_bd7_3"] = aie2::shim_dma_bd7_3;
   regNameToValue["shim_dma_bd7_4"] = aie2::shim_dma_bd7_4;
   regNameToValue["shim_dma_bd7_5"] = aie2::shim_dma_bd7_5;
   regNameToValue["shim_dma_bd7_6"] = aie2::shim_dma_bd7_6;
   regNameToValue["shim_dma_bd7_7"] = aie2::shim_dma_bd7_7;
   regNameToValue["shim_dma_bd8_0"] = aie2::shim_dma_bd8_0;
   regNameToValue["shim_dma_bd8_1"] = aie2::shim_dma_bd8_1;
   regNameToValue["shim_dma_bd8_2"] = aie2::shim_dma_bd8_2;
   regNameToValue["shim_dma_bd8_3"] = aie2::shim_dma_bd8_3;
   regNameToValue["shim_dma_bd8_4"] = aie2::shim_dma_bd8_4;
   regNameToValue["shim_dma_bd8_5"] = aie2::shim_dma_bd8_5;
   regNameToValue["shim_dma_bd8_6"] = aie2::shim_dma_bd8_6;
   regNameToValue["shim_dma_bd8_7"] = aie2::shim_dma_bd8_7;
   regNameToValue["shim_dma_bd9_0"] = aie2::shim_dma_bd9_0;
   regNameToValue["shim_dma_bd9_1"] = aie2::shim_dma_bd9_1;
   regNameToValue["shim_dma_bd9_2"] = aie2::shim_dma_bd9_2;
   regNameToValue["shim_dma_bd9_3"] = aie2::shim_dma_bd9_3;
   regNameToValue["shim_dma_bd9_4"] = aie2::shim_dma_bd9_4;
   regNameToValue["shim_dma_bd9_5"] = aie2::shim_dma_bd9_5;
   regNameToValue["shim_dma_bd9_6"] = aie2::shim_dma_bd9_6;
   regNameToValue["shim_dma_bd9_7"] = aie2::shim_dma_bd9_7;
   regNameToValue["shim_dma_mm2s_0_ctrl"] = aie2::shim_dma_mm2s_0_ctrl;
   regNameToValue["shim_dma_mm2s_0_task_queue"] = aie2::shim_dma_mm2s_0_task_queue;
   regNameToValue["shim_dma_mm2s_1_ctrl"] = aie2::shim_dma_mm2s_1_ctrl;
   regNameToValue["shim_dma_mm2s_1_task_queue"] = aie2::shim_dma_mm2s_1_task_queue;
   regNameToValue["shim_dma_mm2s_status_0"] = aie2::shim_dma_mm2s_status_0;
   regNameToValue["shim_dma_mm2s_status_1"] = aie2::shim_dma_mm2s_status_1;
   regNameToValue["shim_dma_s2mm_0_ctrl"] = aie2::shim_dma_s2mm_0_ctrl;
   regNameToValue["shim_dma_s2mm_0_task_queue"] = aie2::shim_dma_s2mm_0_task_queue;
   regNameToValue["shim_dma_s2mm_1_ctrl"] = aie2::shim_dma_s2mm_1_ctrl;
   regNameToValue["shim_dma_s2mm_1_task_queue"] = aie2::shim_dma_s2mm_1_task_queue;
   regNameToValue["shim_dma_s2mm_current_write_count_0"] = aie2::shim_dma_s2mm_current_write_count_0;
   regNameToValue["shim_dma_s2mm_current_write_count_1"] = aie2::shim_dma_s2mm_current_write_count_1;
   regNameToValue["shim_dma_s2mm_fot_count_fifo_pop_0"] = aie2::shim_dma_s2mm_fot_count_fifo_pop_0;
   regNameToValue["shim_dma_s2mm_fot_count_fifo_pop_1"] = aie2::shim_dma_s2mm_fot_count_fifo_pop_1;
   regNameToValue["shim_dma_s2mm_status_0"] = aie2::shim_dma_s2mm_status_0;
   regNameToValue["shim_dma_s2mm_status_1"] = aie2::shim_dma_s2mm_status_1;
   regNameToValue["shim_demux_config"] = aie2::shim_demux_config;
   regNameToValue["shim_edge_detection_event_control"] = aie2::shim_edge_detection_event_control;
   regNameToValue["shim_event_broadcast_a_0"] = aie2::shim_event_broadcast_a_0;
   regNameToValue["shim_event_broadcast_a_10"] = aie2::shim_event_broadcast_a_10;
   regNameToValue["shim_event_broadcast_a_11"] = aie2::shim_event_broadcast_a_11;
   regNameToValue["shim_event_broadcast_a_12"] = aie2::shim_event_broadcast_a_12;
   regNameToValue["shim_event_broadcast_a_13"] = aie2::shim_event_broadcast_a_13;
   regNameToValue["shim_event_broadcast_a_14"] = aie2::shim_event_broadcast_a_14;
   regNameToValue["shim_event_broadcast_a_15"] = aie2::shim_event_broadcast_a_15;
   regNameToValue["shim_event_broadcast_a_1"] = aie2::shim_event_broadcast_a_1;
   regNameToValue["shim_event_broadcast_a_2"] = aie2::shim_event_broadcast_a_2;
   regNameToValue["shim_event_broadcast_a_3"] = aie2::shim_event_broadcast_a_3;
   regNameToValue["shim_event_broadcast_a_4"] = aie2::shim_event_broadcast_a_4;
   regNameToValue["shim_event_broadcast_a_5"] = aie2::shim_event_broadcast_a_5;
   regNameToValue["shim_event_broadcast_a_6"] = aie2::shim_event_broadcast_a_6;
   regNameToValue["shim_event_broadcast_a_7"] = aie2::shim_event_broadcast_a_7;
   regNameToValue["shim_event_broadcast_a_8"] = aie2::shim_event_broadcast_a_8;
   regNameToValue["shim_event_broadcast_a_9"] = aie2::shim_event_broadcast_a_9;
   regNameToValue["shim_event_broadcast_a_block_east_clr"] = aie2::shim_event_broadcast_a_block_east_clr;
   regNameToValue["shim_event_broadcast_a_block_east_set"] = aie2::shim_event_broadcast_a_block_east_set;
   regNameToValue["shim_event_broadcast_a_block_east_value"] = aie2::shim_event_broadcast_a_block_east_value;
   regNameToValue["shim_event_broadcast_a_block_north_clr"] = aie2::shim_event_broadcast_a_block_north_clr;
   regNameToValue["shim_event_broadcast_a_block_north_set"] = aie2::shim_event_broadcast_a_block_north_set;
   regNameToValue["shim_event_broadcast_a_block_north_value"] = aie2::shim_event_broadcast_a_block_north_value;
   regNameToValue["shim_event_broadcast_a_block_south_clr"] = aie2::shim_event_broadcast_a_block_south_clr;
   regNameToValue["shim_event_broadcast_a_block_south_set"] = aie2::shim_event_broadcast_a_block_south_set;
   regNameToValue["shim_event_broadcast_a_block_south_value"] = aie2::shim_event_broadcast_a_block_south_value;
   regNameToValue["shim_event_broadcast_a_block_west_clr"] = aie2::shim_event_broadcast_a_block_west_clr;
   regNameToValue["shim_event_broadcast_a_block_west_set"] = aie2::shim_event_broadcast_a_block_west_set;
   regNameToValue["shim_event_broadcast_a_block_west_value"] = aie2::shim_event_broadcast_a_block_west_value;
   regNameToValue["shim_event_broadcast_b_block_east_clr"] = aie2::shim_event_broadcast_b_block_east_clr;
   regNameToValue["shim_event_broadcast_b_block_east_set"] = aie2::shim_event_broadcast_b_block_east_set;
   regNameToValue["shim_event_broadcast_b_block_east_value"] = aie2::shim_event_broadcast_b_block_east_value;
   regNameToValue["shim_event_broadcast_b_block_north_clr"] = aie2::shim_event_broadcast_b_block_north_clr;
   regNameToValue["shim_event_broadcast_b_block_north_set"] = aie2::shim_event_broadcast_b_block_north_set;
   regNameToValue["shim_event_broadcast_b_block_north_value"] = aie2::shim_event_broadcast_b_block_north_value;
   regNameToValue["shim_event_broadcast_b_block_south_clr"] = aie2::shim_event_broadcast_b_block_south_clr;
   regNameToValue["shim_event_broadcast_b_block_south_set"] = aie2::shim_event_broadcast_b_block_south_set;
   regNameToValue["shim_event_broadcast_b_block_south_value"] = aie2::shim_event_broadcast_b_block_south_value;
   regNameToValue["shim_event_broadcast_b_block_west_clr"] = aie2::shim_event_broadcast_b_block_west_clr;
   regNameToValue["shim_event_broadcast_b_block_west_set"] = aie2::shim_event_broadcast_b_block_west_set;
   regNameToValue["shim_event_broadcast_b_block_west_value"] = aie2::shim_event_broadcast_b_block_west_value;
   regNameToValue["shim_event_generate"] = aie2::shim_event_generate;
   regNameToValue["shim_event_group_0_enable"] = aie2::shim_event_group_0_enable;
   regNameToValue["shim_event_group_broadcast_a_enable"] = aie2::shim_event_group_broadcast_a_enable;
   regNameToValue["shim_event_group_dma_enable"] = aie2::shim_event_group_dma_enable;
   regNameToValue["shim_event_group_errors_enable"] = aie2::shim_event_group_errors_enable;
   regNameToValue["shim_event_group_lock_enable"] = aie2::shim_event_group_lock_enable;
   regNameToValue["shim_event_group_stream_switch_enable"] = aie2::shim_event_group_stream_switch_enable;
   regNameToValue["shim_event_status0"] = aie2::shim_event_status0;
   regNameToValue["shim_event_status1"] = aie2::shim_event_status1;
   regNameToValue["shim_event_status2"] = aie2::shim_event_status2;
   regNameToValue["shim_event_status3"] = aie2::shim_event_status3;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_a_clear"] = aie2::shim_interrupt_controller_1st_level_block_north_in_a_clear;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_a_set"] = aie2::shim_interrupt_controller_1st_level_block_north_in_a_set;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_a_value"] = aie2::shim_interrupt_controller_1st_level_block_north_in_a_value;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_b_clear"] = aie2::shim_interrupt_controller_1st_level_block_north_in_b_clear;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_b_set"] = aie2::shim_interrupt_controller_1st_level_block_north_in_b_set;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_b_value"] = aie2::shim_interrupt_controller_1st_level_block_north_in_b_value;
   regNameToValue["shim_interrupt_controller_1st_level_disable_a"] = aie2::shim_interrupt_controller_1st_level_disable_a;
   regNameToValue["shim_interrupt_controller_1st_level_disable_b"] = aie2::shim_interrupt_controller_1st_level_disable_b;
   regNameToValue["shim_interrupt_controller_1st_level_enable_a"] = aie2::shim_interrupt_controller_1st_level_enable_a;
   regNameToValue["shim_interrupt_controller_1st_level_enable_b"] = aie2::shim_interrupt_controller_1st_level_enable_b;
   regNameToValue["shim_interrupt_controller_1st_level_irq_event_a"] = aie2::shim_interrupt_controller_1st_level_irq_event_a;
   regNameToValue["shim_interrupt_controller_1st_level_irq_event_b"] = aie2::shim_interrupt_controller_1st_level_irq_event_b;
   regNameToValue["shim_interrupt_controller_1st_level_irq_no_a"] = aie2::shim_interrupt_controller_1st_level_irq_no_a;
   regNameToValue["shim_interrupt_controller_1st_level_irq_no_b"] = aie2::shim_interrupt_controller_1st_level_irq_no_b;
   regNameToValue["shim_interrupt_controller_1st_level_mask_a"] = aie2::shim_interrupt_controller_1st_level_mask_a;
   regNameToValue["shim_interrupt_controller_1st_level_mask_b"] = aie2::shim_interrupt_controller_1st_level_mask_b;
   regNameToValue["shim_interrupt_controller_1st_level_status_a"] = aie2::shim_interrupt_controller_1st_level_status_a;
   regNameToValue["shim_interrupt_controller_1st_level_status_b"] = aie2::shim_interrupt_controller_1st_level_status_b;
   regNameToValue["shim_interrupt_controller_2nd_level_disable"] = aie2::shim_interrupt_controller_2nd_level_disable;
   regNameToValue["shim_interrupt_controller_2nd_level_enable"] = aie2::shim_interrupt_controller_2nd_level_enable;
   regNameToValue["shim_interrupt_controller_2nd_level_interrupt"] = aie2::shim_interrupt_controller_2nd_level_interrupt;
   regNameToValue["shim_interrupt_controller_2nd_level_mask"] = aie2::shim_interrupt_controller_2nd_level_mask;
   regNameToValue["shim_interrupt_controller_2nd_level_status"] = aie2::shim_interrupt_controller_2nd_level_status;
   regNameToValue["shim_lock0_value"] = aie2::shim_lock0_value;
   regNameToValue["shim_lock10_value"] = aie2::shim_lock10_value;
   regNameToValue["shim_lock11_value"] = aie2::shim_lock11_value;
   regNameToValue["shim_lock12_value"] = aie2::shim_lock12_value;
   regNameToValue["shim_lock13_value"] = aie2::shim_lock13_value;
   regNameToValue["shim_lock14_value"] = aie2::shim_lock14_value;
   regNameToValue["shim_lock15_value"] = aie2::shim_lock15_value;
   regNameToValue["shim_lock1_value"] = aie2::shim_lock1_value;
   regNameToValue["shim_lock2_value"] = aie2::shim_lock2_value;
   regNameToValue["shim_lock3_value"] = aie2::shim_lock3_value;
   regNameToValue["shim_lock4_value"] = aie2::shim_lock4_value;
   regNameToValue["shim_lock5_value"] = aie2::shim_lock5_value;
   regNameToValue["shim_lock6_value"] = aie2::shim_lock6_value;
   regNameToValue["shim_lock7_value"] = aie2::shim_lock7_value;
   regNameToValue["shim_lock8_value"] = aie2::shim_lock8_value;
   regNameToValue["shim_lock9_value"] = aie2::shim_lock9_value;
   regNameToValue["shim_lock_request"] = aie2::shim_lock_request;
   regNameToValue["shim_locks_event_selection_0"] = aie2::shim_locks_event_selection_0;
   regNameToValue["shim_locks_event_selection_1"] = aie2::shim_locks_event_selection_1;
   regNameToValue["shim_locks_event_selection_2"] = aie2::shim_locks_event_selection_2;
   regNameToValue["shim_locks_event_selection_3"] = aie2::shim_locks_event_selection_3;
   regNameToValue["shim_locks_event_selection_4"] = aie2::shim_locks_event_selection_4;
   regNameToValue["shim_locks_event_selection_5"] = aie2::shim_locks_event_selection_5;
   regNameToValue["shim_locks_overflow"] = aie2::shim_locks_overflow;
   regNameToValue["shim_locks_underflow"] = aie2::shim_locks_underflow;
   regNameToValue["shim_me_aximm_config"] = aie2::shim_me_aximm_config;
   regNameToValue["shim_module_clock_control_0"] = aie2::shim_module_clock_control_0;
   regNameToValue["shim_module_clock_control_1"] = aie2::shim_module_clock_control_1;
   regNameToValue["shim_module_reset_control_0"] = aie2::shim_module_reset_control_0;
   regNameToValue["shim_module_reset_control_1"] = aie2::shim_module_reset_control_1;
   regNameToValue["shim_mux_config"] = aie2::shim_mux_config;
   regNameToValue["shim_noc_interface_me_to_noc_south2"] = aie2::shim_noc_interface_me_to_noc_south2;
   regNameToValue["shim_noc_interface_me_to_noc_south3"] = aie2::shim_noc_interface_me_to_noc_south3;
   regNameToValue["shim_noc_interface_me_to_noc_south4"] = aie2::shim_noc_interface_me_to_noc_south4;
   regNameToValue["shim_noc_interface_me_to_noc_south5"] = aie2::shim_noc_interface_me_to_noc_south5;
   regNameToValue["shim_pl_interface_downsizer_bypass"] = aie2::shim_pl_interface_downsizer_bypass;
   regNameToValue["shim_pl_interface_downsizer_config"] = aie2::shim_pl_interface_downsizer_config;
   regNameToValue["shim_pl_interface_downsizer_enable"] = aie2::shim_pl_interface_downsizer_enable;
   regNameToValue["shim_pl_interface_upsizer_config"] = aie2::shim_pl_interface_upsizer_config;
   regNameToValue["shim_performance_counter0"] = aie2::shim_performance_counter0;
   regNameToValue["shim_performance_counter0_event_value"] = aie2::shim_performance_counter0_event_value;
   regNameToValue["shim_performance_counter1"] = aie2::shim_performance_counter1;
   regNameToValue["shim_performance_counter1_event_value"] = aie2::shim_performance_counter1_event_value;
   regNameToValue["shim_performance_control0"] = aie2::shim_performance_control0;
   regNameToValue["shim_performance_start_stop_0_1"] = aie2::shim_performance_start_stop_0_1;
   regNameToValue["shim_performance_control1"] = aie2::shim_performance_control1;
   regNameToValue["shim_performance_reset_0_1"] = aie2::shim_performance_reset_0_1;
   regNameToValue["shim_reserved0"] = aie2::shim_reserved0;
   regNameToValue["shim_reserved1"] = aie2::shim_reserved1;
   regNameToValue["shim_reserved2"] = aie2::shim_reserved2;
   regNameToValue["shim_reserved3"] = aie2::shim_reserved3;
   regNameToValue["shim_spare_reg"] = aie2::shim_spare_reg;
   regNameToValue["shim_spare_reg"] = aie2::shim_spare_reg;
   regNameToValue["shim_stream_switch_adaptive_clock_gate_abort_period"] = aie2::shim_stream_switch_adaptive_clock_gate_abort_period;
   regNameToValue["shim_stream_switch_adaptive_clock_gate_status"] = aie2::shim_stream_switch_adaptive_clock_gate_status;
   regNameToValue["shim_stream_switch_deterministic_merge_arb0_ctrl"] = aie2::shim_stream_switch_deterministic_merge_arb0_ctrl;
   regNameToValue["shim_stream_switch_deterministic_merge_arb0_slave0_1"] = aie2::shim_stream_switch_deterministic_merge_arb0_slave0_1;
   regNameToValue["shim_stream_switch_deterministic_merge_arb0_slave2_3"] = aie2::shim_stream_switch_deterministic_merge_arb0_slave2_3;
   regNameToValue["shim_stream_switch_deterministic_merge_arb1_ctrl"] = aie2::shim_stream_switch_deterministic_merge_arb1_ctrl;
   regNameToValue["shim_stream_switch_deterministic_merge_arb1_slave0_1"] = aie2::shim_stream_switch_deterministic_merge_arb1_slave0_1;
   regNameToValue["shim_stream_switch_deterministic_merge_arb1_slave2_3"] = aie2::shim_stream_switch_deterministic_merge_arb1_slave2_3;
   regNameToValue["shim_stream_switch_event_port_selection_0"] = aie2::shim_stream_switch_event_port_selection_0;
   regNameToValue["shim_stream_switch_event_port_selection_1"] = aie2::shim_stream_switch_event_port_selection_1;
   regNameToValue["shim_stream_switch_master_config_east0"] = aie2::shim_stream_switch_master_config_east0;
   regNameToValue["shim_stream_switch_master_config_east1"] = aie2::shim_stream_switch_master_config_east1;
   regNameToValue["shim_stream_switch_master_config_east2"] = aie2::shim_stream_switch_master_config_east2;
   regNameToValue["shim_stream_switch_master_config_east3"] = aie2::shim_stream_switch_master_config_east3;
   regNameToValue["shim_stream_switch_master_config_fifo0"] = aie2::shim_stream_switch_master_config_fifo0;
   regNameToValue["shim_stream_switch_master_config_north0"] = aie2::shim_stream_switch_master_config_north0;
   regNameToValue["shim_stream_switch_master_config_north1"] = aie2::shim_stream_switch_master_config_north1;
   regNameToValue["shim_stream_switch_master_config_north2"] = aie2::shim_stream_switch_master_config_north2;
   regNameToValue["shim_stream_switch_master_config_north3"] = aie2::shim_stream_switch_master_config_north3;
   regNameToValue["shim_stream_switch_master_config_north4"] = aie2::shim_stream_switch_master_config_north4;
   regNameToValue["shim_stream_switch_master_config_north5"] = aie2::shim_stream_switch_master_config_north5;
   regNameToValue["shim_stream_switch_master_config_south0"] = aie2::shim_stream_switch_master_config_south0;
   regNameToValue["shim_stream_switch_master_config_south1"] = aie2::shim_stream_switch_master_config_south1;
   regNameToValue["shim_stream_switch_master_config_south2"] = aie2::shim_stream_switch_master_config_south2;
   regNameToValue["shim_stream_switch_master_config_south3"] = aie2::shim_stream_switch_master_config_south3;
   regNameToValue["shim_stream_switch_master_config_south4"] = aie2::shim_stream_switch_master_config_south4;
   regNameToValue["shim_stream_switch_master_config_south5"] = aie2::shim_stream_switch_master_config_south5;
   regNameToValue["shim_stream_switch_master_config_tile_ctrl"] = aie2::shim_stream_switch_master_config_tile_ctrl;
   regNameToValue["shim_stream_switch_master_config_west0"] = aie2::shim_stream_switch_master_config_west0;
   regNameToValue["shim_stream_switch_master_config_west1"] = aie2::shim_stream_switch_master_config_west1;
   regNameToValue["shim_stream_switch_master_config_west2"] = aie2::shim_stream_switch_master_config_west2;
   regNameToValue["shim_stream_switch_master_config_west3"] = aie2::shim_stream_switch_master_config_west3;
   regNameToValue["shim_stream_switch_parity_injection"] = aie2::shim_stream_switch_parity_injection;
   regNameToValue["shim_stream_switch_parity_status"] = aie2::shim_stream_switch_parity_status;
   regNameToValue["shim_stream_switch_slave_config_east_0"] = aie2::shim_stream_switch_slave_config_east_0;
   regNameToValue["shim_stream_switch_slave_config_east_1"] = aie2::shim_stream_switch_slave_config_east_1;
   regNameToValue["shim_stream_switch_slave_config_east_2"] = aie2::shim_stream_switch_slave_config_east_2;
   regNameToValue["shim_stream_switch_slave_config_east_3"] = aie2::shim_stream_switch_slave_config_east_3;
   regNameToValue["shim_stream_switch_slave_config_fifo_0"] = aie2::shim_stream_switch_slave_config_fifo_0;
   regNameToValue["shim_stream_switch_slave_config_north_0"] = aie2::shim_stream_switch_slave_config_north_0;
   regNameToValue["shim_stream_switch_slave_config_north_1"] = aie2::shim_stream_switch_slave_config_north_1;
   regNameToValue["shim_stream_switch_slave_config_north_2"] = aie2::shim_stream_switch_slave_config_north_2;
   regNameToValue["shim_stream_switch_slave_config_north_3"] = aie2::shim_stream_switch_slave_config_north_3;
   regNameToValue["shim_stream_switch_slave_config_south_0"] = aie2::shim_stream_switch_slave_config_south_0;
   regNameToValue["shim_stream_switch_slave_config_south_1"] = aie2::shim_stream_switch_slave_config_south_1;
   regNameToValue["shim_stream_switch_slave_config_south_2"] = aie2::shim_stream_switch_slave_config_south_2;
   regNameToValue["shim_stream_switch_slave_config_south_3"] = aie2::shim_stream_switch_slave_config_south_3;
   regNameToValue["shim_stream_switch_slave_config_south_4"] = aie2::shim_stream_switch_slave_config_south_4;
   regNameToValue["shim_stream_switch_slave_config_south_5"] = aie2::shim_stream_switch_slave_config_south_5;
   regNameToValue["shim_stream_switch_slave_config_south_6"] = aie2::shim_stream_switch_slave_config_south_6;
   regNameToValue["shim_stream_switch_slave_config_south_7"] = aie2::shim_stream_switch_slave_config_south_7;
   regNameToValue["shim_stream_switch_slave_config_tile_ctrl"] = aie2::shim_stream_switch_slave_config_tile_ctrl;
   regNameToValue["shim_stream_switch_slave_config_trace"] = aie2::shim_stream_switch_slave_config_trace;
   regNameToValue["shim_stream_switch_slave_config_west_0"] = aie2::shim_stream_switch_slave_config_west_0;
   regNameToValue["shim_stream_switch_slave_config_west_1"] = aie2::shim_stream_switch_slave_config_west_1;
   regNameToValue["shim_stream_switch_slave_config_west_2"] = aie2::shim_stream_switch_slave_config_west_2;
   regNameToValue["shim_stream_switch_slave_config_west_3"] = aie2::shim_stream_switch_slave_config_west_3;
   regNameToValue["shim_stream_switch_slave_east_0_slot0"] = aie2::shim_stream_switch_slave_east_0_slot0;
   regNameToValue["shim_stream_switch_slave_east_0_slot1"] = aie2::shim_stream_switch_slave_east_0_slot1;
   regNameToValue["shim_stream_switch_slave_east_0_slot2"] = aie2::shim_stream_switch_slave_east_0_slot2;
   regNameToValue["shim_stream_switch_slave_east_0_slot3"] = aie2::shim_stream_switch_slave_east_0_slot3;
   regNameToValue["shim_stream_switch_slave_east_1_slot0"] = aie2::shim_stream_switch_slave_east_1_slot0;
   regNameToValue["shim_stream_switch_slave_east_1_slot1"] = aie2::shim_stream_switch_slave_east_1_slot1;
   regNameToValue["shim_stream_switch_slave_east_1_slot2"] = aie2::shim_stream_switch_slave_east_1_slot2;
   regNameToValue["shim_stream_switch_slave_east_1_slot3"] = aie2::shim_stream_switch_slave_east_1_slot3;
   regNameToValue["shim_stream_switch_slave_east_2_slot0"] = aie2::shim_stream_switch_slave_east_2_slot0;
   regNameToValue["shim_stream_switch_slave_east_2_slot1"] = aie2::shim_stream_switch_slave_east_2_slot1;
   regNameToValue["shim_stream_switch_slave_east_2_slot2"] = aie2::shim_stream_switch_slave_east_2_slot2;
   regNameToValue["shim_stream_switch_slave_east_2_slot3"] = aie2::shim_stream_switch_slave_east_2_slot3;
   regNameToValue["shim_stream_switch_slave_east_3_slot0"] = aie2::shim_stream_switch_slave_east_3_slot0;
   regNameToValue["shim_stream_switch_slave_east_3_slot1"] = aie2::shim_stream_switch_slave_east_3_slot1;
   regNameToValue["shim_stream_switch_slave_east_3_slot2"] = aie2::shim_stream_switch_slave_east_3_slot2;
   regNameToValue["shim_stream_switch_slave_east_3_slot3"] = aie2::shim_stream_switch_slave_east_3_slot3;
   regNameToValue["shim_stream_switch_slave_fifo_0_slot0"] = aie2::shim_stream_switch_slave_fifo_0_slot0;
   regNameToValue["shim_stream_switch_slave_fifo_0_slot1"] = aie2::shim_stream_switch_slave_fifo_0_slot1;
   regNameToValue["shim_stream_switch_slave_fifo_0_slot2"] = aie2::shim_stream_switch_slave_fifo_0_slot2;
   regNameToValue["shim_stream_switch_slave_fifo_0_slot3"] = aie2::shim_stream_switch_slave_fifo_0_slot3;
   regNameToValue["shim_stream_switch_slave_north_0_slot0"] = aie2::shim_stream_switch_slave_north_0_slot0;
   regNameToValue["shim_stream_switch_slave_north_0_slot1"] = aie2::shim_stream_switch_slave_north_0_slot1;
   regNameToValue["shim_stream_switch_slave_north_0_slot2"] = aie2::shim_stream_switch_slave_north_0_slot2;
   regNameToValue["shim_stream_switch_slave_north_0_slot3"] = aie2::shim_stream_switch_slave_north_0_slot3;
   regNameToValue["shim_stream_switch_slave_north_1_slot0"] = aie2::shim_stream_switch_slave_north_1_slot0;
   regNameToValue["shim_stream_switch_slave_north_1_slot1"] = aie2::shim_stream_switch_slave_north_1_slot1;
   regNameToValue["shim_stream_switch_slave_north_1_slot2"] = aie2::shim_stream_switch_slave_north_1_slot2;
   regNameToValue["shim_stream_switch_slave_north_1_slot3"] = aie2::shim_stream_switch_slave_north_1_slot3;
   regNameToValue["shim_stream_switch_slave_north_2_slot0"] = aie2::shim_stream_switch_slave_north_2_slot0;
   regNameToValue["shim_stream_switch_slave_north_2_slot1"] = aie2::shim_stream_switch_slave_north_2_slot1;
   regNameToValue["shim_stream_switch_slave_north_2_slot2"] = aie2::shim_stream_switch_slave_north_2_slot2;
   regNameToValue["shim_stream_switch_slave_north_2_slot3"] = aie2::shim_stream_switch_slave_north_2_slot3;
   regNameToValue["shim_stream_switch_slave_north_3_slot0"] = aie2::shim_stream_switch_slave_north_3_slot0;
   regNameToValue["shim_stream_switch_slave_north_3_slot1"] = aie2::shim_stream_switch_slave_north_3_slot1;
   regNameToValue["shim_stream_switch_slave_north_3_slot2"] = aie2::shim_stream_switch_slave_north_3_slot2;
   regNameToValue["shim_stream_switch_slave_north_3_slot3"] = aie2::shim_stream_switch_slave_north_3_slot3;
   regNameToValue["shim_stream_switch_slave_south_0_slot0"] = aie2::shim_stream_switch_slave_south_0_slot0;
   regNameToValue["shim_stream_switch_slave_south_0_slot1"] = aie2::shim_stream_switch_slave_south_0_slot1;
   regNameToValue["shim_stream_switch_slave_south_0_slot2"] = aie2::shim_stream_switch_slave_south_0_slot2;
   regNameToValue["shim_stream_switch_slave_south_0_slot3"] = aie2::shim_stream_switch_slave_south_0_slot3;
   regNameToValue["shim_stream_switch_slave_south_1_slot0"] = aie2::shim_stream_switch_slave_south_1_slot0;
   regNameToValue["shim_stream_switch_slave_south_1_slot1"] = aie2::shim_stream_switch_slave_south_1_slot1;
   regNameToValue["shim_stream_switch_slave_south_1_slot2"] = aie2::shim_stream_switch_slave_south_1_slot2;
   regNameToValue["shim_stream_switch_slave_south_1_slot3"] = aie2::shim_stream_switch_slave_south_1_slot3;
   regNameToValue["shim_stream_switch_slave_south_2_slot0"] = aie2::shim_stream_switch_slave_south_2_slot0;
   regNameToValue["shim_stream_switch_slave_south_2_slot1"] = aie2::shim_stream_switch_slave_south_2_slot1;
   regNameToValue["shim_stream_switch_slave_south_2_slot2"] = aie2::shim_stream_switch_slave_south_2_slot2;
   regNameToValue["shim_stream_switch_slave_south_2_slot3"] = aie2::shim_stream_switch_slave_south_2_slot3;
   regNameToValue["shim_stream_switch_slave_south_3_slot0"] = aie2::shim_stream_switch_slave_south_3_slot0;
   regNameToValue["shim_stream_switch_slave_south_3_slot1"] = aie2::shim_stream_switch_slave_south_3_slot1;
   regNameToValue["shim_stream_switch_slave_south_3_slot2"] = aie2::shim_stream_switch_slave_south_3_slot2;
   regNameToValue["shim_stream_switch_slave_south_3_slot3"] = aie2::shim_stream_switch_slave_south_3_slot3;
   regNameToValue["shim_stream_switch_slave_south_4_slot0"] = aie2::shim_stream_switch_slave_south_4_slot0;
   regNameToValue["shim_stream_switch_slave_south_4_slot1"] = aie2::shim_stream_switch_slave_south_4_slot1;
   regNameToValue["shim_stream_switch_slave_south_4_slot2"] = aie2::shim_stream_switch_slave_south_4_slot2;
   regNameToValue["shim_stream_switch_slave_south_4_slot3"] = aie2::shim_stream_switch_slave_south_4_slot3;
   regNameToValue["shim_stream_switch_slave_south_5_slot0"] = aie2::shim_stream_switch_slave_south_5_slot0;
   regNameToValue["shim_stream_switch_slave_south_5_slot1"] = aie2::shim_stream_switch_slave_south_5_slot1;
   regNameToValue["shim_stream_switch_slave_south_5_slot2"] = aie2::shim_stream_switch_slave_south_5_slot2;
   regNameToValue["shim_stream_switch_slave_south_5_slot3"] = aie2::shim_stream_switch_slave_south_5_slot3;
   regNameToValue["shim_stream_switch_slave_south_6_slot0"] = aie2::shim_stream_switch_slave_south_6_slot0;
   regNameToValue["shim_stream_switch_slave_south_6_slot1"] = aie2::shim_stream_switch_slave_south_6_slot1;
   regNameToValue["shim_stream_switch_slave_south_6_slot2"] = aie2::shim_stream_switch_slave_south_6_slot2;
   regNameToValue["shim_stream_switch_slave_south_6_slot3"] = aie2::shim_stream_switch_slave_south_6_slot3;
   regNameToValue["shim_stream_switch_slave_south_7_slot0"] = aie2::shim_stream_switch_slave_south_7_slot0;
   regNameToValue["shim_stream_switch_slave_south_7_slot1"] = aie2::shim_stream_switch_slave_south_7_slot1;
   regNameToValue["shim_stream_switch_slave_south_7_slot2"] = aie2::shim_stream_switch_slave_south_7_slot2;
   regNameToValue["shim_stream_switch_slave_south_7_slot3"] = aie2::shim_stream_switch_slave_south_7_slot3;
   regNameToValue["shim_stream_switch_slave_tile_ctrl_slot0"] = aie2::shim_stream_switch_slave_tile_ctrl_slot0;
   regNameToValue["shim_stream_switch_slave_tile_ctrl_slot1"] = aie2::shim_stream_switch_slave_tile_ctrl_slot1;
   regNameToValue["shim_stream_switch_slave_tile_ctrl_slot2"] = aie2::shim_stream_switch_slave_tile_ctrl_slot2;
   regNameToValue["shim_stream_switch_slave_tile_ctrl_slot3"] = aie2::shim_stream_switch_slave_tile_ctrl_slot3;
   regNameToValue["shim_stream_switch_slave_trace_slot0"] = aie2::shim_stream_switch_slave_trace_slot0;
   regNameToValue["shim_stream_switch_slave_trace_slot1"] = aie2::shim_stream_switch_slave_trace_slot1;
   regNameToValue["shim_stream_switch_slave_trace_slot2"] = aie2::shim_stream_switch_slave_trace_slot2;
   regNameToValue["shim_stream_switch_slave_trace_slot3"] = aie2::shim_stream_switch_slave_trace_slot3;
   regNameToValue["shim_stream_switch_slave_west_0_slot0"] = aie2::shim_stream_switch_slave_west_0_slot0;
   regNameToValue["shim_stream_switch_slave_west_0_slot1"] = aie2::shim_stream_switch_slave_west_0_slot1;
   regNameToValue["shim_stream_switch_slave_west_0_slot2"] = aie2::shim_stream_switch_slave_west_0_slot2;
   regNameToValue["shim_stream_switch_slave_west_0_slot3"] = aie2::shim_stream_switch_slave_west_0_slot3;
   regNameToValue["shim_stream_switch_slave_west_1_slot0"] = aie2::shim_stream_switch_slave_west_1_slot0;
   regNameToValue["shim_stream_switch_slave_west_1_slot1"] = aie2::shim_stream_switch_slave_west_1_slot1;
   regNameToValue["shim_stream_switch_slave_west_1_slot2"] = aie2::shim_stream_switch_slave_west_1_slot2;
   regNameToValue["shim_stream_switch_slave_west_1_slot3"] = aie2::shim_stream_switch_slave_west_1_slot3;
   regNameToValue["shim_stream_switch_slave_west_2_slot0"] = aie2::shim_stream_switch_slave_west_2_slot0;
   regNameToValue["shim_stream_switch_slave_west_2_slot1"] = aie2::shim_stream_switch_slave_west_2_slot1;
   regNameToValue["shim_stream_switch_slave_west_2_slot2"] = aie2::shim_stream_switch_slave_west_2_slot2;
   regNameToValue["shim_stream_switch_slave_west_2_slot3"] = aie2::shim_stream_switch_slave_west_2_slot3;
   regNameToValue["shim_stream_switch_slave_west_3_slot0"] = aie2::shim_stream_switch_slave_west_3_slot0;
   regNameToValue["shim_stream_switch_slave_west_3_slot1"] = aie2::shim_stream_switch_slave_west_3_slot1;
   regNameToValue["shim_stream_switch_slave_west_3_slot2"] = aie2::shim_stream_switch_slave_west_3_slot2;
   regNameToValue["shim_stream_switch_slave_west_3_slot3"] = aie2::shim_stream_switch_slave_west_3_slot3;
   regNameToValue["shim_tile_control"] = aie2::shim_tile_control;
   regNameToValue["shim_timer_control"] = aie2::shim_timer_control;
   regNameToValue["shim_timer_high"] = aie2::shim_timer_high;
   regNameToValue["shim_timer_low"] = aie2::shim_timer_low;
   regNameToValue["shim_timer_trig_event_high_value"] = aie2::shim_timer_trig_event_high_value;
   regNameToValue["shim_timer_trig_event_low_value"] = aie2::shim_timer_trig_event_low_value;
   regNameToValue["shim_trace_control0"] = aie2::shim_trace_control0;
   regNameToValue["shim_trace_control1"] = aie2::shim_trace_control1;
   regNameToValue["shim_trace_event0"] = aie2::shim_trace_event0;
   regNameToValue["shim_trace_event1"] = aie2::shim_trace_event1;
   regNameToValue["shim_trace_status"] = aie2::shim_trace_status;
   regNameToValue["shim_lock_step_size"] = aie2::shim_lock_step_size;
   regNameToValue["shim_dma_bd_step_size"] = aie2::shim_dma_bd_step_size;
   regNameToValue["shim_dma_s2mm_step_size"] = aie2::shim_dma_s2mm_step_size;
}


void AIE2UsedRegisters::populateRegValueToNameMap() {
   // core_module registers
   coreRegValueToName = {
      {0x00030000, "cm_core_amll0_part1"},
      {0x00030010, "cm_core_amll0_part2"},
      {0x00030020, "cm_core_amlh0_part1"},
      {0x00030030, "cm_core_amlh0_part2"},
      {0x00030040, "cm_core_amhl0_part1"},
      {0x00030050, "cm_core_amhl0_part2"},
      {0x00030060, "cm_core_amhh0_part1"},
      {0x00030070, "cm_core_amhh0_part2"},
      {0x00030080, "cm_core_amll1_part1"},
      {0x00030090, "cm_core_amll1_part2"},
      {0x000300A0, "cm_core_amlh1_part1"},
      {0x000300B0, "cm_core_amlh1_part2"},
      {0x000300C0, "cm_core_amhl1_part1"},
      {0x000300D0, "cm_core_amhl1_part2"},
      {0x000300E0, "cm_core_amhh1_part1"},
      {0x000300F0, "cm_core_amhh1_part2"},
      {0x00030100, "cm_core_amll2_part1"},
      {0x00030110, "cm_core_amll2_part2"},
      {0x00030120, "cm_core_amlh2_part1"},
      {0x00030130, "cm_core_amlh2_part2"},
      {0x00030140, "cm_core_amhl2_part1"},
      {0x00030150, "cm_core_amhl2_part2"},
      {0x00030160, "cm_core_amhh2_part1"},
      {0x00030170, "cm_core_amhh2_part2"},
      {0x00030180, "cm_core_amll3_part1"},
      {0x00030190, "cm_core_amll3_part2"},
      {0x000301A0, "cm_core_amlh3_part1"},
      {0x000301B0, "cm_core_amlh3_part2"},
      {0x000301C0, "cm_core_amhl3_part1"},
      {0x000301D0, "cm_core_amhl3_part2"},
      {0x000301E0, "cm_core_amhh3_part1"},
      {0x000301F0, "cm_core_amhh3_part2"},
      {0x00030200, "cm_core_amll4_part1"},
      {0x00030210, "cm_core_amll4_part2"},
      {0x00030220, "cm_core_amlh4_part1"},
      {0x00030230, "cm_core_amlh4_part2"},
      {0x00030240, "cm_core_amhl4_part1"},
      {0x00030250, "cm_core_amhl4_part2"},
      {0x00030260, "cm_core_amhh4_part1"},
      {0x00030270, "cm_core_amhh4_part2"},
      {0x00030280, "cm_core_amll5_part1"},
      {0x00030290, "cm_core_amll5_part2"},
      {0x000302A0, "cm_core_amlh5_part1"},
      {0x000302B0, "cm_core_amlh5_part2"},
      {0x000302C0, "cm_core_amhl5_part1"},
      {0x000302D0, "cm_core_amhl5_part2"},
      {0x000302E0, "cm_core_amhh5_part1"},
      {0x000302F0, "cm_core_amhh5_part2"},
      {0x00030300, "cm_core_amll6_part1"},
      {0x00030310, "cm_core_amll6_part2"},
      {0x00030320, "cm_core_amlh6_part1"},
      {0x00030330, "cm_core_amlh6_part2"},
      {0x00030340, "cm_core_amhl6_part1"},
      {0x00030350, "cm_core_amhl6_part2"},
      {0x00030360, "cm_core_amhh6_part1"},
      {0x00030370, "cm_core_amhh6_part2"},
      {0x00030380, "cm_core_amll7_part1"},
      {0x00030390, "cm_core_amll7_part2"},
      {0x000303A0, "cm_core_amlh7_part1"},
      {0x000303B0, "cm_core_amlh7_part2"},
      {0x000303C0, "cm_core_amhl7_part1"},
      {0x000303D0, "cm_core_amhl7_part2"},
      {0x000303E0, "cm_core_amhh7_part1"},
      {0x000303F0, "cm_core_amhh7_part2"},
      {0x00030400, "cm_core_amll8_part1"},
      {0x00030410, "cm_core_amll8_part2"},
      {0x00030420, "cm_core_amlh8_part1"},
      {0x00030430, "cm_core_amlh8_part2"},
      {0x00030440, "cm_core_amhl8_part1"},
      {0x00030450, "cm_core_amhl8_part2"},
      {0x00030460, "cm_core_amhh8_part1"},
      {0x00030470, "cm_core_amhh8_part2"},
      {0x00030480, "cm_reserved0"},
      {0x00030490, "cm_reserved1"},
      {0x000304A0, "cm_reserved2"},
      {0x000304B0, "cm_reserved3"},
      {0x000304C0, "cm_reserved4"},
      {0x000304D0, "cm_reserved5"},
      {0x000304E0, "cm_reserved6"},
      {0x000304F0, "cm_reserved7"},
      {0x00030500, "cm_reserved8"},
      {0x00030510, "cm_reserved9"},
      {0x00030520, "cm_reserved10"},
      {0x00030530, "cm_reserved11"},
      {0x00030540, "cm_reserved12"},
      {0x00030550, "cm_reserved13"},
      {0x00030560, "cm_reserved14"},
      {0x00030570, "cm_reserved15"},
      {0x00030580, "cm_reserved16"},
      {0x00030590, "cm_reserved17"},
      {0x000305A0, "cm_reserved18"},
      {0x000305B0, "cm_reserved19"},
      {0x000305C0, "cm_reserved20"},
      {0x000305D0, "cm_reserved21"},
      {0x000305E0, "cm_reserved22"},
      {0x000305F0, "cm_reserved23"},
      {0x00030600, "cm_reserved24"},
      {0x00030610, "cm_reserved25"},
      {0x00030620, "cm_reserved26"},
      {0x00030630, "cm_reserved27"},
      {0x00030640, "cm_reserved28"},
      {0x00030650, "cm_reserved29"},
      {0x00030660, "cm_reserved30"},
      {0x00030670, "cm_reserved31"},
      {0x00030680, "cm_reserved32"},
      {0x00030690, "cm_reserved33"},
      {0x000306A0, "cm_reserved34"},
      {0x000306B0, "cm_reserved35"},
      {0x000306C0, "cm_reserved36"},
      {0x000306D0, "cm_reserved37"},
      {0x000306E0, "cm_reserved38"},
      {0x000306F0, "cm_reserved39"},
      {0x00030700, "cm_reserved40"},
      {0x00030710, "cm_reserved41"},
      {0x00030720, "cm_reserved42"},
      {0x00030730, "cm_reserved43"},
      {0x00030740, "cm_reserved44"},
      {0x00030750, "cm_reserved45"},
      {0x00030760, "cm_reserved46"},
      {0x00030770, "cm_reserved47"},
      {0x00030780, "cm_reserved48"},
      {0x00030790, "cm_reserved49"},
      {0x000307A0, "cm_reserved50"},
      {0x000307B0, "cm_reserved51"},
      {0x000307C0, "cm_reserved52"},
      {0x000307D0, "cm_reserved53"},
      {0x000307E0, "cm_reserved54"},
      {0x000307F0, "cm_reserved55"},
      {0x00030800, "cm_core_wl0_part1"},
      {0x00030810, "cm_core_wl0_part2"},
      {0x00030820, "cm_core_wh0_part1"},
      {0x00030830, "cm_core_wh0_part2"},
      {0x00030840, "cm_core_wl1_part1"},
      {0x00030850, "cm_core_wl1_part2"},
      {0x00030860, "cm_core_wh1_part1"},
      {0x00030870, "cm_core_wh1_part2"},
      {0x00030880, "cm_core_wl2_part1"},
      {0x00030890, "cm_core_wl2_part2"},
      {0x000308A0, "cm_core_wh2_part1"},
      {0x000308B0, "cm_core_wh2_part2"},
      {0x000308C0, "cm_core_wl3_part1"},
      {0x000308D0, "cm_core_wl3_part2"},
      {0x000308E0, "cm_core_wh3_part1"},
      {0x000308F0, "cm_core_wh3_part2"},
      {0x00030900, "cm_core_wl4_part1"},
      {0x00030910, "cm_core_wl4_part2"},
      {0x00030920, "cm_core_wh4_part1"},
      {0x00030930, "cm_core_wh4_part2"},
      {0x00030940, "cm_core_wl5_part1"},
      {0x00030950, "cm_core_wl5_part2"},
      {0x00030960, "cm_core_wh5_part1"},
      {0x00030970, "cm_core_wh5_part2"},
      {0x00030980, "cm_core_wl6_part1"},
      {0x00030990, "cm_core_wl6_part2"},
      {0x000309A0, "cm_core_wh6_part1"},
      {0x000309B0, "cm_core_wh6_part2"},
      {0x000309C0, "cm_core_wl7_part1"},
      {0x000309D0, "cm_core_wl7_part2"},
      {0x000309E0, "cm_core_wh7_part1"},
      {0x000309F0, "cm_core_wh7_part2"},
      {0x00030A00, "cm_core_wl8_part1"},
      {0x00030A10, "cm_core_wl8_part2"},
      {0x00030A20, "cm_core_wh8_part1"},
      {0x00030A30, "cm_core_wh8_part2"},
      {0x00030A40, "cm_core_wl9_part1"},
      {0x00030A50, "cm_core_wl9_part2"},
      {0x00030A60, "cm_core_wh9_part1"},
      {0x00030A70, "cm_core_wh9_part2"},
      {0x00030A80, "cm_core_wl10_part1"},
      {0x00030A90, "cm_core_wl10_part2"},
      {0x00030AA0, "cm_core_wh10_part1"},
      {0x00030AB0, "cm_core_wh10_part2"},
      {0x00030AC0, "cm_core_wl11_part1"},
      {0x00030AD0, "cm_core_wl11_part2"},
      {0x00030AE0, "cm_core_wh11_part1"},
      {0x00030AF0, "cm_core_wh11_part2"},
      {0x00030B00, "cm_reserved56"},
      {0x00030B10, "cm_reserved57"},
      {0x00030B20, "cm_reserved58"},
      {0x00030B30, "cm_reserved59"},
      {0x00030B40, "cm_reserved60"},
      {0x00030B50, "cm_reserved61"},
      {0x00030B60, "cm_reserved62"},
      {0x00030B70, "cm_reserved63"},
      {0x00030B80, "cm_reserved64"},
      {0x00030B90, "cm_reserved65"},
      {0x00030BA0, "cm_reserved66"},
      {0x00030BB0, "cm_reserved67"},
      {0x00030BC0, "cm_reserved68"},
      {0x00030BD0, "cm_reserved69"},
      {0x00030BE0, "cm_reserved70"},
      {0x00030BF0, "cm_reserved71"},
      {0x00030C00, "cm_core_r0"},
      {0x00030C10, "cm_core_r1"},
      {0x00030C20, "cm_core_r2"},
      {0x00030C30, "cm_core_r3"},
      {0x00030C40, "cm_core_r4"},
      {0x00030C50, "cm_core_r5"},
      {0x00030C60, "cm_core_r6"},
      {0x00030C70, "cm_core_r7"},
      {0x00030C80, "cm_core_r8"},
      {0x00030C90, "cm_core_r9"},
      {0x00030CA0, "cm_core_r10"},
      {0x00030CB0, "cm_core_r11"},
      {0x00030CC0, "cm_core_r12"},
      {0x00030CD0, "cm_core_r13"},
      {0x00030CE0, "cm_core_r14"},
      {0x00030CF0, "cm_core_r15"},
      {0x00030D00, "cm_core_r16"},
      {0x00030D10, "cm_core_r17"},
      {0x00030D20, "cm_core_r18"},
      {0x00030D30, "cm_core_r19"},
      {0x00030D40, "cm_core_r20"},
      {0x00030D50, "cm_core_r21"},
      {0x00030D60, "cm_core_r22"},
      {0x00030D70, "cm_core_r23"},
      {0x00030D80, "cm_core_r24"},
      {0x00030D90, "cm_core_r25"},
      {0x00030DA0, "cm_core_r26"},
      {0x00030DB0, "cm_core_r27"},
      {0x00030DC0, "cm_core_r28"},
      {0x00030DD0, "cm_core_r29"},
      {0x00030DE0, "cm_core_r30"},
      {0x00030DF0, "cm_core_r31"},
      {0x00030E00, "cm_core_m0"},
      {0x00030E10, "cm_core_m1"},
      {0x00030E20, "cm_core_m2"},
      {0x00030E30, "cm_core_m3"},
      {0x00030E40, "cm_core_m4"},
      {0x00030E50, "cm_core_m5"},
      {0x00030E60, "cm_core_m6"},
      {0x00030E70, "cm_core_m7"},
      {0x00030E80, "cm_core_dn0"},
      {0x00030E90, "cm_core_dn1"},
      {0x00030EA0, "cm_core_dn2"},
      {0x00030EB0, "cm_core_dn3"},
      {0x00030EC0, "cm_core_dn4"},
      {0x00030ED0, "cm_core_dn5"},
      {0x00030EE0, "cm_core_dn6"},
      {0x00030EF0, "cm_core_dn7"},
      {0x00030F00, "cm_core_dj0"},
      {0x00030F10, "cm_core_dj1"},
      {0x00030F20, "cm_core_dj2"},
      {0x00030F30, "cm_core_dj3"},
      {0x00030F40, "cm_core_dj4"},
      {0x00030F50, "cm_core_dj5"},
      {0x00030F60, "cm_core_dj6"},
      {0x00030F70, "cm_core_dj7"},
      {0x00030F80, "cm_core_dc0"},
      {0x00030F90, "cm_core_dc1"},
      {0x00030FA0, "cm_core_dc2"},
      {0x00030FB0, "cm_core_dc3"},
      {0x00030FC0, "cm_core_dc4"},
      {0x00030FD0, "cm_core_dc5"},
      {0x00030FE0, "cm_core_dc6"},
      {0x00030FF0, "cm_core_dc7"},
      {0x00031000, "cm_core_p0"},
      {0x00031010, "cm_core_p1"},
      {0x00031020, "cm_core_p2"},
      {0x00031030, "cm_core_p3"},
      {0x00031040, "cm_core_p4"},
      {0x00031050, "cm_core_p5"},
      {0x00031060, "cm_core_p6"},
      {0x00031070, "cm_core_p7"},
      {0x00031080, "cm_core_s0"},
      {0x00031090, "cm_core_s1"},
      {0x000310A0, "cm_core_s2"},
      {0x000310B0, "cm_core_s3"},
      {0x000310C0, "cm_core_q0"},
      {0x000310D0, "cm_core_q1"},
      {0x000310E0, "cm_core_q2"},
      {0x000310F0, "cm_core_q3"},
      {0x00031100, "cm_program_counter"},
      {0x00031110, "cm_core_fc"},
      {0x00031120, "cm_core_sp"},
      {0x00031130, "cm_core_lr"},
      {0x00031140, "cm_core_ls"},
      {0x00031150, "cm_core_le"},
      {0x00031160, "cm_core_lc"},
      {0x00031170, "cm_core_cr"},
      {0x00031180, "cm_core_sr"},
      {0x00031190, "cm_core_dp"},
      {0x00031500, "cm_performance_control0"},
      {0x00031504, "cm_performance_control1"},
      {0x00031508, "cm_performance_control2"},
      {0x00031520, "cm_performance_counter0"},
      {0x00031524, "cm_performance_counter1"},
      {0x00031528, "cm_performance_counter2"},
      {0x0003152C, "cm_performance_counter3"},
      {0x00031580, "cm_performance_counter0_event_value"},
      {0x00031584, "cm_performance_counter1_event_value"},
      {0x00031588, "cm_performance_counter2_event_value"},
      {0x0003158C, "cm_performance_counter3_event_value"},
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
      {0x00032038, "cm_core_processor_bus"},
      {0x00032100, "cm_ecc_control"},
      {0x00032110, "cm_ecc_scrubbing_event"},
      {0x00032120, "cm_ecc_failing_address"},
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
      {0x00034408, "cm_edge_detection_event_control"},
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
      {0x00036040, "cm_cssd_trigger"},
      {0x00036050, "cm_spare_reg"},
      {0x00036060, "cm_accumulator_control"},
      {0x00036070, "cm_memory_control"},
      {0x0003F000, "cm_stream_switch_master_config_aie_core0"},
      {0x0003F004, "cm_stream_switch_master_config_dma0"},
      {0x0003F008, "cm_stream_switch_master_config_dma1"},
      {0x0003F00C, "cm_stream_switch_master_config_tile_ctrl"},
      {0x0003F010, "cm_stream_switch_master_config_fifo0"},
      {0x0003F014, "cm_stream_switch_master_config_south0"},
      {0x0003F018, "cm_stream_switch_master_config_south1"},
      {0x0003F01C, "cm_stream_switch_master_config_south2"},
      {0x0003F020, "cm_stream_switch_master_config_south3"},
      {0x0003F024, "cm_stream_switch_master_config_west0"},
      {0x0003F028, "cm_stream_switch_master_config_west1"},
      {0x0003F02C, "cm_stream_switch_master_config_west2"},
      {0x0003F030, "cm_stream_switch_master_config_west3"},
      {0x0003F034, "cm_stream_switch_master_config_north0"},
      {0x0003F038, "cm_stream_switch_master_config_north1"},
      {0x0003F03C, "cm_stream_switch_master_config_north2"},
      {0x0003F040, "cm_stream_switch_master_config_north3"},
      {0x0003F044, "cm_stream_switch_master_config_north4"},
      {0x0003F048, "cm_stream_switch_master_config_north5"},
      {0x0003F04C, "cm_stream_switch_master_config_east0"},
      {0x0003F050, "cm_stream_switch_master_config_east1"},
      {0x0003F054, "cm_stream_switch_master_config_east2"},
      {0x0003F058, "cm_stream_switch_master_config_east3"},
      {0x0003F100, "cm_stream_switch_slave_config_aie_core0"},
      {0x0003F104, "cm_stream_switch_slave_config_dma_0"},
      {0x0003F108, "cm_stream_switch_slave_config_dma_1"},
      {0x0003F10C, "cm_stream_switch_slave_config_tile_ctrl"},
      {0x0003F110, "cm_stream_switch_slave_config_fifo_0"},
      {0x0003F114, "cm_stream_switch_slave_config_south_0"},
      {0x0003F118, "cm_stream_switch_slave_config_south_1"},
      {0x0003F11C, "cm_stream_switch_slave_config_south_2"},
      {0x0003F120, "cm_stream_switch_slave_config_south_3"},
      {0x0003F124, "cm_stream_switch_slave_config_south_4"},
      {0x0003F128, "cm_stream_switch_slave_config_south_5"},
      {0x0003F12C, "cm_stream_switch_slave_config_west_0"},
      {0x0003F130, "cm_stream_switch_slave_config_west_1"},
      {0x0003F134, "cm_stream_switch_slave_config_west_2"},
      {0x0003F138, "cm_stream_switch_slave_config_west_3"},
      {0x0003F13C, "cm_stream_switch_slave_config_north_0"},
      {0x0003F140, "cm_stream_switch_slave_config_north_1"},
      {0x0003F144, "cm_stream_switch_slave_config_north_2"},
      {0x0003F148, "cm_stream_switch_slave_config_north_3"},
      {0x0003F14C, "cm_stream_switch_slave_config_east_0"},
      {0x0003F150, "cm_stream_switch_slave_config_east_1"},
      {0x0003F154, "cm_stream_switch_slave_config_east_2"},
      {0x0003F158, "cm_stream_switch_slave_config_east_3"},
      {0x0003F15C, "cm_stream_switch_slave_config_aie_trace"},
      {0x0003F160, "cm_stream_switch_slave_config_mem_trace"},
      {0x0003F200, "cm_stream_switch_slave_aie_core0_slot0"},
      {0x0003F204, "cm_stream_switch_slave_aie_core0_slot1"},
      {0x0003F208, "cm_stream_switch_slave_aie_core0_slot2"},
      {0x0003F20C, "cm_stream_switch_slave_aie_core0_slot3"},
      {0x0003F210, "cm_stream_switch_slave_dma_0_slot0"},
      {0x0003F214, "cm_stream_switch_slave_dma_0_slot1"},
      {0x0003F218, "cm_stream_switch_slave_dma_0_slot2"},
      {0x0003F21C, "cm_stream_switch_slave_dma_0_slot3"},
      {0x0003F220, "cm_stream_switch_slave_dma_1_slot0"},
      {0x0003F224, "cm_stream_switch_slave_dma_1_slot1"},
      {0x0003F228, "cm_stream_switch_slave_dma_1_slot2"},
      {0x0003F22C, "cm_stream_switch_slave_dma_1_slot3"},
      {0x0003F230, "cm_stream_switch_slave_tile_ctrl_slot0"},
      {0x0003F234, "cm_stream_switch_slave_tile_ctrl_slot1"},
      {0x0003F238, "cm_stream_switch_slave_tile_ctrl_slot2"},
      {0x0003F23C, "cm_stream_switch_slave_tile_ctrl_slot3"},
      {0x0003F240, "cm_stream_switch_slave_fifo_0_slot0"},
      {0x0003F244, "cm_stream_switch_slave_fifo_0_slot1"},
      {0x0003F248, "cm_stream_switch_slave_fifo_0_slot2"},
      {0x0003F24C, "cm_stream_switch_slave_fifo_0_slot3"},
      {0x0003F250, "cm_stream_switch_slave_south_0_slot0"},
      {0x0003F254, "cm_stream_switch_slave_south_0_slot1"},
      {0x0003F258, "cm_stream_switch_slave_south_0_slot2"},
      {0x0003F25C, "cm_stream_switch_slave_south_0_slot3"},
      {0x0003F260, "cm_stream_switch_slave_south_1_slot0"},
      {0x0003F264, "cm_stream_switch_slave_south_1_slot1"},
      {0x0003F268, "cm_stream_switch_slave_south_1_slot2"},
      {0x0003F26C, "cm_stream_switch_slave_south_1_slot3"},
      {0x0003F270, "cm_stream_switch_slave_south_2_slot0"},
      {0x0003F274, "cm_stream_switch_slave_south_2_slot1"},
      {0x0003F278, "cm_stream_switch_slave_south_2_slot2"},
      {0x0003F27C, "cm_stream_switch_slave_south_2_slot3"},
      {0x0003F280, "cm_stream_switch_slave_south_3_slot0"},
      {0x0003F284, "cm_stream_switch_slave_south_3_slot1"},
      {0x0003F288, "cm_stream_switch_slave_south_3_slot2"},
      {0x0003F28C, "cm_stream_switch_slave_south_3_slot3"},
      {0x0003F290, "cm_stream_switch_slave_south_4_slot0"},
      {0x0003F294, "cm_stream_switch_slave_south_4_slot1"},
      {0x0003F298, "cm_stream_switch_slave_south_4_slot2"},
      {0x0003F29C, "cm_stream_switch_slave_south_4_slot3"},
      {0x0003F2A0, "cm_stream_switch_slave_south_5_slot0"},
      {0x0003F2A4, "cm_stream_switch_slave_south_5_slot1"},
      {0x0003F2A8, "cm_stream_switch_slave_south_5_slot2"},
      {0x0003F2AC, "cm_stream_switch_slave_south_5_slot3"},
      {0x0003F2B0, "cm_stream_switch_slave_west_0_slot0"},
      {0x0003F2B4, "cm_stream_switch_slave_west_0_slot1"},
      {0x0003F2B8, "cm_stream_switch_slave_west_0_slot2"},
      {0x0003F2BC, "cm_stream_switch_slave_west_0_slot3"},
      {0x0003F2C0, "cm_stream_switch_slave_west_1_slot0"},
      {0x0003F2C4, "cm_stream_switch_slave_west_1_slot1"},
      {0x0003F2C8, "cm_stream_switch_slave_west_1_slot2"},
      {0x0003F2CC, "cm_stream_switch_slave_west_1_slot3"},
      {0x0003F2D0, "cm_stream_switch_slave_west_2_slot0"},
      {0x0003F2D4, "cm_stream_switch_slave_west_2_slot1"},
      {0x0003F2D8, "cm_stream_switch_slave_west_2_slot2"},
      {0x0003F2DC, "cm_stream_switch_slave_west_2_slot3"},
      {0x0003F2E0, "cm_stream_switch_slave_west_3_slot0"},
      {0x0003F2E4, "cm_stream_switch_slave_west_3_slot1"},
      {0x0003F2E8, "cm_stream_switch_slave_west_3_slot2"},
      {0x0003F2EC, "cm_stream_switch_slave_west_3_slot3"},
      {0x0003F2F0, "cm_stream_switch_slave_north_0_slot0"},
      {0x0003F2F4, "cm_stream_switch_slave_north_0_slot1"},
      {0x0003F2F8, "cm_stream_switch_slave_north_0_slot2"},
      {0x0003F2FC, "cm_stream_switch_slave_north_0_slot3"},
      {0x0003F300, "cm_stream_switch_slave_north_1_slot0"},
      {0x0003F304, "cm_stream_switch_slave_north_1_slot1"},
      {0x0003F308, "cm_stream_switch_slave_north_1_slot2"},
      {0x0003F30C, "cm_stream_switch_slave_north_1_slot3"},
      {0x0003F310, "cm_stream_switch_slave_north_2_slot0"},
      {0x0003F314, "cm_stream_switch_slave_north_2_slot1"},
      {0x0003F318, "cm_stream_switch_slave_north_2_slot2"},
      {0x0003F31C, "cm_stream_switch_slave_north_2_slot3"},
      {0x0003F320, "cm_stream_switch_slave_north_3_slot0"},
      {0x0003F324, "cm_stream_switch_slave_north_3_slot1"},
      {0x0003F328, "cm_stream_switch_slave_north_3_slot2"},
      {0x0003F32C, "cm_stream_switch_slave_north_3_slot3"},
      {0x0003F330, "cm_stream_switch_slave_east_0_slot0"},
      {0x0003F334, "cm_stream_switch_slave_east_0_slot1"},
      {0x0003F338, "cm_stream_switch_slave_east_0_slot2"},
      {0x0003F33C, "cm_stream_switch_slave_east_0_slot3"},
      {0x0003F340, "cm_stream_switch_slave_east_1_slot0"},
      {0x0003F344, "cm_stream_switch_slave_east_1_slot1"},
      {0x0003F348, "cm_stream_switch_slave_east_1_slot2"},
      {0x0003F34C, "cm_stream_switch_slave_east_1_slot3"},
      {0x0003F350, "cm_stream_switch_slave_east_2_slot0"},
      {0x0003F354, "cm_stream_switch_slave_east_2_slot1"},
      {0x0003F358, "cm_stream_switch_slave_east_2_slot2"},
      {0x0003F35C, "cm_stream_switch_slave_east_2_slot3"},
      {0x0003F360, "cm_stream_switch_slave_east_3_slot0"},
      {0x0003F364, "cm_stream_switch_slave_east_3_slot1"},
      {0x0003F368, "cm_stream_switch_slave_east_3_slot2"},
      {0x0003F36C, "cm_stream_switch_slave_east_3_slot3"},
      {0x0003F370, "cm_stream_switch_slave_aie_trace_slot0"},
      {0x0003F374, "cm_stream_switch_slave_aie_trace_slot1"},
      {0x0003F378, "cm_stream_switch_slave_aie_trace_slot2"},
      {0x0003F37C, "cm_stream_switch_slave_aie_trace_slot3"},
      {0x0003F380, "cm_stream_switch_slave_mem_trace_slot0"},
      {0x0003F384, "cm_stream_switch_slave_mem_trace_slot1"},
      {0x0003F388, "cm_stream_switch_slave_mem_trace_slot2"},
      {0x0003F38C, "cm_stream_switch_slave_mem_trace_slot3"},
      {0x0003F800, "cm_stream_switch_deterministic_merge_arb0_slave0_1"},
      {0x0003F804, "cm_stream_switch_deterministic_merge_arb0_slave2_3"},
      {0x0003F808, "cm_stream_switch_deterministic_merge_arb0_ctrl"},
      {0x0003F810, "cm_stream_switch_deterministic_merge_arb1_slave0_1"},
      {0x0003F814, "cm_stream_switch_deterministic_merge_arb1_slave2_3"},
      {0x0003F818, "cm_stream_switch_deterministic_merge_arb1_ctrl"},
      {0x0003FF00, "cm_stream_switch_event_port_selection_0"},
      {0x0003FF04, "cm_stream_switch_event_port_selection_1"},
      {0x0003FF10, "cm_stream_switch_parity_status"},
      {0x0003FF20, "cm_stream_switch_parity_injection"},
      {0x0003FF30, "cm_tile_control_packet_handler_status"},
      {0x0003FF34, "cm_stream_switch_adaptive_clock_gate_status"},
      {0x0003FF38, "cm_stream_switch_adaptive_clock_gate_abort_period"},
      {0x00060000, "cm_module_clock_control"},
      {0x00060010, "cm_module_reset_control"}
   };

   // memory_module registers
   memoryRegValueToName = {
      {0x00012000, "mm_checkbit_error_generation"},
      {0x00014404, "mm_combo_event_control"},
      {0x00014400, "mm_combo_event_inputs"},
      {0x0001D000, "mm_dma_bd0_0"},
      {0x0001D004, "mm_dma_bd0_1"},
      {0x0001D008, "mm_dma_bd0_2"},
      {0x0001D00C, "mm_dma_bd0_3"},
      {0x0001D010, "mm_dma_bd0_4"},
      {0x0001D014, "mm_dma_bd0_5"},
      {0x0001D140, "mm_dma_bd10_0"},
      {0x0001D144, "mm_dma_bd10_1"},
      {0x0001D148, "mm_dma_bd10_2"},
      {0x0001D14C, "mm_dma_bd10_3"},
      {0x0001D150, "mm_dma_bd10_4"},
      {0x0001D154, "mm_dma_bd10_5"},
      {0x0001D160, "mm_dma_bd11_0"},
      {0x0001D164, "mm_dma_bd11_1"},
      {0x0001D168, "mm_dma_bd11_2"},
      {0x0001D16C, "mm_dma_bd11_3"},
      {0x0001D170, "mm_dma_bd11_4"},
      {0x0001D174, "mm_dma_bd11_5"},
      {0x0001D180, "mm_dma_bd12_0"},
      {0x0001D184, "mm_dma_bd12_1"},
      {0x0001D188, "mm_dma_bd12_2"},
      {0x0001D18C, "mm_dma_bd12_3"},
      {0x0001D190, "mm_dma_bd12_4"},
      {0x0001D194, "mm_dma_bd12_5"},
      {0x0001D1A0, "mm_dma_bd13_0"},
      {0x0001D1A4, "mm_dma_bd13_1"},
      {0x0001D1A8, "mm_dma_bd13_2"},
      {0x0001D1AC, "mm_dma_bd13_3"},
      {0x0001D1B0, "mm_dma_bd13_4"},
      {0x0001D1B4, "mm_dma_bd13_5"},
      {0x0001D1C0, "mm_dma_bd14_0"},
      {0x0001D1C4, "mm_dma_bd14_1"},
      {0x0001D1C8, "mm_dma_bd14_2"},
      {0x0001D1CC, "mm_dma_bd14_3"},
      {0x0001D1D0, "mm_dma_bd14_4"},
      {0x0001D1D4, "mm_dma_bd14_5"},
      {0x0001D1E0, "mm_dma_bd15_0"},
      {0x0001D1E4, "mm_dma_bd15_1"},
      {0x0001D1E8, "mm_dma_bd15_2"},
      {0x0001D1EC, "mm_dma_bd15_3"},
      {0x0001D1F0, "mm_dma_bd15_4"},
      {0x0001D1F4, "mm_dma_bd15_5"},
      {0x0001D020, "mm_dma_bd1_0"},
      {0x0001D024, "mm_dma_bd1_1"},
      {0x0001D028, "mm_dma_bd1_2"},
      {0x0001D02C, "mm_dma_bd1_3"},
      {0x0001D030, "mm_dma_bd1_4"},
      {0x0001D034, "mm_dma_bd1_5"},
      {0x0001D040, "mm_dma_bd2_0"},
      {0x0001D044, "mm_dma_bd2_1"},
      {0x0001D048, "mm_dma_bd2_2"},
      {0x0001D04C, "mm_dma_bd2_3"},
      {0x0001D050, "mm_dma_bd2_4"},
      {0x0001D054, "mm_dma_bd2_5"},
      {0x0001D060, "mm_dma_bd3_0"},
      {0x0001D064, "mm_dma_bd3_1"},
      {0x0001D068, "mm_dma_bd3_2"},
      {0x0001D06C, "mm_dma_bd3_3"},
      {0x0001D070, "mm_dma_bd3_4"},
      {0x0001D074, "mm_dma_bd3_5"},
      {0x0001D080, "mm_dma_bd4_0"},
      {0x0001D084, "mm_dma_bd4_1"},
      {0x0001D088, "mm_dma_bd4_2"},
      {0x0001D08C, "mm_dma_bd4_3"},
      {0x0001D090, "mm_dma_bd4_4"},
      {0x0001D094, "mm_dma_bd4_5"},
      {0x0001D0A0, "mm_dma_bd5_0"},
      {0x0001D0A4, "mm_dma_bd5_1"},
      {0x0001D0A8, "mm_dma_bd5_2"},
      {0x0001D0AC, "mm_dma_bd5_3"},
      {0x0001D0B0, "mm_dma_bd5_4"},
      {0x0001D0B4, "mm_dma_bd5_5"},
      {0x0001D0C0, "mm_dma_bd6_0"},
      {0x0001D0C4, "mm_dma_bd6_1"},
      {0x0001D0C8, "mm_dma_bd6_2"},
      {0x0001D0CC, "mm_dma_bd6_3"},
      {0x0001D0D0, "mm_dma_bd6_4"},
      {0x0001D0D4, "mm_dma_bd6_5"},
      {0x0001D0E0, "mm_dma_bd7_0"},
      {0x0001D0E4, "mm_dma_bd7_1"},
      {0x0001D0E8, "mm_dma_bd7_2"},
      {0x0001D0EC, "mm_dma_bd7_3"},
      {0x0001D0F0, "mm_dma_bd7_4"},
      {0x0001D0F4, "mm_dma_bd7_5"},
      {0x0001D100, "mm_dma_bd8_0"},
      {0x0001D104, "mm_dma_bd8_1"},
      {0x0001D108, "mm_dma_bd8_2"},
      {0x0001D10C, "mm_dma_bd8_3"},
      {0x0001D110, "mm_dma_bd8_4"},
      {0x0001D114, "mm_dma_bd8_5"},
      {0x0001D120, "mm_dma_bd9_0"},
      {0x0001D124, "mm_dma_bd9_1"},
      {0x0001D128, "mm_dma_bd9_2"},
      {0x0001D12C, "mm_dma_bd9_3"},
      {0x0001D130, "mm_dma_bd9_4"},
      {0x0001D134, "mm_dma_bd9_5"},
      {0x0001DE10, "mm_dma_mm2s_0_ctrl"},
      {0x0001DE14, "mm_dma_mm2s_0_start_queue"},
      {0x0001DE18, "mm_dma_mm2s_1_ctrl"},
      {0x0001DE1C, "mm_dma_mm2s_1_start_queue"},
      {0x0001DF10, "mm_dma_mm2s_status_0"},
      {0x0001DF14, "mm_dma_mm2s_status_1"},
      {0x0001DE00, "mm_dma_s2mm_0_ctrl"},
      {0x0001DE04, "mm_dma_s2mm_0_start_queue"},
      {0x0001DE08, "mm_dma_s2mm_1_ctrl"},
      {0x0001DE0C, "mm_dma_s2mm_1_start_queue"},
      {0x0001DF18, "mm_dma_s2mm_current_write_count_0"},
      {0x0001DF1C, "mm_dma_s2mm_current_write_count_1"},
      {0x0001DF20, "mm_dma_s2mm_fot_count_fifo_pop_0"},
      {0x0001DF24, "mm_dma_s2mm_fot_count_fifo_pop_1"},
      {0x0001DF00, "mm_dma_s2mm_status_0"},
      {0x0001DF04, "mm_dma_s2mm_status_1"},
      {0x00012120, "mm_ecc_failing_address"},
      {0x00012110, "mm_ecc_scrubbing_event"},
      {0x00014408, "mm_edge_detection_event_control"},
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
      {0x0001F000, "mm_lock0_value"},
      {0x0001F0A0, "mm_lock10_value"},
      {0x0001F0B0, "mm_lock11_value"},
      {0x0001F0C0, "mm_lock12_value"},
      {0x0001F0D0, "mm_lock13_value"},
      {0x0001F0E0, "mm_lock14_value"},
      {0x0001F0F0, "mm_lock15_value"},
      {0x0001F010, "mm_lock1_value"},
      {0x0001F020, "mm_lock2_value"},
      {0x0001F030, "mm_lock3_value"},
      {0x0001F040, "mm_lock4_value"},
      {0x0001F050, "mm_lock5_value"},
      {0x0001F060, "mm_lock6_value"},
      {0x0001F070, "mm_lock7_value"},
      {0x0001F080, "mm_lock8_value"},
      {0x0001F090, "mm_lock9_value"},
      {0x00040000, "mm_lock_request"},
      {0x0001F100, "mm_locks_event_selection_0"},
      {0x0001F104, "mm_locks_event_selection_1"},
      {0x0001F108, "mm_locks_event_selection_2"},
      {0x0001F10C, "mm_locks_event_selection_3"},
      {0x0001F110, "mm_locks_event_selection_4"},
      {0x0001F114, "mm_locks_event_selection_5"},
      {0x0001F118, "mm_locks_event_selection_6"},
      {0x0001F11C, "mm_locks_event_selection_7"},
      {0x0001F120, "mm_locks_overflow"},
      {0x0001F128, "mm_locks_underflow"},
      {0x00016010, "mm_memory_control"},
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
      {0x00096040, "mem_cssd_trigger"},
      {0x00092000, "mem_checkbit_error_generation"},
      {0x00094404, "mem_combo_event_control"},
      {0x00094400, "mem_combo_event_inputs"},
      {0x000A0000, "mem_dma_bd0_0"},
      {0x000A0004, "mem_dma_bd0_1"},
      {0x000A0008, "mem_dma_bd0_2"},
      {0x000A000C, "mem_dma_bd0_3"},
      {0x000A0010, "mem_dma_bd0_4"},
      {0x000A0014, "mem_dma_bd0_5"},
      {0x000A0018, "mem_dma_bd0_6"},
      {0x000A001C, "mem_dma_bd0_7"},
      {0x000A0140, "mem_dma_bd10_0"},
      {0x000A0144, "mem_dma_bd10_1"},
      {0x000A0148, "mem_dma_bd10_2"},
      {0x000A014C, "mem_dma_bd10_3"},
      {0x000A0150, "mem_dma_bd10_4"},
      {0x000A0154, "mem_dma_bd10_5"},
      {0x000A0158, "mem_dma_bd10_6"},
      {0x000A015C, "mem_dma_bd10_7"},
      {0x000A0160, "mem_dma_bd11_0"},
      {0x000A0164, "mem_dma_bd11_1"},
      {0x000A0168, "mem_dma_bd11_2"},
      {0x000A016C, "mem_dma_bd11_3"},
      {0x000A0170, "mem_dma_bd11_4"},
      {0x000A0174, "mem_dma_bd11_5"},
      {0x000A0178, "mem_dma_bd11_6"},
      {0x000A017C, "mem_dma_bd11_7"},
      {0x000A0180, "mem_dma_bd12_0"},
      {0x000A0184, "mem_dma_bd12_1"},
      {0x000A0188, "mem_dma_bd12_2"},
      {0x000A018C, "mem_dma_bd12_3"},
      {0x000A0190, "mem_dma_bd12_4"},
      {0x000A0194, "mem_dma_bd12_5"},
      {0x000A0198, "mem_dma_bd12_6"},
      {0x000A019C, "mem_dma_bd12_7"},
      {0x000A01A0, "mem_dma_bd13_0"},
      {0x000A01A4, "mem_dma_bd13_1"},
      {0x000A01A8, "mem_dma_bd13_2"},
      {0x000A01AC, "mem_dma_bd13_3"},
      {0x000A01B0, "mem_dma_bd13_4"},
      {0x000A01B4, "mem_dma_bd13_5"},
      {0x000A01B8, "mem_dma_bd13_6"},
      {0x000A01BC, "mem_dma_bd13_7"},
      {0x000A01C0, "mem_dma_bd14_0"},
      {0x000A01C4, "mem_dma_bd14_1"},
      {0x000A01C8, "mem_dma_bd14_2"},
      {0x000A01CC, "mem_dma_bd14_3"},
      {0x000A01D0, "mem_dma_bd14_4"},
      {0x000A01D4, "mem_dma_bd14_5"},
      {0x000A01D8, "mem_dma_bd14_6"},
      {0x000A01DC, "mem_dma_bd14_7"},
      {0x000A01E0, "mem_dma_bd15_0"},
      {0x000A01E4, "mem_dma_bd15_1"},
      {0x000A01E8, "mem_dma_bd15_2"},
      {0x000A01EC, "mem_dma_bd15_3"},
      {0x000A01F0, "mem_dma_bd15_4"},
      {0x000A01F4, "mem_dma_bd15_5"},
      {0x000A01F8, "mem_dma_bd15_6"},
      {0x000A01FC, "mem_dma_bd15_7"},
      {0x000A0200, "mem_dma_bd16_0"},
      {0x000A0204, "mem_dma_bd16_1"},
      {0x000A0208, "mem_dma_bd16_2"},
      {0x000A020C, "mem_dma_bd16_3"},
      {0x000A0210, "mem_dma_bd16_4"},
      {0x000A0214, "mem_dma_bd16_5"},
      {0x000A0218, "mem_dma_bd16_6"},
      {0x000A021C, "mem_dma_bd16_7"},
      {0x000A0220, "mem_dma_bd17_0"},
      {0x000A0224, "mem_dma_bd17_1"},
      {0x000A0228, "mem_dma_bd17_2"},
      {0x000A022C, "mem_dma_bd17_3"},
      {0x000A0230, "mem_dma_bd17_4"},
      {0x000A0234, "mem_dma_bd17_5"},
      {0x000A0238, "mem_dma_bd17_6"},
      {0x000A023C, "mem_dma_bd17_7"},
      {0x000A0240, "mem_dma_bd18_0"},
      {0x000A0244, "mem_dma_bd18_1"},
      {0x000A0248, "mem_dma_bd18_2"},
      {0x000A024C, "mem_dma_bd18_3"},
      {0x000A0250, "mem_dma_bd18_4"},
      {0x000A0254, "mem_dma_bd18_5"},
      {0x000A0258, "mem_dma_bd18_6"},
      {0x000A025C, "mem_dma_bd18_7"},
      {0x000A0260, "mem_dma_bd19_0"},
      {0x000A0264, "mem_dma_bd19_1"},
      {0x000A0268, "mem_dma_bd19_2"},
      {0x000A026C, "mem_dma_bd19_3"},
      {0x000A0270, "mem_dma_bd19_4"},
      {0x000A0274, "mem_dma_bd19_5"},
      {0x000A0278, "mem_dma_bd19_6"},
      {0x000A027C, "mem_dma_bd19_7"},
      {0x000A0020, "mem_dma_bd1_0"},
      {0x000A0024, "mem_dma_bd1_1"},
      {0x000A0028, "mem_dma_bd1_2"},
      {0x000A002C, "mem_dma_bd1_3"},
      {0x000A0030, "mem_dma_bd1_4"},
      {0x000A0034, "mem_dma_bd1_5"},
      {0x000A0038, "mem_dma_bd1_6"},
      {0x000A003C, "mem_dma_bd1_7"},
      {0x000A0280, "mem_dma_bd20_0"},
      {0x000A0284, "mem_dma_bd20_1"},
      {0x000A0288, "mem_dma_bd20_2"},
      {0x000A028C, "mem_dma_bd20_3"},
      {0x000A0290, "mem_dma_bd20_4"},
      {0x000A0294, "mem_dma_bd20_5"},
      {0x000A0298, "mem_dma_bd20_6"},
      {0x000A029C, "mem_dma_bd20_7"},
      {0x000A02A0, "mem_dma_bd21_0"},
      {0x000A02A4, "mem_dma_bd21_1"},
      {0x000A02A8, "mem_dma_bd21_2"},
      {0x000A02AC, "mem_dma_bd21_3"},
      {0x000A02B0, "mem_dma_bd21_4"},
      {0x000A02B4, "mem_dma_bd21_5"},
      {0x000A02B8, "mem_dma_bd21_6"},
      {0x000A02BC, "mem_dma_bd21_7"},
      {0x000A02C0, "mem_dma_bd22_0"},
      {0x000A02C4, "mem_dma_bd22_1"},
      {0x000A02C8, "mem_dma_bd22_2"},
      {0x000A02CC, "mem_dma_bd22_3"},
      {0x000A02D0, "mem_dma_bd22_4"},
      {0x000A02D4, "mem_dma_bd22_5"},
      {0x000A02D8, "mem_dma_bd22_6"},
      {0x000A02DC, "mem_dma_bd22_7"},
      {0x000A02E0, "mem_dma_bd23_0"},
      {0x000A02E4, "mem_dma_bd23_1"},
      {0x000A02E8, "mem_dma_bd23_2"},
      {0x000A02EC, "mem_dma_bd23_3"},
      {0x000A02F0, "mem_dma_bd23_4"},
      {0x000A02F4, "mem_dma_bd23_5"},
      {0x000A02F8, "mem_dma_bd23_6"},
      {0x000A02FC, "mem_dma_bd23_7"},
      {0x000A0300, "mem_dma_bd24_0"},
      {0x000A0304, "mem_dma_bd24_1"},
      {0x000A0308, "mem_dma_bd24_2"},
      {0x000A030C, "mem_dma_bd24_3"},
      {0x000A0310, "mem_dma_bd24_4"},
      {0x000A0314, "mem_dma_bd24_5"},
      {0x000A0318, "mem_dma_bd24_6"},
      {0x000A031C, "mem_dma_bd24_7"},
      {0x000A0320, "mem_dma_bd25_0"},
      {0x000A0324, "mem_dma_bd25_1"},
      {0x000A0328, "mem_dma_bd25_2"},
      {0x000A032C, "mem_dma_bd25_3"},
      {0x000A0330, "mem_dma_bd25_4"},
      {0x000A0334, "mem_dma_bd25_5"},
      {0x000A0338, "mem_dma_bd25_6"},
      {0x000A033C, "mem_dma_bd25_7"},
      {0x000A0340, "mem_dma_bd26_0"},
      {0x000A0344, "mem_dma_bd26_1"},
      {0x000A0348, "mem_dma_bd26_2"},
      {0x000A034C, "mem_dma_bd26_3"},
      {0x000A0350, "mem_dma_bd26_4"},
      {0x000A0354, "mem_dma_bd26_5"},
      {0x000A0358, "mem_dma_bd26_6"},
      {0x000A035C, "mem_dma_bd26_7"},
      {0x000A0360, "mem_dma_bd27_0"},
      {0x000A0364, "mem_dma_bd27_1"},
      {0x000A0368, "mem_dma_bd27_2"},
      {0x000A036C, "mem_dma_bd27_3"},
      {0x000A0370, "mem_dma_bd27_4"},
      {0x000A0374, "mem_dma_bd27_5"},
      {0x000A0378, "mem_dma_bd27_6"},
      {0x000A037C, "mem_dma_bd27_7"},
      {0x000A0380, "mem_dma_bd28_0"},
      {0x000A0384, "mem_dma_bd28_1"},
      {0x000A0388, "mem_dma_bd28_2"},
      {0x000A038C, "mem_dma_bd28_3"},
      {0x000A0390, "mem_dma_bd28_4"},
      {0x000A0394, "mem_dma_bd28_5"},
      {0x000A0398, "mem_dma_bd28_6"},
      {0x000A039C, "mem_dma_bd28_7"},
      {0x000A03A0, "mem_dma_bd29_0"},
      {0x000A03A4, "mem_dma_bd29_1"},
      {0x000A03A8, "mem_dma_bd29_2"},
      {0x000A03AC, "mem_dma_bd29_3"},
      {0x000A03B0, "mem_dma_bd29_4"},
      {0x000A03B4, "mem_dma_bd29_5"},
      {0x000A03B8, "mem_dma_bd29_6"},
      {0x000A03BC, "mem_dma_bd29_7"},
      {0x000A0040, "mem_dma_bd2_0"},
      {0x000A0044, "mem_dma_bd2_1"},
      {0x000A0048, "mem_dma_bd2_2"},
      {0x000A004C, "mem_dma_bd2_3"},
      {0x000A0050, "mem_dma_bd2_4"},
      {0x000A0054, "mem_dma_bd2_5"},
      {0x000A0058, "mem_dma_bd2_6"},
      {0x000A005C, "mem_dma_bd2_7"},
      {0x000A03C0, "mem_dma_bd30_0"},
      {0x000A03C4, "mem_dma_bd30_1"},
      {0x000A03C8, "mem_dma_bd30_2"},
      {0x000A03CC, "mem_dma_bd30_3"},
      {0x000A03D0, "mem_dma_bd30_4"},
      {0x000A03D4, "mem_dma_bd30_5"},
      {0x000A03D8, "mem_dma_bd30_6"},
      {0x000A03DC, "mem_dma_bd30_7"},
      {0x000A03E0, "mem_dma_bd31_0"},
      {0x000A03E4, "mem_dma_bd31_1"},
      {0x000A03E8, "mem_dma_bd31_2"},
      {0x000A03EC, "mem_dma_bd31_3"},
      {0x000A03F0, "mem_dma_bd31_4"},
      {0x000A03F4, "mem_dma_bd31_5"},
      {0x000A03F8, "mem_dma_bd31_6"},
      {0x000A03FC, "mem_dma_bd31_7"},
      {0x000A0400, "mem_dma_bd32_0"},
      {0x000A0404, "mem_dma_bd32_1"},
      {0x000A0408, "mem_dma_bd32_2"},
      {0x000A040C, "mem_dma_bd32_3"},
      {0x000A0410, "mem_dma_bd32_4"},
      {0x000A0414, "mem_dma_bd32_5"},
      {0x000A0418, "mem_dma_bd32_6"},
      {0x000A041C, "mem_dma_bd32_7"},
      {0x000A0420, "mem_dma_bd33_0"},
      {0x000A0424, "mem_dma_bd33_1"},
      {0x000A0428, "mem_dma_bd33_2"},
      {0x000A042C, "mem_dma_bd33_3"},
      {0x000A0430, "mem_dma_bd33_4"},
      {0x000A0434, "mem_dma_bd33_5"},
      {0x000A0438, "mem_dma_bd33_6"},
      {0x000A043C, "mem_dma_bd33_7"},
      {0x000A0440, "mem_dma_bd34_0"},
      {0x000A0444, "mem_dma_bd34_1"},
      {0x000A0448, "mem_dma_bd34_2"},
      {0x000A044C, "mem_dma_bd34_3"},
      {0x000A0450, "mem_dma_bd34_4"},
      {0x000A0454, "mem_dma_bd34_5"},
      {0x000A0458, "mem_dma_bd34_6"},
      {0x000A045C, "mem_dma_bd34_7"},
      {0x000A0460, "mem_dma_bd35_0"},
      {0x000A0464, "mem_dma_bd35_1"},
      {0x000A0468, "mem_dma_bd35_2"},
      {0x000A046C, "mem_dma_bd35_3"},
      {0x000A0470, "mem_dma_bd35_4"},
      {0x000A0474, "mem_dma_bd35_5"},
      {0x000A0478, "mem_dma_bd35_6"},
      {0x000A047C, "mem_dma_bd35_7"},
      {0x000A0480, "mem_dma_bd36_0"},
      {0x000A0484, "mem_dma_bd36_1"},
      {0x000A0488, "mem_dma_bd36_2"},
      {0x000A048C, "mem_dma_bd36_3"},
      {0x000A0490, "mem_dma_bd36_4"},
      {0x000A0494, "mem_dma_bd36_5"},
      {0x000A0498, "mem_dma_bd36_6"},
      {0x000A049C, "mem_dma_bd36_7"},
      {0x000A04A0, "mem_dma_bd37_0"},
      {0x000A04A4, "mem_dma_bd37_1"},
      {0x000A04A8, "mem_dma_bd37_2"},
      {0x000A04AC, "mem_dma_bd37_3"},
      {0x000A04B0, "mem_dma_bd37_4"},
      {0x000A04B4, "mem_dma_bd37_5"},
      {0x000A04B8, "mem_dma_bd37_6"},
      {0x000A04BC, "mem_dma_bd37_7"},
      {0x000A04C0, "mem_dma_bd38_0"},
      {0x000A04C4, "mem_dma_bd38_1"},
      {0x000A04C8, "mem_dma_bd38_2"},
      {0x000A04CC, "mem_dma_bd38_3"},
      {0x000A04D0, "mem_dma_bd38_4"},
      {0x000A04D4, "mem_dma_bd38_5"},
      {0x000A04D8, "mem_dma_bd38_6"},
      {0x000A04DC, "mem_dma_bd38_7"},
      {0x000A04E0, "mem_dma_bd39_0"},
      {0x000A04E4, "mem_dma_bd39_1"},
      {0x000A04E8, "mem_dma_bd39_2"},
      {0x000A04EC, "mem_dma_bd39_3"},
      {0x000A04F0, "mem_dma_bd39_4"},
      {0x000A04F4, "mem_dma_bd39_5"},
      {0x000A04F8, "mem_dma_bd39_6"},
      {0x000A04FC, "mem_dma_bd39_7"},
      {0x000A0060, "mem_dma_bd3_0"},
      {0x000A0064, "mem_dma_bd3_1"},
      {0x000A0068, "mem_dma_bd3_2"},
      {0x000A006C, "mem_dma_bd3_3"},
      {0x000A0070, "mem_dma_bd3_4"},
      {0x000A0074, "mem_dma_bd3_5"},
      {0x000A0078, "mem_dma_bd3_6"},
      {0x000A007C, "mem_dma_bd3_7"},
      {0x000A0500, "mem_dma_bd40_0"},
      {0x000A0504, "mem_dma_bd40_1"},
      {0x000A0508, "mem_dma_bd40_2"},
      {0x000A050C, "mem_dma_bd40_3"},
      {0x000A0510, "mem_dma_bd40_4"},
      {0x000A0514, "mem_dma_bd40_5"},
      {0x000A0518, "mem_dma_bd40_6"},
      {0x000A051C, "mem_dma_bd40_7"},
      {0x000A0520, "mem_dma_bd41_0"},
      {0x000A0524, "mem_dma_bd41_1"},
      {0x000A0528, "mem_dma_bd41_2"},
      {0x000A052C, "mem_dma_bd41_3"},
      {0x000A0530, "mem_dma_bd41_4"},
      {0x000A0534, "mem_dma_bd41_5"},
      {0x000A0538, "mem_dma_bd41_6"},
      {0x000A053C, "mem_dma_bd41_7"},
      {0x000A0540, "mem_dma_bd42_0"},
      {0x000A0544, "mem_dma_bd42_1"},
      {0x000A0548, "mem_dma_bd42_2"},
      {0x000A054C, "mem_dma_bd42_3"},
      {0x000A0550, "mem_dma_bd42_4"},
      {0x000A0554, "mem_dma_bd42_5"},
      {0x000A0558, "mem_dma_bd42_6"},
      {0x000A055C, "mem_dma_bd42_7"},
      {0x000A0560, "mem_dma_bd43_0"},
      {0x000A0564, "mem_dma_bd43_1"},
      {0x000A0568, "mem_dma_bd43_2"},
      {0x000A056C, "mem_dma_bd43_3"},
      {0x000A0570, "mem_dma_bd43_4"},
      {0x000A0574, "mem_dma_bd43_5"},
      {0x000A0578, "mem_dma_bd43_6"},
      {0x000A057C, "mem_dma_bd43_7"},
      {0x000A0580, "mem_dma_bd44_0"},
      {0x000A0584, "mem_dma_bd44_1"},
      {0x000A0588, "mem_dma_bd44_2"},
      {0x000A058C, "mem_dma_bd44_3"},
      {0x000A0590, "mem_dma_bd44_4"},
      {0x000A0594, "mem_dma_bd44_5"},
      {0x000A0598, "mem_dma_bd44_6"},
      {0x000A059C, "mem_dma_bd44_7"},
      {0x000A05A0, "mem_dma_bd45_0"},
      {0x000A05A4, "mem_dma_bd45_1"},
      {0x000A05A8, "mem_dma_bd45_2"},
      {0x000A05AC, "mem_dma_bd45_3"},
      {0x000A05B0, "mem_dma_bd45_4"},
      {0x000A05B4, "mem_dma_bd45_5"},
      {0x000A05B8, "mem_dma_bd45_6"},
      {0x000A05BC, "mem_dma_bd45_7"},
      {0x000A05C0, "mem_dma_bd46_0"},
      {0x000A05C4, "mem_dma_bd46_1"},
      {0x000A05C8, "mem_dma_bd46_2"},
      {0x000A05CC, "mem_dma_bd46_3"},
      {0x000A05D0, "mem_dma_bd46_4"},
      {0x000A05D4, "mem_dma_bd46_5"},
      {0x000A05D8, "mem_dma_bd46_6"},
      {0x000A05DC, "mem_dma_bd46_7"},
      {0x000A05E0, "mem_dma_bd47_0"},
      {0x000A05E4, "mem_dma_bd47_1"},
      {0x000A05E8, "mem_dma_bd47_2"},
      {0x000A05EC, "mem_dma_bd47_3"},
      {0x000A05F0, "mem_dma_bd47_4"},
      {0x000A05F4, "mem_dma_bd47_5"},
      {0x000A05F8, "mem_dma_bd47_6"},
      {0x000A05FC, "mem_dma_bd47_7"},
      {0x000A0080, "mem_dma_bd4_0"},
      {0x000A0084, "mem_dma_bd4_1"},
      {0x000A0088, "mem_dma_bd4_2"},
      {0x000A008C, "mem_dma_bd4_3"},
      {0x000A0090, "mem_dma_bd4_4"},
      {0x000A0094, "mem_dma_bd4_5"},
      {0x000A0098, "mem_dma_bd4_6"},
      {0x000A009C, "mem_dma_bd4_7"},
      {0x000A00A0, "mem_dma_bd5_0"},
      {0x000A00A4, "mem_dma_bd5_1"},
      {0x000A00A8, "mem_dma_bd5_2"},
      {0x000A00AC, "mem_dma_bd5_3"},
      {0x000A00B0, "mem_dma_bd5_4"},
      {0x000A00B4, "mem_dma_bd5_5"},
      {0x000A00B8, "mem_dma_bd5_6"},
      {0x000A00BC, "mem_dma_bd5_7"},
      {0x000A00C0, "mem_dma_bd6_0"},
      {0x000A00C4, "mem_dma_bd6_1"},
      {0x000A00C8, "mem_dma_bd6_2"},
      {0x000A00CC, "mem_dma_bd6_3"},
      {0x000A00D0, "mem_dma_bd6_4"},
      {0x000A00D4, "mem_dma_bd6_5"},
      {0x000A00D8, "mem_dma_bd6_6"},
      {0x000A00DC, "mem_dma_bd6_7"},
      {0x000A00E0, "mem_dma_bd7_0"},
      {0x000A00E4, "mem_dma_bd7_1"},
      {0x000A00E8, "mem_dma_bd7_2"},
      {0x000A00EC, "mem_dma_bd7_3"},
      {0x000A00F0, "mem_dma_bd7_4"},
      {0x000A00F4, "mem_dma_bd7_5"},
      {0x000A00F8, "mem_dma_bd7_6"},
      {0x000A00FC, "mem_dma_bd7_7"},
      {0x000A0100, "mem_dma_bd8_0"},
      {0x000A0104, "mem_dma_bd8_1"},
      {0x000A0108, "mem_dma_bd8_2"},
      {0x000A010C, "mem_dma_bd8_3"},
      {0x000A0110, "mem_dma_bd8_4"},
      {0x000A0114, "mem_dma_bd8_5"},
      {0x000A0118, "mem_dma_bd8_6"},
      {0x000A011C, "mem_dma_bd8_7"},
      {0x000A0120, "mem_dma_bd9_0"},
      {0x000A0124, "mem_dma_bd9_1"},
      {0x000A0128, "mem_dma_bd9_2"},
      {0x000A012C, "mem_dma_bd9_3"},
      {0x000A0130, "mem_dma_bd9_4"},
      {0x000A0134, "mem_dma_bd9_5"},
      {0x000A0138, "mem_dma_bd9_6"},
      {0x000A013C, "mem_dma_bd9_7"},
      {0x000A06A0, "mem_dma_event_channel_selection"},
      {0x000A0630, "mem_dma_mm2s_0_ctrl"},
      {0x000A0634, "mem_dma_mm2s_0_start_queue"},
      {0x000A0638, "mem_dma_mm2s_1_ctrl"},
      {0x000A063C, "mem_dma_mm2s_1_start_queue"},
      {0x000A0640, "mem_dma_mm2s_2_ctrl"},
      {0x000A0644, "mem_dma_mm2s_2_start_queue"},
      {0x000A0648, "mem_dma_mm2s_3_ctrl"},
      {0x000A064C, "mem_dma_mm2s_3_start_queue"},
      {0x000A0650, "mem_dma_mm2s_4_ctrl"},
      {0x000A0654, "mem_dma_mm2s_4_start_queue"},
      {0x000A0658, "mem_dma_mm2s_5_ctrl"},
      {0x000A065C, "mem_dma_mm2s_5_start_queue"},
      {0x000A0680, "mem_dma_mm2s_status_0"},
      {0x000A0684, "mem_dma_mm2s_status_1"},
      {0x000A0688, "mem_dma_mm2s_status_2"},
      {0x000A068C, "mem_dma_mm2s_status_3"},
      {0x000A0690, "mem_dma_mm2s_status_4"},
      {0x000A0694, "mem_dma_mm2s_status_5"},
      {0x000A0600, "mem_dma_s2mm_0_ctrl"},
      {0x000A0604, "mem_dma_s2mm_0_start_queue"},
      {0x000A0608, "mem_dma_s2mm_1_ctrl"},
      {0x000A060C, "mem_dma_s2mm_1_start_queue"},
      {0x000A0610, "mem_dma_s2mm_2_ctrl"},
      {0x000A0614, "mem_dma_s2mm_2_start_queue"},
      {0x000A0618, "mem_dma_s2mm_3_ctrl"},
      {0x000A061C, "mem_dma_s2mm_3_start_queue"},
      {0x000A0620, "mem_dma_s2mm_4_ctrl"},
      {0x000A0624, "mem_dma_s2mm_4_start_queue"},
      {0x000A0628, "mem_dma_s2mm_5_ctrl"},
      {0x000A062C, "mem_dma_s2mm_5_start_queue"},
      {0x000A06B0, "mem_dma_s2mm_current_write_count_0"},
      {0x000A06B4, "mem_dma_s2mm_current_write_count_1"},
      {0x000A06B8, "mem_dma_s2mm_current_write_count_2"},
      {0x000A06BC, "mem_dma_s2mm_current_write_count_3"},
      {0x000A06C0, "mem_dma_s2mm_current_write_count_4"},
      {0x000A06C4, "mem_dma_s2mm_current_write_count_5"},
      {0x000A06C8, "mem_dma_s2mm_fot_count_fifo_pop_0"},
      {0x000A06CC, "mem_dma_s2mm_fot_count_fifo_pop_1"},
      {0x000A06D0, "mem_dma_s2mm_fot_count_fifo_pop_2"},
      {0x000A06D4, "mem_dma_s2mm_fot_count_fifo_pop_3"},
      {0x000A06D8, "mem_dma_s2mm_fot_count_fifo_pop_4"},
      {0x000A06DC, "mem_dma_s2mm_fot_count_fifo_pop_5"},
      {0x000A0660, "mem_dma_s2mm_status_0"},
      {0x000A0664, "mem_dma_s2mm_status_1"},
      {0x000A0668, "mem_dma_s2mm_status_2"},
      {0x000A066C, "mem_dma_s2mm_status_3"},
      {0x000A0670, "mem_dma_s2mm_status_4"},
      {0x000A0674, "mem_dma_s2mm_status_5"},
      {0x00092120, "mem_ecc_failing_address"},
      {0x00092110, "mem_ecc_scrubbing_event"},
      {0x00094408, "mem_edge_detection_event_control"},
      {0x00094010, "mem_event_broadcast0"},
      {0x00094014, "mem_event_broadcast1"},
      {0x00094038, "mem_event_broadcast10"},
      {0x0009403C, "mem_event_broadcast11"},
      {0x00094040, "mem_event_broadcast12"},
      {0x00094044, "mem_event_broadcast13"},
      {0x00094048, "mem_event_broadcast14"},
      {0x0009404C, "mem_event_broadcast15"},
      {0x00094018, "mem_event_broadcast2"},
      {0x0009401C, "mem_event_broadcast3"},
      {0x00094020, "mem_event_broadcast4"},
      {0x00094024, "mem_event_broadcast5"},
      {0x00094028, "mem_event_broadcast6"},
      {0x0009402C, "mem_event_broadcast7"},
      {0x00094030, "mem_event_broadcast8"},
      {0x00094034, "mem_event_broadcast9"},
      {0x00094084, "mem_event_broadcast_a_block_east_clr"},
      {0x00094080, "mem_event_broadcast_a_block_east_set"},
      {0x00094088, "mem_event_broadcast_a_block_east_value"},
      {0x00094074, "mem_event_broadcast_a_block_north_clr"},
      {0x00094070, "mem_event_broadcast_a_block_north_set"},
      {0x00094078, "mem_event_broadcast_a_block_north_value"},
      {0x00094054, "mem_event_broadcast_a_block_south_clr"},
      {0x00094050, "mem_event_broadcast_a_block_south_set"},
      {0x00094058, "mem_event_broadcast_a_block_south_value"},
      {0x00094064, "mem_event_broadcast_a_block_west_clr"},
      {0x00094060, "mem_event_broadcast_a_block_west_set"},
      {0x00094068, "mem_event_broadcast_a_block_west_value"},
      {0x000940C4, "mem_event_broadcast_b_block_east_clr"},
      {0x000940C0, "mem_event_broadcast_b_block_east_set"},
      {0x000940C8, "mem_event_broadcast_b_block_east_value"},
      {0x000940B4, "mem_event_broadcast_b_block_north_clr"},
      {0x000940B0, "mem_event_broadcast_b_block_north_set"},
      {0x000940B8, "mem_event_broadcast_b_block_north_value"},
      {0x00094094, "mem_event_broadcast_b_block_south_clr"},
      {0x00094090, "mem_event_broadcast_b_block_south_set"},
      {0x00094098, "mem_event_broadcast_b_block_south_value"},
      {0x000940A4, "mem_event_broadcast_b_block_west_clr"},
      {0x000940A0, "mem_event_broadcast_b_block_west_set"},
      {0x000940A8, "mem_event_broadcast_b_block_west_value"},
      {0x00094008, "mem_event_generate"},
      {0x00094500, "mem_event_group_0_enable"},
      {0x0009451C, "mem_event_group_broadcast_enable"},
      {0x00094508, "mem_event_group_dma_enable"},
      {0x00094518, "mem_event_group_error_enable"},
      {0x0009450C, "mem_event_group_lock_enable"},
      {0x00094514, "mem_event_group_memory_conflict_enable"},
      {0x00094510, "mem_event_group_stream_switch_enable"},
      {0x00094520, "mem_event_group_user_event_enable"},
      {0x00094504, "mem_event_group_watchpoint_enable"},
      {0x00094200, "mem_event_status0"},
      {0x00094204, "mem_event_status1"},
      {0x00094208, "mem_event_status2"},
      {0x0009420C, "mem_event_status3"},
      {0x00094210, "mem_event_status4"},
      {0x00094214, "mem_event_status5"},
      {0x000C0000, "mem_lock0_value"},
      {0x000C00A0, "mem_lock10_value"},
      {0x000C00B0, "mem_lock11_value"},
      {0x000C00C0, "mem_lock12_value"},
      {0x000C00D0, "mem_lock13_value"},
      {0x000C00E0, "mem_lock14_value"},
      {0x000C00F0, "mem_lock15_value"},
      {0x000C0100, "mem_lock16_value"},
      {0x000C0110, "mem_lock17_value"},
      {0x000C0120, "mem_lock18_value"},
      {0x000C0130, "mem_lock19_value"},
      {0x000C0010, "mem_lock1_value"},
      {0x000C0140, "mem_lock20_value"},
      {0x000C0150, "mem_lock21_value"},
      {0x000C0160, "mem_lock22_value"},
      {0x000C0170, "mem_lock23_value"},
      {0x000C0180, "mem_lock24_value"},
      {0x000C0190, "mem_lock25_value"},
      {0x000C01A0, "mem_lock26_value"},
      {0x000C01B0, "mem_lock27_value"},
      {0x000C01C0, "mem_lock28_value"},
      {0x000C01D0, "mem_lock29_value"},
      {0x000C0020, "mem_lock2_value"},
      {0x000C01E0, "mem_lock30_value"},
      {0x000C01F0, "mem_lock31_value"},
      {0x000C0200, "mem_lock32_value"},
      {0x000C0210, "mem_lock33_value"},
      {0x000C0220, "mem_lock34_value"},
      {0x000C0230, "mem_lock35_value"},
      {0x000C0240, "mem_lock36_value"},
      {0x000C0250, "mem_lock37_value"},
      {0x000C0260, "mem_lock38_value"},
      {0x000C0270, "mem_lock39_value"},
      {0x000C0030, "mem_lock3_value"},
      {0x000C0280, "mem_lock40_value"},
      {0x000C0290, "mem_lock41_value"},
      {0x000C02A0, "mem_lock42_value"},
      {0x000C02B0, "mem_lock43_value"},
      {0x000C02C0, "mem_lock44_value"},
      {0x000C02D0, "mem_lock45_value"},
      {0x000C02E0, "mem_lock46_value"},
      {0x000C02F0, "mem_lock47_value"},
      {0x000C0300, "mem_lock48_value"},
      {0x000C0310, "mem_lock49_value"},
      {0x000C0040, "mem_lock4_value"},
      {0x000C0320, "mem_lock50_value"},
      {0x000C0330, "mem_lock51_value"},
      {0x000C0340, "mem_lock52_value"},
      {0x000C0350, "mem_lock53_value"},
      {0x000C0360, "mem_lock54_value"},
      {0x000C0370, "mem_lock55_value"},
      {0x000C0380, "mem_lock56_value"},
      {0x000C0390, "mem_lock57_value"},
      {0x000C03A0, "mem_lock58_value"},
      {0x000C03B0, "mem_lock59_value"},
      {0x000C0050, "mem_lock5_value"},
      {0x000C03C0, "mem_lock60_value"},
      {0x000C03D0, "mem_lock61_value"},
      {0x000C03E0, "mem_lock62_value"},
      {0x000C03F0, "mem_lock63_value"},
      {0x000C0060, "mem_lock6_value"},
      {0x000C0070, "mem_lock7_value"},
      {0x000C0080, "mem_lock8_value"},
      {0x000C0090, "mem_lock9_value"},
      {0x000D0000, "mem_lock_request"},
      {0x000C0400, "mem_locks_event_selection_0"},
      {0x000C0404, "mem_locks_event_selection_1"},
      {0x000C0408, "mem_locks_event_selection_2"},
      {0x000C040C, "mem_locks_event_selection_3"},
      {0x000C0410, "mem_locks_event_selection_4"},
      {0x000C0414, "mem_locks_event_selection_5"},
      {0x000C0418, "mem_locks_event_selection_6"},
      {0x000C041C, "mem_locks_event_selection_7"},
      {0x000C0420, "mem_locks_overflow_0"},
      {0x000C0424, "mem_locks_overflow_1"},
      {0x000C0428, "mem_locks_underflow_0"},
      {0x000C042C, "mem_locks_underflow_1"},
      {0x00096048, "mem_memory_control"},
      {0x000FFF00, "mem_module_clock_control"},
      {0x000FFF10, "mem_module_reset_control"},
      {0x00091000, "mem_performance_control0"},
      {0x00091004, "mem_performance_control1"},
      {0x00091008, "mem_performance_control2"},
      {0x00091020, "mem_performance_counter0"},
      {0x00091080, "mem_performance_counter0_event_value"},
      {0x00091024, "mem_performance_counter1"},
      {0x00091084, "mem_performance_counter1_event_value"},
      {0x00091028, "mem_performance_counter2"},
      {0x00091088, "mem_performance_counter2_event_value"},
      {0x0009102C, "mem_performance_counter3"},
      {0x0009108C, "mem_performance_counter3_event_value"},
      {0x00094220, "mem_reserved0"},
      {0x00094224, "mem_reserved1"},
      {0x00094228, "mem_reserved2"},
      {0x0009422C, "mem_reserved3"},
      {0x00096000, "mem_spare_reg"},
      {0x000B0F38, "mem_stream_switch_adaptive_clock_gate_abort_period"},
      {0x000B0F34, "mem_stream_switch_adaptive_clock_gate_status"},
      {0x000B0808, "mem_stream_switch_deterministic_merge_arb0_ctrl"},
      {0x000B0800, "mem_stream_switch_deterministic_merge_arb0_slave0_1"},
      {0x000B0804, "mem_stream_switch_deterministic_merge_arb0_slave2_3"},
      {0x000B0818, "mem_stream_switch_deterministic_merge_arb1_ctrl"},
      {0x000B0810, "mem_stream_switch_deterministic_merge_arb1_slave0_1"},
      {0x000B0814, "mem_stream_switch_deterministic_merge_arb1_slave2_3"},
      {0x000B0F00, "mem_stream_switch_event_port_selection_0"},
      {0x000B0F04, "mem_stream_switch_event_port_selection_1"},
      {0x000B0000, "mem_stream_switch_master_config_dma0"},
      {0x000B0004, "mem_stream_switch_master_config_dma1"},
      {0x000B0008, "mem_stream_switch_master_config_dma2"},
      {0x000B000C, "mem_stream_switch_master_config_dma3"},
      {0x000B0010, "mem_stream_switch_master_config_dma4"},
      {0x000B0014, "mem_stream_switch_master_config_dma5"},
      {0x000B002C, "mem_stream_switch_master_config_north0"},
      {0x000B0030, "mem_stream_switch_master_config_north1"},
      {0x000B0034, "mem_stream_switch_master_config_north2"},
      {0x000B0038, "mem_stream_switch_master_config_north3"},
      {0x000B003C, "mem_stream_switch_master_config_north4"},
      {0x000B0040, "mem_stream_switch_master_config_north5"},
      {0x000B001C, "mem_stream_switch_master_config_south0"},
      {0x000B0020, "mem_stream_switch_master_config_south1"},
      {0x000B0024, "mem_stream_switch_master_config_south2"},
      {0x000B0028, "mem_stream_switch_master_config_south3"},
      {0x000B0018, "mem_stream_switch_master_config_tile_ctrl"},
      {0x000B0F20, "mem_stream_switch_parity_injection"},
      {0x000B0F10, "mem_stream_switch_parity_status"},
      {0x000B0100, "mem_stream_switch_slave_config_dma_0"},
      {0x000B0104, "mem_stream_switch_slave_config_dma_1"},
      {0x000B0108, "mem_stream_switch_slave_config_dma_2"},
      {0x000B010C, "mem_stream_switch_slave_config_dma_3"},
      {0x000B0110, "mem_stream_switch_slave_config_dma_4"},
      {0x000B0114, "mem_stream_switch_slave_config_dma_5"},
      {0x000B0134, "mem_stream_switch_slave_config_north_0"},
      {0x000B0138, "mem_stream_switch_slave_config_north_1"},
      {0x000B013C, "mem_stream_switch_slave_config_north_2"},
      {0x000B0140, "mem_stream_switch_slave_config_north_3"},
      {0x000B011C, "mem_stream_switch_slave_config_south_0"},
      {0x000B0120, "mem_stream_switch_slave_config_south_1"},
      {0x000B0124, "mem_stream_switch_slave_config_south_2"},
      {0x000B0128, "mem_stream_switch_slave_config_south_3"},
      {0x000B012C, "mem_stream_switch_slave_config_south_4"},
      {0x000B0130, "mem_stream_switch_slave_config_south_5"},
      {0x000B0118, "mem_stream_switch_slave_config_tile_ctrl"},
      {0x000B0144, "mem_stream_switch_slave_config_trace"},
      {0x000B0200, "mem_stream_switch_slave_dma_0_slot0"},
      {0x000B0204, "mem_stream_switch_slave_dma_0_slot1"},
      {0x000B0208, "mem_stream_switch_slave_dma_0_slot2"},
      {0x000B020C, "mem_stream_switch_slave_dma_0_slot3"},
      {0x000B0210, "mem_stream_switch_slave_dma_1_slot0"},
      {0x000B0214, "mem_stream_switch_slave_dma_1_slot1"},
      {0x000B0218, "mem_stream_switch_slave_dma_1_slot2"},
      {0x000B021C, "mem_stream_switch_slave_dma_1_slot3"},
      {0x000B0220, "mem_stream_switch_slave_dma_2_slot0"},
      {0x000B0224, "mem_stream_switch_slave_dma_2_slot1"},
      {0x000B0228, "mem_stream_switch_slave_dma_2_slot2"},
      {0x000B022C, "mem_stream_switch_slave_dma_2_slot3"},
      {0x000B0230, "mem_stream_switch_slave_dma_3_slot0"},
      {0x000B0234, "mem_stream_switch_slave_dma_3_slot1"},
      {0x000B0238, "mem_stream_switch_slave_dma_3_slot2"},
      {0x000B023C, "mem_stream_switch_slave_dma_3_slot3"},
      {0x000B0240, "mem_stream_switch_slave_dma_4_slot0"},
      {0x000B0244, "mem_stream_switch_slave_dma_4_slot1"},
      {0x000B0248, "mem_stream_switch_slave_dma_4_slot2"},
      {0x000B024C, "mem_stream_switch_slave_dma_4_slot3"},
      {0x000B0250, "mem_stream_switch_slave_dma_5_slot0"},
      {0x000B0254, "mem_stream_switch_slave_dma_5_slot1"},
      {0x000B0258, "mem_stream_switch_slave_dma_5_slot2"},
      {0x000B025C, "mem_stream_switch_slave_dma_5_slot3"},
      {0x000B02D0, "mem_stream_switch_slave_north_0_slot0"},
      {0x000B02D4, "mem_stream_switch_slave_north_0_slot1"},
      {0x000B02D8, "mem_stream_switch_slave_north_0_slot2"},
      {0x000B02DC, "mem_stream_switch_slave_north_0_slot3"},
      {0x000B02E0, "mem_stream_switch_slave_north_1_slot0"},
      {0x000B02E4, "mem_stream_switch_slave_north_1_slot1"},
      {0x000B02E8, "mem_stream_switch_slave_north_1_slot2"},
      {0x000B02EC, "mem_stream_switch_slave_north_1_slot3"},
      {0x000B02F0, "mem_stream_switch_slave_north_2_slot0"},
      {0x000B02F4, "mem_stream_switch_slave_north_2_slot1"},
      {0x000B02F8, "mem_stream_switch_slave_north_2_slot2"},
      {0x000B02FC, "mem_stream_switch_slave_north_2_slot3"},
      {0x000B0300, "mem_stream_switch_slave_north_3_slot0"},
      {0x000B0304, "mem_stream_switch_slave_north_3_slot1"},
      {0x000B0308, "mem_stream_switch_slave_north_3_slot2"},
      {0x000B030C, "mem_stream_switch_slave_north_3_slot3"},
      {0x000B0270, "mem_stream_switch_slave_south_0_slot0"},
      {0x000B0274, "mem_stream_switch_slave_south_0_slot1"},
      {0x000B0278, "mem_stream_switch_slave_south_0_slot2"},
      {0x000B027C, "mem_stream_switch_slave_south_0_slot3"},
      {0x000B0280, "mem_stream_switch_slave_south_1_slot0"},
      {0x000B0284, "mem_stream_switch_slave_south_1_slot1"},
      {0x000B0288, "mem_stream_switch_slave_south_1_slot2"},
      {0x000B028C, "mem_stream_switch_slave_south_1_slot3"},
      {0x000B0290, "mem_stream_switch_slave_south_2_slot0"},
      {0x000B0294, "mem_stream_switch_slave_south_2_slot1"},
      {0x000B0298, "mem_stream_switch_slave_south_2_slot2"},
      {0x000B029C, "mem_stream_switch_slave_south_2_slot3"},
      {0x000B02A0, "mem_stream_switch_slave_south_3_slot0"},
      {0x000B02A4, "mem_stream_switch_slave_south_3_slot1"},
      {0x000B02A8, "mem_stream_switch_slave_south_3_slot2"},
      {0x000B02AC, "mem_stream_switch_slave_south_3_slot3"},
      {0x000B02B0, "mem_stream_switch_slave_south_4_slot0"},
      {0x000B02B4, "mem_stream_switch_slave_south_4_slot1"},
      {0x000B02B8, "mem_stream_switch_slave_south_4_slot2"},
      {0x000B02BC, "mem_stream_switch_slave_south_4_slot3"},
      {0x000B02C0, "mem_stream_switch_slave_south_5_slot0"},
      {0x000B02C4, "mem_stream_switch_slave_south_5_slot1"},
      {0x000B02C8, "mem_stream_switch_slave_south_5_slot2"},
      {0x000B02CC, "mem_stream_switch_slave_south_5_slot3"},
      {0x000B0260, "mem_stream_switch_slave_tile_ctrl_slot0"},
      {0x000B0264, "mem_stream_switch_slave_tile_ctrl_slot1"},
      {0x000B0268, "mem_stream_switch_slave_tile_ctrl_slot2"},
      {0x000B026C, "mem_stream_switch_slave_tile_ctrl_slot3"},
      {0x000B0310, "mem_stream_switch_slave_trace_slot0"},
      {0x000B0314, "mem_stream_switch_slave_trace_slot1"},
      {0x000B0318, "mem_stream_switch_slave_trace_slot2"},
      {0x000B031C, "mem_stream_switch_slave_trace_slot3"},
      {0x00096030, "mem_tile_control"},
      {0x000B0F30, "mem_tile_control_packet_handler_status"},
      {0x00094000, "mem_timer_control"},
      {0x000940FC, "mem_timer_high"},
      {0x000940F8, "mem_timer_low"},
      {0x000940F4, "mem_timer_trig_event_high_value"},
      {0x000940F0, "mem_timer_trig_event_low_value"},
      {0x000940D0, "mem_trace_control0"},
      {0x000940D4, "mem_trace_control1"},
      {0x000940E0, "mem_trace_event0"},
      {0x000940E4, "mem_trace_event1"},
      {0x000940D8, "mem_trace_status"},
      {0x00094100, "mem_watchpoint0"},
      {0x00094104, "mem_watchpoint1"},
      {0x00094108, "mem_watchpoint2"},
      {0x0009410C, "mem_watchpoint3"}
   };

   // shim_tile_module registers
   shimRegValueToName = {
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
      {0x000FFF24, "shim_cssd_trigger"},
      {0x000FFF20, "shim_column_clock_control"},
      {0x000FFF28, "shim_column_reset_control"},
      {0x00034404, "shim_combo_event_control"},
      {0x00034400, "shim_combo_event_inputs"},
      {0x0003FF30, "shim_control_packet_handler_status"},
      {0x0001D000, "shim_dma_bd0_0"},
      {0x0001D004, "shim_dma_bd0_1"},
      {0x0001D008, "shim_dma_bd0_2"},
      {0x0001D00C, "shim_dma_bd0_3"},
      {0x0001D010, "shim_dma_bd0_4"},
      {0x0001D014, "shim_dma_bd0_5"},
      {0x0001D018, "shim_dma_bd0_6"},
      {0x0001D01C, "shim_dma_bd0_7"},
      {0x0001D140, "shim_dma_bd10_0"},
      {0x0001D144, "shim_dma_bd10_1"},
      {0x0001D148, "shim_dma_bd10_2"},
      {0x0001D14C, "shim_dma_bd10_3"},
      {0x0001D150, "shim_dma_bd10_4"},
      {0x0001D154, "shim_dma_bd10_5"},
      {0x0001D158, "shim_dma_bd10_6"},
      {0x0001D15C, "shim_dma_bd10_7"},
      {0x0001D160, "shim_dma_bd11_0"},
      {0x0001D164, "shim_dma_bd11_1"},
      {0x0001D168, "shim_dma_bd11_2"},
      {0x0001D16C, "shim_dma_bd11_3"},
      {0x0001D170, "shim_dma_bd11_4"},
      {0x0001D174, "shim_dma_bd11_5"},
      {0x0001D178, "shim_dma_bd11_6"},
      {0x0001D17C, "shim_dma_bd11_7"},
      {0x0001D180, "shim_dma_bd12_0"},
      {0x0001D184, "shim_dma_bd12_1"},
      {0x0001D188, "shim_dma_bd12_2"},
      {0x0001D18C, "shim_dma_bd12_3"},
      {0x0001D190, "shim_dma_bd12_4"},
      {0x0001D194, "shim_dma_bd12_5"},
      {0x0001D198, "shim_dma_bd12_6"},
      {0x0001D19C, "shim_dma_bd12_7"},
      {0x0001D1A0, "shim_dma_bd13_0"},
      {0x0001D1A4, "shim_dma_bd13_1"},
      {0x0001D1A8, "shim_dma_bd13_2"},
      {0x0001D1AC, "shim_dma_bd13_3"},
      {0x0001D1B0, "shim_dma_bd13_4"},
      {0x0001D1B4, "shim_dma_bd13_5"},
      {0x0001D1B8, "shim_dma_bd13_6"},
      {0x0001D1BC, "shim_dma_bd13_7"},
      {0x0001D1C0, "shim_dma_bd14_0"},
      {0x0001D1C4, "shim_dma_bd14_1"},
      {0x0001D1C8, "shim_dma_bd14_2"},
      {0x0001D1CC, "shim_dma_bd14_3"},
      {0x0001D1D0, "shim_dma_bd14_4"},
      {0x0001D1D4, "shim_dma_bd14_5"},
      {0x0001D1D8, "shim_dma_bd14_6"},
      {0x0001D1DC, "shim_dma_bd14_7"},
      {0x0001D1E0, "shim_dma_bd15_0"},
      {0x0001D1E4, "shim_dma_bd15_1"},
      {0x0001D1E8, "shim_dma_bd15_2"},
      {0x0001D1EC, "shim_dma_bd15_3"},
      {0x0001D1F0, "shim_dma_bd15_4"},
      {0x0001D1F4, "shim_dma_bd15_5"},
      {0x0001D1F8, "shim_dma_bd15_6"},
      {0x0001D1FC, "shim_dma_bd15_7"},
      {0x0001D020, "shim_dma_bd1_0"},
      {0x0001D024, "shim_dma_bd1_1"},
      {0x0001D028, "shim_dma_bd1_2"},
      {0x0001D02C, "shim_dma_bd1_3"},
      {0x0001D030, "shim_dma_bd1_4"},
      {0x0001D034, "shim_dma_bd1_5"},
      {0x0001D038, "shim_dma_bd1_6"},
      {0x0001D03C, "shim_dma_bd1_7"},
      {0x0001D040, "shim_dma_bd2_0"},
      {0x0001D044, "shim_dma_bd2_1"},
      {0x0001D048, "shim_dma_bd2_2"},
      {0x0001D04C, "shim_dma_bd2_3"},
      {0x0001D050, "shim_dma_bd2_4"},
      {0x0001D054, "shim_dma_bd2_5"},
      {0x0001D058, "shim_dma_bd2_6"},
      {0x0001D05C, "shim_dma_bd2_7"},
      {0x0001D060, "shim_dma_bd3_0"},
      {0x0001D064, "shim_dma_bd3_1"},
      {0x0001D068, "shim_dma_bd3_2"},
      {0x0001D06C, "shim_dma_bd3_3"},
      {0x0001D070, "shim_dma_bd3_4"},
      {0x0001D074, "shim_dma_bd3_5"},
      {0x0001D078, "shim_dma_bd3_6"},
      {0x0001D07C, "shim_dma_bd3_7"},
      {0x0001D080, "shim_dma_bd4_0"},
      {0x0001D084, "shim_dma_bd4_1"},
      {0x0001D088, "shim_dma_bd4_2"},
      {0x0001D08C, "shim_dma_bd4_3"},
      {0x0001D090, "shim_dma_bd4_4"},
      {0x0001D094, "shim_dma_bd4_5"},
      {0x0001D098, "shim_dma_bd4_6"},
      {0x0001D09C, "shim_dma_bd4_7"},
      {0x0001D0A0, "shim_dma_bd5_0"},
      {0x0001D0A4, "shim_dma_bd5_1"},
      {0x0001D0A8, "shim_dma_bd5_2"},
      {0x0001D0AC, "shim_dma_bd5_3"},
      {0x0001D0B0, "shim_dma_bd5_4"},
      {0x0001D0B4, "shim_dma_bd5_5"},
      {0x0001D0B8, "shim_dma_bd5_6"},
      {0x0001D0BC, "shim_dma_bd5_7"},
      {0x0001D0C0, "shim_dma_bd6_0"},
      {0x0001D0C4, "shim_dma_bd6_1"},
      {0x0001D0C8, "shim_dma_bd6_2"},
      {0x0001D0CC, "shim_dma_bd6_3"},
      {0x0001D0D0, "shim_dma_bd6_4"},
      {0x0001D0D4, "shim_dma_bd6_5"},
      {0x0001D0D8, "shim_dma_bd6_6"},
      {0x0001D0DC, "shim_dma_bd6_7"},
      {0x0001D0E0, "shim_dma_bd7_0"},
      {0x0001D0E4, "shim_dma_bd7_1"},
      {0x0001D0E8, "shim_dma_bd7_2"},
      {0x0001D0EC, "shim_dma_bd7_3"},
      {0x0001D0F0, "shim_dma_bd7_4"},
      {0x0001D0F4, "shim_dma_bd7_5"},
      {0x0001D0F8, "shim_dma_bd7_6"},
      {0x0001D0FC, "shim_dma_bd7_7"},
      {0x0001D100, "shim_dma_bd8_0"},
      {0x0001D104, "shim_dma_bd8_1"},
      {0x0001D108, "shim_dma_bd8_2"},
      {0x0001D10C, "shim_dma_bd8_3"},
      {0x0001D110, "shim_dma_bd8_4"},
      {0x0001D114, "shim_dma_bd8_5"},
      {0x0001D118, "shim_dma_bd8_6"},
      {0x0001D11C, "shim_dma_bd8_7"},
      {0x0001D120, "shim_dma_bd9_0"},
      {0x0001D124, "shim_dma_bd9_1"},
      {0x0001D128, "shim_dma_bd9_2"},
      {0x0001D12C, "shim_dma_bd9_3"},
      {0x0001D130, "shim_dma_bd9_4"},
      {0x0001D134, "shim_dma_bd9_5"},
      {0x0001D138, "shim_dma_bd9_6"},
      {0x0001D13C, "shim_dma_bd9_7"},
      {0x0001D210, "shim_dma_mm2s_0_ctrl"},
      {0x0001D214, "shim_dma_mm2s_0_task_queue"},
      {0x0001D218, "shim_dma_mm2s_1_ctrl"},
      {0x0001D21C, "shim_dma_mm2s_1_task_queue"},
      {0x0001D228, "shim_dma_mm2s_status_0"},
      {0x0001D22C, "shim_dma_mm2s_status_1"},
      {0x0001D200, "shim_dma_s2mm_0_ctrl"},
      {0x0001D204, "shim_dma_s2mm_0_task_queue"},
      {0x0001D208, "shim_dma_s2mm_1_ctrl"},
      {0x0001D20C, "shim_dma_s2mm_1_task_queue"},
      {0x0001D230, "shim_dma_s2mm_current_write_count_0"},
      {0x0001D234, "shim_dma_s2mm_current_write_count_1"},
      {0x0001D238, "shim_dma_s2mm_fot_count_fifo_pop_0"},
      {0x0001D23C, "shim_dma_s2mm_fot_count_fifo_pop_1"},
      {0x0001D220, "shim_dma_s2mm_status_0"},
      {0x0001D224, "shim_dma_s2mm_status_1"},
      {0x0001F004, "shim_demux_config"},
      {0x00034408, "shim_edge_detection_event_control"},
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
      {0x00014000, "shim_lock0_value"},
      {0x000140A0, "shim_lock10_value"},
      {0x000140B0, "shim_lock11_value"},
      {0x000140C0, "shim_lock12_value"},
      {0x000140D0, "shim_lock13_value"},
      {0x000140E0, "shim_lock14_value"},
      {0x000140F0, "shim_lock15_value"},
      {0x00014010, "shim_lock1_value"},
      {0x00014020, "shim_lock2_value"},
      {0x00014030, "shim_lock3_value"},
      {0x00014040, "shim_lock4_value"},
      {0x00014050, "shim_lock5_value"},
      {0x00014060, "shim_lock6_value"},
      {0x00014070, "shim_lock7_value"},
      {0x00014080, "shim_lock8_value"},
      {0x00014090, "shim_lock9_value"},
      {0x00040000, "shim_lock_request"},
      {0x00014100, "shim_locks_event_selection_0"},
      {0x00014104, "shim_locks_event_selection_1"},
      {0x00014108, "shim_locks_event_selection_2"},
      {0x0001410C, "shim_locks_event_selection_3"},
      {0x00014110, "shim_locks_event_selection_4"},
      {0x00014114, "shim_locks_event_selection_5"},
      {0x00014120, "shim_locks_overflow"},
      {0x00014128, "shim_locks_underflow"},
      {0x0001E020, "shim_me_aximm_config"},
      {0x000FFF00, "shim_module_clock_control_0"},
      {0x000FFF04, "shim_module_clock_control_1"},
      {0x000FFF10, "shim_module_reset_control_0"},
      {0x000FFF14, "shim_module_reset_control_1"},
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
      {0x000FFF30, "shim_spare_reg"},
      {0x0003FF38, "shim_stream_switch_adaptive_clock_gate_abort_period"},
      {0x0003FF34, "shim_stream_switch_adaptive_clock_gate_status"},
      {0x0003F808, "shim_stream_switch_deterministic_merge_arb0_ctrl"},
      {0x0003F800, "shim_stream_switch_deterministic_merge_arb0_slave0_1"},
      {0x0003F804, "shim_stream_switch_deterministic_merge_arb0_slave2_3"},
      {0x0003F818, "shim_stream_switch_deterministic_merge_arb1_ctrl"},
      {0x0003F810, "shim_stream_switch_deterministic_merge_arb1_slave0_1"},
      {0x0003F814, "shim_stream_switch_deterministic_merge_arb1_slave2_3"},
      {0x0003FF00, "shim_stream_switch_event_port_selection_0"},
      {0x0003FF04, "shim_stream_switch_event_port_selection_1"},
      {0x0003F048, "shim_stream_switch_master_config_east0"},
      {0x0003F04C, "shim_stream_switch_master_config_east1"},
      {0x0003F050, "shim_stream_switch_master_config_east2"},
      {0x0003F054, "shim_stream_switch_master_config_east3"},
      {0x0003F004, "shim_stream_switch_master_config_fifo0"},
      {0x0003F030, "shim_stream_switch_master_config_north0"},
      {0x0003F034, "shim_stream_switch_master_config_north1"},
      {0x0003F038, "shim_stream_switch_master_config_north2"},
      {0x0003F03C, "shim_stream_switch_master_config_north3"},
      {0x0003F040, "shim_stream_switch_master_config_north4"},
      {0x0003F044, "shim_stream_switch_master_config_north5"},
      {0x0003F008, "shim_stream_switch_master_config_south0"},
      {0x0003F00C, "shim_stream_switch_master_config_south1"},
      {0x0003F010, "shim_stream_switch_master_config_south2"},
      {0x0003F014, "shim_stream_switch_master_config_south3"},
      {0x0003F018, "shim_stream_switch_master_config_south4"},
      {0x0003F01C, "shim_stream_switch_master_config_south5"},
      {0x0003F000, "shim_stream_switch_master_config_tile_ctrl"},
      {0x0003F020, "shim_stream_switch_master_config_west0"},
      {0x0003F024, "shim_stream_switch_master_config_west1"},
      {0x0003F028, "shim_stream_switch_master_config_west2"},
      {0x0003F02C, "shim_stream_switch_master_config_west3"},
      {0x0003FF20, "shim_stream_switch_parity_injection"},
      {0x0003FF10, "shim_stream_switch_parity_status"},
      {0x0003F148, "shim_stream_switch_slave_config_east_0"},
      {0x0003F14C, "shim_stream_switch_slave_config_east_1"},
      {0x0003F150, "shim_stream_switch_slave_config_east_2"},
      {0x0003F154, "shim_stream_switch_slave_config_east_3"},
      {0x0003F104, "shim_stream_switch_slave_config_fifo_0"},
      {0x0003F138, "shim_stream_switch_slave_config_north_0"},
      {0x0003F13C, "shim_stream_switch_slave_config_north_1"},
      {0x0003F140, "shim_stream_switch_slave_config_north_2"},
      {0x0003F144, "shim_stream_switch_slave_config_north_3"},
      {0x0003F108, "shim_stream_switch_slave_config_south_0"},
      {0x0003F10C, "shim_stream_switch_slave_config_south_1"},
      {0x0003F110, "shim_stream_switch_slave_config_south_2"},
      {0x0003F114, "shim_stream_switch_slave_config_south_3"},
      {0x0003F118, "shim_stream_switch_slave_config_south_4"},
      {0x0003F11C, "shim_stream_switch_slave_config_south_5"},
      {0x0003F120, "shim_stream_switch_slave_config_south_6"},
      {0x0003F124, "shim_stream_switch_slave_config_south_7"},
      {0x0003F100, "shim_stream_switch_slave_config_tile_ctrl"},
      {0x0003F158, "shim_stream_switch_slave_config_trace"},
      {0x0003F128, "shim_stream_switch_slave_config_west_0"},
      {0x0003F12C, "shim_stream_switch_slave_config_west_1"},
      {0x0003F130, "shim_stream_switch_slave_config_west_2"},
      {0x0003F134, "shim_stream_switch_slave_config_west_3"},
      {0x0003F320, "shim_stream_switch_slave_east_0_slot0"},
      {0x0003F324, "shim_stream_switch_slave_east_0_slot1"},
      {0x0003F328, "shim_stream_switch_slave_east_0_slot2"},
      {0x0003F32C, "shim_stream_switch_slave_east_0_slot3"},
      {0x0003F330, "shim_stream_switch_slave_east_1_slot0"},
      {0x0003F334, "shim_stream_switch_slave_east_1_slot1"},
      {0x0003F338, "shim_stream_switch_slave_east_1_slot2"},
      {0x0003F33C, "shim_stream_switch_slave_east_1_slot3"},
      {0x0003F340, "shim_stream_switch_slave_east_2_slot0"},
      {0x0003F344, "shim_stream_switch_slave_east_2_slot1"},
      {0x0003F348, "shim_stream_switch_slave_east_2_slot2"},
      {0x0003F34C, "shim_stream_switch_slave_east_2_slot3"},
      {0x0003F350, "shim_stream_switch_slave_east_3_slot0"},
      {0x0003F354, "shim_stream_switch_slave_east_3_slot1"},
      {0x0003F358, "shim_stream_switch_slave_east_3_slot2"},
      {0x0003F35C, "shim_stream_switch_slave_east_3_slot3"},
      {0x0003F210, "shim_stream_switch_slave_fifo_0_slot0"},
      {0x0003F214, "shim_stream_switch_slave_fifo_0_slot1"},
      {0x0003F218, "shim_stream_switch_slave_fifo_0_slot2"},
      {0x0003F21C, "shim_stream_switch_slave_fifo_0_slot3"},
      {0x0003F2E0, "shim_stream_switch_slave_north_0_slot0"},
      {0x0003F2E4, "shim_stream_switch_slave_north_0_slot1"},
      {0x0003F2E8, "shim_stream_switch_slave_north_0_slot2"},
      {0x0003F2EC, "shim_stream_switch_slave_north_0_slot3"},
      {0x0003F2F0, "shim_stream_switch_slave_north_1_slot0"},
      {0x0003F2F4, "shim_stream_switch_slave_north_1_slot1"},
      {0x0003F2F8, "shim_stream_switch_slave_north_1_slot2"},
      {0x0003F2FC, "shim_stream_switch_slave_north_1_slot3"},
      {0x0003F300, "shim_stream_switch_slave_north_2_slot0"},
      {0x0003F304, "shim_stream_switch_slave_north_2_slot1"},
      {0x0003F308, "shim_stream_switch_slave_north_2_slot2"},
      {0x0003F30C, "shim_stream_switch_slave_north_2_slot3"},
      {0x0003F310, "shim_stream_switch_slave_north_3_slot0"},
      {0x0003F314, "shim_stream_switch_slave_north_3_slot1"},
      {0x0003F318, "shim_stream_switch_slave_north_3_slot2"},
      {0x0003F31C, "shim_stream_switch_slave_north_3_slot3"},
      {0x0003F220, "shim_stream_switch_slave_south_0_slot0"},
      {0x0003F224, "shim_stream_switch_slave_south_0_slot1"},
      {0x0003F228, "shim_stream_switch_slave_south_0_slot2"},
      {0x0003F22C, "shim_stream_switch_slave_south_0_slot3"},
      {0x0003F230, "shim_stream_switch_slave_south_1_slot0"},
      {0x0003F234, "shim_stream_switch_slave_south_1_slot1"},
      {0x0003F238, "shim_stream_switch_slave_south_1_slot2"},
      {0x0003F23C, "shim_stream_switch_slave_south_1_slot3"},
      {0x0003F240, "shim_stream_switch_slave_south_2_slot0"},
      {0x0003F244, "shim_stream_switch_slave_south_2_slot1"},
      {0x0003F248, "shim_stream_switch_slave_south_2_slot2"},
      {0x0003F24C, "shim_stream_switch_slave_south_2_slot3"},
      {0x0003F250, "shim_stream_switch_slave_south_3_slot0"},
      {0x0003F254, "shim_stream_switch_slave_south_3_slot1"},
      {0x0003F258, "shim_stream_switch_slave_south_3_slot2"},
      {0x0003F25C, "shim_stream_switch_slave_south_3_slot3"},
      {0x0003F260, "shim_stream_switch_slave_south_4_slot0"},
      {0x0003F264, "shim_stream_switch_slave_south_4_slot1"},
      {0x0003F268, "shim_stream_switch_slave_south_4_slot2"},
      {0x0003F26C, "shim_stream_switch_slave_south_4_slot3"},
      {0x0003F270, "shim_stream_switch_slave_south_5_slot0"},
      {0x0003F274, "shim_stream_switch_slave_south_5_slot1"},
      {0x0003F278, "shim_stream_switch_slave_south_5_slot2"},
      {0x0003F27C, "shim_stream_switch_slave_south_5_slot3"},
      {0x0003F280, "shim_stream_switch_slave_south_6_slot0"},
      {0x0003F284, "shim_stream_switch_slave_south_6_slot1"},
      {0x0003F288, "shim_stream_switch_slave_south_6_slot2"},
      {0x0003F28C, "shim_stream_switch_slave_south_6_slot3"},
      {0x0003F290, "shim_stream_switch_slave_south_7_slot0"},
      {0x0003F294, "shim_stream_switch_slave_south_7_slot1"},
      {0x0003F298, "shim_stream_switch_slave_south_7_slot2"},
      {0x0003F29C, "shim_stream_switch_slave_south_7_slot3"},
      {0x0003F200, "shim_stream_switch_slave_tile_ctrl_slot0"},
      {0x0003F204, "shim_stream_switch_slave_tile_ctrl_slot1"},
      {0x0003F208, "shim_stream_switch_slave_tile_ctrl_slot2"},
      {0x0003F20C, "shim_stream_switch_slave_tile_ctrl_slot3"},
      {0x0003F360, "shim_stream_switch_slave_trace_slot0"},
      {0x0003F364, "shim_stream_switch_slave_trace_slot1"},
      {0x0003F368, "shim_stream_switch_slave_trace_slot2"},
      {0x0003F36C, "shim_stream_switch_slave_trace_slot3"},
      {0x0003F2A0, "shim_stream_switch_slave_west_0_slot0"},
      {0x0003F2A4, "shim_stream_switch_slave_west_0_slot1"},
      {0x0003F2A8, "shim_stream_switch_slave_west_0_slot2"},
      {0x0003F2AC, "shim_stream_switch_slave_west_0_slot3"},
      {0x0003F2B0, "shim_stream_switch_slave_west_1_slot0"},
      {0x0003F2B4, "shim_stream_switch_slave_west_1_slot1"},
      {0x0003F2B8, "shim_stream_switch_slave_west_1_slot2"},
      {0x0003F2BC, "shim_stream_switch_slave_west_1_slot3"},
      {0x0003F2C0, "shim_stream_switch_slave_west_2_slot0"},
      {0x0003F2C4, "shim_stream_switch_slave_west_2_slot1"},
      {0x0003F2C8, "shim_stream_switch_slave_west_2_slot2"},
      {0x0003F2CC, "shim_stream_switch_slave_west_2_slot3"},
      {0x0003F2D0, "shim_stream_switch_slave_west_3_slot0"},
      {0x0003F2D4, "shim_stream_switch_slave_west_3_slot1"},
      {0x0003F2D8, "shim_stream_switch_slave_west_3_slot2"},
      {0x0003F2DC, "shim_stream_switch_slave_west_3_slot3"},
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
      {0x10, "shim_lock_step_size"},
      {0x20, "shim_dma_bd_step_size"},
      {0x8, "shim_dma_s2mm_step_size"}
   };
}

void AIE2UsedRegisters::populateRegAddrToSizeMap() {
   // core_module registers
   coreRegAddrToSize = {
        {0x00030000, 128},
        {0x00030010, 128},
        {0x00030020, 128},
        {0x00030030, 128},
        {0x00030040, 128},
        {0x00030050, 128},
        {0x00030060, 128},
        {0x00030070, 128},
        {0x00030080, 128},
        {0x00030090, 128},
        {0x000300A0, 128},
        {0x000300B0, 128},
        {0x000300C0, 128},
        {0x000300D0, 128},
        {0x000300E0, 128},
        {0x000300F0, 128},
        {0x00030100, 128},
        {0x00030110, 128},
        {0x00030120, 128},
        {0x00030130, 128},
        {0x00030140, 128},
        {0x00030150, 128},
        {0x00030160, 128},
        {0x00030170, 128},
        {0x00030180, 128},
        {0x00030190, 128},
        {0x000301A0, 128},
        {0x000301B0, 128},
        {0x000301C0, 128},
        {0x000301D0, 128},
        {0x000301E0, 128},
        {0x000301F0, 128},
        {0x00030200, 128},
        {0x00030210, 128},
        {0x00030220, 128},
        {0x00030230, 128},
        {0x00030240, 128},
        {0x00030250, 128},
        {0x00030260, 128},
        {0x00030270, 128},
        {0x00030280, 128},
        {0x00030290, 128},
        {0x000302A0, 128},
        {0x000302B0, 128},
        {0x000302C0, 128},
        {0x000302D0, 128},
        {0x000302E0, 128},
        {0x000302F0, 128},
        {0x00030300, 128},
        {0x00030310, 128},
        {0x00030320, 128},
        {0x00030330, 128},
        {0x00030340, 128},
        {0x00030350, 128},
        {0x00030360, 128},
        {0x00030370, 128},
        {0x00030380, 128},
        {0x00030390, 128},
        {0x000303A0, 128},
        {0x000303B0, 128},
        {0x000303C0, 128},
        {0x000303D0, 128},
        {0x000303E0, 128},
        {0x000303F0, 128},
        {0x00030400, 128},
        {0x00030410, 128},
        {0x00030420, 128},
        {0x00030430, 128},
        {0x00030440, 128},
        {0x00030450, 128},
        {0x00030460, 128},
        {0x00030470, 128},
        {0x00030480, 128},
        {0x00030490, 128},
        {0x000304A0, 128},
        {0x000304B0, 128},
        {0x000304C0, 128},
        {0x000304D0, 128},
        {0x000304E0, 128},
        {0x000304F0, 128},
        {0x00030500, 128},
        {0x00030510, 128},
        {0x00030520, 128},
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
        {0x000307A0, 128},
        {0x000307B0, 128},
        {0x000307C0, 128},
        {0x000307D0, 128},
        {0x000307E0, 128},
        {0x000307F0, 128},
        {0x00030800, 128},
        {0x00030810, 128},
        {0x00030820, 128},
        {0x00030830, 128},
        {0x00030840, 128},
        {0x00030850, 128},
        {0x00030860, 128},
        {0x00030870, 128},
        {0x00030880, 128},
        {0x00030890, 128},
        {0x000308A0, 128},
        {0x000308B0, 128},
        {0x000308C0, 128},
        {0x000308D0, 128},
        {0x000308E0, 128},
        {0x000308F0, 128},
        {0x00030900, 128},
        {0x00030910, 128},
        {0x00030920, 128},
        {0x00030930, 128},
        {0x00030940, 128},
        {0x00030950, 128},
        {0x00030960, 128},
        {0x00030970, 128},
        {0x00030980, 128},
        {0x00030990, 128},
        {0x000309A0, 128},
        {0x000309B0, 128},
        {0x000309C0, 128},
        {0x000309D0, 128},
        {0x000309E0, 128},
        {0x000309F0, 128},
        {0x00030A00, 128},
        {0x00030A10, 128},
        {0x00030A20, 128},
        {0x00030A30, 128},
        {0x00030A40, 128},
        {0x00030A50, 128},
        {0x00030A60, 128},
        {0x00030A70, 128},
        {0x00030A80, 128},
        {0x00030A90, 128},
        {0x00030AA0, 128},
        {0x00030AB0, 128},
        {0x00030AC0, 128},
        {0x00030AD0, 128},
        {0x00030AE0, 128},
        {0x00030AF0, 128},
        {0x00030B00, 128},
        {0x00030B10, 128},
        {0x00030B20, 128},
        {0x00030B30, 128},
        {0x00030B40, 128},
        {0x00030B50, 128},
        {0x00030B60, 128},
        {0x00030B70, 128},
        {0x00030B80, 128},
        {0x00030B90, 128},
        {0x00030BA0, 128},
        {0x00030BB0, 128},
        {0x00030BC0, 128},
        {0x00030BD0, 128},
        {0x00030BE0, 128},
        {0x00030BF0, 128},
        {0x000310C0, 128},
        {0x000310D0, 128},
        {0x000310E0, 128},
        {0x000310F0, 128}
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