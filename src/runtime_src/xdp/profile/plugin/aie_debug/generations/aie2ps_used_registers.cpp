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

#include "xdp/profile/plugin/aie_debug/generations/aie2ps_registers.h"
//#include "xdp/profile/plugin/aie_debug/aie_debug_metadata.h"
#include "xdp/profile/plugin/aie_debug/used_registers.h"

namespace xdp {

  void AIE2psUsedRegisters::populateProfileRegisters() {
    // Core modules
    core_addresses.emplace(aie2ps::cm_performance_control0);
    core_addresses.emplace(aie2ps::cm_performance_control1);
    core_addresses.emplace(aie2ps::cm_performance_control2);
    core_addresses.emplace(aie2ps::cm_performance_counter0);
    core_addresses.emplace(aie2ps::cm_performance_counter1);
    core_addresses.emplace(aie2ps::cm_performance_counter2);
    core_addresses.emplace(aie2ps::cm_performance_counter3);
    core_addresses.emplace(aie2ps::cm_performance_counter0_event_value);
    core_addresses.emplace(aie2ps::cm_performance_counter1_event_value);
    core_addresses.emplace(aie2ps::cm_performance_counter2_event_value);
    core_addresses.emplace(aie2ps::cm_performance_counter3_event_value);

    // Memory modules
    memory_addresses.emplace(aie2ps::mm_performance_control0);
    memory_addresses.emplace(aie2ps::mm_performance_control1);
    memory_addresses.emplace(aie2ps::mm_performance_control2);
    memory_addresses.emplace(aie2ps::mm_performance_control3);
    memory_addresses.emplace(aie2ps::mm_performance_counter0);
    memory_addresses.emplace(aie2ps::mm_performance_counter1);
    memory_addresses.emplace(aie2ps::mm_performance_counter2);
    memory_addresses.emplace(aie2ps::mm_performance_counter3);
    memory_addresses.emplace(aie2ps::mm_performance_counter0_event_value);
    memory_addresses.emplace(aie2ps::mm_performance_counter1_event_value);

    // Interface tiles
    interface_addresses.emplace(aie2ps::shim_performance_control0);
    interface_addresses.emplace(aie2ps::shim_performance_control1);
    interface_addresses.emplace(aie2ps::shim_performance_control2);
    interface_addresses.emplace(aie2ps::shim_performance_control3);
    interface_addresses.emplace(aie2ps::shim_performance_control4);
    interface_addresses.emplace(aie2ps::shim_performance_control5);
    interface_addresses.emplace(aie2ps::shim_performance_counter0);
    interface_addresses.emplace(aie2ps::shim_performance_counter1);
    interface_addresses.emplace(aie2ps::shim_performance_counter2);
    interface_addresses.emplace(aie2ps::shim_performance_counter3);
    interface_addresses.emplace(aie2ps::shim_performance_counter4);
    interface_addresses.emplace(aie2ps::shim_performance_counter5);
    interface_addresses.emplace(aie2ps::shim_performance_counter0_event_value);
    interface_addresses.emplace(aie2ps::shim_performance_counter1_event_value);

    // Memory tiles
    memory_tile_addresses.emplace(aie2ps::mem_performance_control0);
    memory_tile_addresses.emplace(aie2ps::mem_performance_control1);
    memory_tile_addresses.emplace(aie2ps::mem_performance_control2);
    memory_tile_addresses.emplace(aie2ps::mem_performance_control3);
    memory_tile_addresses.emplace(aie2ps::mem_performance_control4);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter0);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter1);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter2);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter3);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter4);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter5);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter0_event_value);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter1_event_value);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter2_event_value);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter3_event_value);
  }

  void AIE2psUsedRegisters::populateTraceRegisters() {
    // Core modules
    core_addresses.emplace(aie2ps::cm_core_status);
    core_addresses.emplace(aie2ps::cm_trace_control0);
    core_addresses.emplace(aie2ps::cm_trace_control1);
    core_addresses.emplace(aie2ps::cm_trace_status);
    core_addresses.emplace(aie2ps::cm_trace_event0);
    core_addresses.emplace(aie2ps::cm_trace_event1);
    core_addresses.emplace(aie2ps::cm_event_status0);
    core_addresses.emplace(aie2ps::cm_event_status1);
    core_addresses.emplace(aie2ps::cm_event_status2);
    core_addresses.emplace(aie2ps::cm_event_status3);
    core_addresses.emplace(aie2ps::cm_event_broadcast0);
    core_addresses.emplace(aie2ps::cm_event_broadcast1);
    core_addresses.emplace(aie2ps::cm_event_broadcast2);
    core_addresses.emplace(aie2ps::cm_event_broadcast3);
    core_addresses.emplace(aie2ps::cm_event_broadcast4);
    core_addresses.emplace(aie2ps::cm_event_broadcast5);
    core_addresses.emplace(aie2ps::cm_event_broadcast6);
    core_addresses.emplace(aie2ps::cm_event_broadcast7);
    core_addresses.emplace(aie2ps::cm_event_broadcast8);
    core_addresses.emplace(aie2ps::cm_event_broadcast9);
    core_addresses.emplace(aie2ps::cm_event_broadcast10);
    core_addresses.emplace(aie2ps::cm_event_broadcast11);
    core_addresses.emplace(aie2ps::cm_event_broadcast12);
    core_addresses.emplace(aie2ps::cm_event_broadcast13);
    core_addresses.emplace(aie2ps::cm_event_broadcast14);
    core_addresses.emplace(aie2ps::cm_event_broadcast15);
    core_addresses.emplace(aie2ps::cm_timer_trig_event_low_value);
    core_addresses.emplace(aie2ps::cm_timer_trig_event_high_value);
    core_addresses.emplace(aie2ps::cm_timer_low);
    core_addresses.emplace(aie2ps::cm_timer_high);
    core_addresses.emplace(aie2ps::cm_edge_detection_event_control);
    core_addresses.emplace(aie2ps::cm_stream_switch_event_port_selection_0);
    core_addresses.emplace(aie2ps::cm_stream_switch_event_port_selection_1);

    // Memory modules
    memory_addresses.emplace(aie2ps::mm_trace_control0);
    memory_addresses.emplace(aie2ps::mm_trace_control1);
    memory_addresses.emplace(aie2ps::mm_trace_status);
    memory_addresses.emplace(aie2ps::mm_trace_event0);
    memory_addresses.emplace(aie2ps::mm_trace_event1);
    memory_addresses.emplace(aie2ps::mm_event_status0);
    memory_addresses.emplace(aie2ps::mm_event_status1);
    memory_addresses.emplace(aie2ps::mm_event_status2);
    memory_addresses.emplace(aie2ps::mm_event_status3);
    memory_addresses.emplace(aie2ps::mm_event_broadcast0);
    memory_addresses.emplace(aie2ps::mm_event_broadcast1);
    memory_addresses.emplace(aie2ps::mm_event_broadcast2);
    memory_addresses.emplace(aie2ps::mm_event_broadcast3);
    memory_addresses.emplace(aie2ps::mm_event_broadcast4);
    memory_addresses.emplace(aie2ps::mm_event_broadcast5);
    memory_addresses.emplace(aie2ps::mm_event_broadcast6);
    memory_addresses.emplace(aie2ps::mm_event_broadcast7);
    memory_addresses.emplace(aie2ps::mm_event_broadcast8);
    memory_addresses.emplace(aie2ps::mm_event_broadcast9);
    memory_addresses.emplace(aie2ps::mm_event_broadcast10);
    memory_addresses.emplace(aie2ps::mm_event_broadcast11);
    memory_addresses.emplace(aie2ps::mm_event_broadcast12);
    memory_addresses.emplace(aie2ps::mm_event_broadcast13);
    memory_addresses.emplace(aie2ps::mm_event_broadcast14);
    memory_addresses.emplace(aie2ps::mm_event_broadcast15);

    // Interface tiles
    interface_addresses.emplace(aie2ps::shim_trace_control0);
    interface_addresses.emplace(aie2ps::shim_trace_control1);
    interface_addresses.emplace(aie2ps::shim_trace_status);
    interface_addresses.emplace(aie2ps::shim_trace_event0);
    interface_addresses.emplace(aie2ps::shim_trace_event1);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_0);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_1);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_2);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_3);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_4);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_5);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_6);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_7);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_8);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_9);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_10);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_11);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_12);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_13);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_14);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_15);
    interface_addresses.emplace(aie2ps::shim_event_status0);
    interface_addresses.emplace(aie2ps::shim_event_status1);
    interface_addresses.emplace(aie2ps::shim_event_status2);
    interface_addresses.emplace(aie2ps::shim_event_status3);
    interface_addresses.emplace(aie2ps::shim_stream_switch_event_port_selection_0);
    interface_addresses.emplace(aie2ps::shim_stream_switch_event_port_selection_1);

    // Memory tiles
    memory_tile_addresses.emplace(aie2ps::mem_trace_control0);
    memory_tile_addresses.emplace(aie2ps::mem_trace_control1);
    memory_tile_addresses.emplace(aie2ps::mem_trace_status);
    memory_tile_addresses.emplace(aie2ps::mem_trace_event0);
    memory_tile_addresses.emplace(aie2ps::mem_trace_event1);
    memory_tile_addresses.emplace(aie2ps::mem_dma_event_channel_selection);
    memory_tile_addresses.emplace(aie2ps::mem_edge_detection_event_control);
    memory_tile_addresses.emplace(aie2ps::mem_stream_switch_event_port_selection_0);
    memory_tile_addresses.emplace(aie2ps::mem_stream_switch_event_port_selection_1);
    memory_tile_addresses.emplace(aie2ps::mem_event_broadcast0);
    memory_tile_addresses.emplace(aie2ps::mem_event_status0);
    memory_tile_addresses.emplace(aie2ps::mem_event_status1);
    memory_tile_addresses.emplace(aie2ps::mem_event_status2);
    memory_tile_addresses.emplace(aie2ps::mem_event_status3);
    memory_tile_addresses.emplace(aie2ps::mem_event_status4);
    memory_tile_addresses.emplace(aie2ps::mem_event_status5);
  }


void AIE2psUsedRegisters::populateRegNameToValueMap() {
   regNameToValue["cm_core_bmll0_part1"] = aie2ps::cm_core_bmll0_part1;
   regNameToValue["cm_core_bmll0_part2"] = aie2ps::cm_core_bmll0_part2;
   regNameToValue["cm_core_bmll0_part3"] = aie2ps::cm_core_bmll0_part3;
   regNameToValue["cm_core_bmll0_part4"] = aie2ps::cm_core_bmll0_part4;
   regNameToValue["cm_core_bmlh0_part1"] = aie2ps::cm_core_bmlh0_part1;
   regNameToValue["cm_core_bmlh0_part2"] = aie2ps::cm_core_bmlh0_part2;
   regNameToValue["cm_core_bmlh0_part3"] = aie2ps::cm_core_bmlh0_part3;
   regNameToValue["cm_core_bmlh0_part4"] = aie2ps::cm_core_bmlh0_part4;
   regNameToValue["cm_core_bmhl0_part1"] = aie2ps::cm_core_bmhl0_part1;
   regNameToValue["cm_core_bmhl0_part2"] = aie2ps::cm_core_bmhl0_part2;
   regNameToValue["cm_core_bmhl0_part3"] = aie2ps::cm_core_bmhl0_part3;
   regNameToValue["cm_core_bmhl0_part4"] = aie2ps::cm_core_bmhl0_part4;
   regNameToValue["cm_core_bmhh0_part1"] = aie2ps::cm_core_bmhh0_part1;
   regNameToValue["cm_core_bmhh0_part2"] = aie2ps::cm_core_bmhh0_part2;
   regNameToValue["cm_core_bmhh0_part3"] = aie2ps::cm_core_bmhh0_part3;
   regNameToValue["cm_core_bmhh0_part4"] = aie2ps::cm_core_bmhh0_part4;
   regNameToValue["cm_core_bmll1_part1"] = aie2ps::cm_core_bmll1_part1;
   regNameToValue["cm_core_bmll1_part2"] = aie2ps::cm_core_bmll1_part2;
   regNameToValue["cm_core_bmll1_part3"] = aie2ps::cm_core_bmll1_part3;
   regNameToValue["cm_core_bmll1_part4"] = aie2ps::cm_core_bmll1_part4;
   regNameToValue["cm_core_bmlh1_part1"] = aie2ps::cm_core_bmlh1_part1;
   regNameToValue["cm_core_bmlh1_part2"] = aie2ps::cm_core_bmlh1_part2;
   regNameToValue["cm_core_bmlh1_part3"] = aie2ps::cm_core_bmlh1_part3;
   regNameToValue["cm_core_bmlh1_part4"] = aie2ps::cm_core_bmlh1_part4;
   regNameToValue["cm_core_bmhl1_part1"] = aie2ps::cm_core_bmhl1_part1;
   regNameToValue["cm_core_bmhl1_part2"] = aie2ps::cm_core_bmhl1_part2;
   regNameToValue["cm_core_bmhl1_part3"] = aie2ps::cm_core_bmhl1_part3;
   regNameToValue["cm_core_bmhl1_part4"] = aie2ps::cm_core_bmhl1_part4;
   regNameToValue["cm_core_bmhh1_part1"] = aie2ps::cm_core_bmhh1_part1;
   regNameToValue["cm_core_bmhh1_part2"] = aie2ps::cm_core_bmhh1_part2;
   regNameToValue["cm_core_bmhh1_part3"] = aie2ps::cm_core_bmhh1_part3;
   regNameToValue["cm_core_bmhh1_part4"] = aie2ps::cm_core_bmhh1_part4;
   regNameToValue["cm_core_bmll2_part1"] = aie2ps::cm_core_bmll2_part1;
   regNameToValue["cm_core_bmll2_part2"] = aie2ps::cm_core_bmll2_part2;
   regNameToValue["cm_core_bmll2_part3"] = aie2ps::cm_core_bmll2_part3;
   regNameToValue["cm_core_bmll2_part4"] = aie2ps::cm_core_bmll2_part4;
   regNameToValue["cm_core_bmlh2_part1"] = aie2ps::cm_core_bmlh2_part1;
   regNameToValue["cm_core_bmlh2_part2"] = aie2ps::cm_core_bmlh2_part2;
   regNameToValue["cm_core_bmlh2_part3"] = aie2ps::cm_core_bmlh2_part3;
   regNameToValue["cm_core_bmlh2_part4"] = aie2ps::cm_core_bmlh2_part4;
   regNameToValue["cm_core_bmhl2_part1"] = aie2ps::cm_core_bmhl2_part1;
   regNameToValue["cm_core_bmhl2_part2"] = aie2ps::cm_core_bmhl2_part2;
   regNameToValue["cm_core_bmhl2_part3"] = aie2ps::cm_core_bmhl2_part3;
   regNameToValue["cm_core_bmhl2_part4"] = aie2ps::cm_core_bmhl2_part4;
   regNameToValue["cm_core_bmhh2_part1"] = aie2ps::cm_core_bmhh2_part1;
   regNameToValue["cm_core_bmhh2_part2"] = aie2ps::cm_core_bmhh2_part2;
   regNameToValue["cm_core_bmhh2_part3"] = aie2ps::cm_core_bmhh2_part3;
   regNameToValue["cm_core_bmhh2_part4"] = aie2ps::cm_core_bmhh2_part4;
   regNameToValue["cm_core_bmll3_part1"] = aie2ps::cm_core_bmll3_part1;
   regNameToValue["cm_core_bmll3_part2"] = aie2ps::cm_core_bmll3_part2;
   regNameToValue["cm_core_bmll3_part3"] = aie2ps::cm_core_bmll3_part3;
   regNameToValue["cm_core_bmll3_part4"] = aie2ps::cm_core_bmll3_part4;
   regNameToValue["cm_core_bmlh3_part1"] = aie2ps::cm_core_bmlh3_part1;
   regNameToValue["cm_core_bmlh3_part2"] = aie2ps::cm_core_bmlh3_part2;
   regNameToValue["cm_core_bmlh3_part3"] = aie2ps::cm_core_bmlh3_part3;
   regNameToValue["cm_core_bmlh3_part4"] = aie2ps::cm_core_bmlh3_part4;
   regNameToValue["cm_core_bmhl3_part1"] = aie2ps::cm_core_bmhl3_part1;
   regNameToValue["cm_core_bmhl3_part2"] = aie2ps::cm_core_bmhl3_part2;
   regNameToValue["cm_core_bmhl3_part3"] = aie2ps::cm_core_bmhl3_part3;
   regNameToValue["cm_core_bmhl3_part4"] = aie2ps::cm_core_bmhl3_part4;
   regNameToValue["cm_core_bmhh3_part1"] = aie2ps::cm_core_bmhh3_part1;
   regNameToValue["cm_core_bmhh3_part2"] = aie2ps::cm_core_bmhh3_part2;
   regNameToValue["cm_core_bmhh3_part3"] = aie2ps::cm_core_bmhh3_part3;
   regNameToValue["cm_core_bmhh3_part4"] = aie2ps::cm_core_bmhh3_part4;
   regNameToValue["cm_core_bmll4_part1"] = aie2ps::cm_core_bmll4_part1;
   regNameToValue["cm_core_bmll4_part2"] = aie2ps::cm_core_bmll4_part2;
   regNameToValue["cm_core_bmll4_part3"] = aie2ps::cm_core_bmll4_part3;
   regNameToValue["cm_core_bmll4_part4"] = aie2ps::cm_core_bmll4_part4;
   regNameToValue["cm_core_bmlh4_part1"] = aie2ps::cm_core_bmlh4_part1;
   regNameToValue["cm_core_bmlh4_part2"] = aie2ps::cm_core_bmlh4_part2;
   regNameToValue["cm_core_bmlh4_part3"] = aie2ps::cm_core_bmlh4_part3;
   regNameToValue["cm_core_bmlh4_part4"] = aie2ps::cm_core_bmlh4_part4;
   regNameToValue["cm_core_bmhl4_part1"] = aie2ps::cm_core_bmhl4_part1;
   regNameToValue["cm_core_bmhl4_part2"] = aie2ps::cm_core_bmhl4_part2;
   regNameToValue["cm_core_bmhl4_part3"] = aie2ps::cm_core_bmhl4_part3;
   regNameToValue["cm_core_bmhl4_part4"] = aie2ps::cm_core_bmhl4_part4;
   regNameToValue["cm_core_bmhh4_part1"] = aie2ps::cm_core_bmhh4_part1;
   regNameToValue["cm_core_bmhh4_part2"] = aie2ps::cm_core_bmhh4_part2;
   regNameToValue["cm_core_bmhh4_part3"] = aie2ps::cm_core_bmhh4_part3;
   regNameToValue["cm_core_bmhh4_part4"] = aie2ps::cm_core_bmhh4_part4;
   regNameToValue["cm_core_bmll5_part1"] = aie2ps::cm_core_bmll5_part1;
   regNameToValue["cm_core_bmll5_part2"] = aie2ps::cm_core_bmll5_part2;
   regNameToValue["cm_core_bmll5_part3"] = aie2ps::cm_core_bmll5_part3;
   regNameToValue["cm_core_bmll5_part4"] = aie2ps::cm_core_bmll5_part4;
   regNameToValue["cm_core_bmlh5_part1"] = aie2ps::cm_core_bmlh5_part1;
   regNameToValue["cm_core_bmlh5_part2"] = aie2ps::cm_core_bmlh5_part2;
   regNameToValue["cm_core_bmlh5_part3"] = aie2ps::cm_core_bmlh5_part3;
   regNameToValue["cm_core_bmlh5_part4"] = aie2ps::cm_core_bmlh5_part4;
   regNameToValue["cm_core_bmhl5_part1"] = aie2ps::cm_core_bmhl5_part1;
   regNameToValue["cm_core_bmhl5_part2"] = aie2ps::cm_core_bmhl5_part2;
   regNameToValue["cm_core_bmhl5_part3"] = aie2ps::cm_core_bmhl5_part3;
   regNameToValue["cm_core_bmhl5_part4"] = aie2ps::cm_core_bmhl5_part4;
   regNameToValue["cm_core_bmhh5_part1"] = aie2ps::cm_core_bmhh5_part1;
   regNameToValue["cm_core_bmhh5_part2"] = aie2ps::cm_core_bmhh5_part2;
   regNameToValue["cm_core_bmhh5_part3"] = aie2ps::cm_core_bmhh5_part3;
   regNameToValue["cm_core_bmhh5_part4"] = aie2ps::cm_core_bmhh5_part4;
   regNameToValue["cm_core_bmll6_part1"] = aie2ps::cm_core_bmll6_part1;
   regNameToValue["cm_core_bmll6_part2"] = aie2ps::cm_core_bmll6_part2;
   regNameToValue["cm_core_bmll6_part3"] = aie2ps::cm_core_bmll6_part3;
   regNameToValue["cm_core_bmll6_part4"] = aie2ps::cm_core_bmll6_part4;
   regNameToValue["cm_core_bmlh6_part1"] = aie2ps::cm_core_bmlh6_part1;
   regNameToValue["cm_core_bmlh6_part2"] = aie2ps::cm_core_bmlh6_part2;
   regNameToValue["cm_core_bmlh6_part3"] = aie2ps::cm_core_bmlh6_part3;
   regNameToValue["cm_core_bmlh6_part4"] = aie2ps::cm_core_bmlh6_part4;
   regNameToValue["cm_core_bmhl6_part1"] = aie2ps::cm_core_bmhl6_part1;
   regNameToValue["cm_core_bmhl6_part2"] = aie2ps::cm_core_bmhl6_part2;
   regNameToValue["cm_core_bmhl6_part3"] = aie2ps::cm_core_bmhl6_part3;
   regNameToValue["cm_core_bmhl6_part4"] = aie2ps::cm_core_bmhl6_part4;
   regNameToValue["cm_core_bmhh6_part1"] = aie2ps::cm_core_bmhh6_part1;
   regNameToValue["cm_core_bmhh6_part2"] = aie2ps::cm_core_bmhh6_part2;
   regNameToValue["cm_core_bmhh6_part3"] = aie2ps::cm_core_bmhh6_part3;
   regNameToValue["cm_core_bmhh6_part4"] = aie2ps::cm_core_bmhh6_part4;
   regNameToValue["cm_core_bmll7_part1"] = aie2ps::cm_core_bmll7_part1;
   regNameToValue["cm_core_bmll7_part2"] = aie2ps::cm_core_bmll7_part2;
   regNameToValue["cm_core_bmll7_part3"] = aie2ps::cm_core_bmll7_part3;
   regNameToValue["cm_core_bmll7_part4"] = aie2ps::cm_core_bmll7_part4;
   regNameToValue["cm_core_bmlh7_part1"] = aie2ps::cm_core_bmlh7_part1;
   regNameToValue["cm_core_bmlh7_part2"] = aie2ps::cm_core_bmlh7_part2;
   regNameToValue["cm_core_bmlh7_part3"] = aie2ps::cm_core_bmlh7_part3;
   regNameToValue["cm_core_bmlh7_part4"] = aie2ps::cm_core_bmlh7_part4;
   regNameToValue["cm_core_bmhl7_part1"] = aie2ps::cm_core_bmhl7_part1;
   regNameToValue["cm_core_bmhl7_part2"] = aie2ps::cm_core_bmhl7_part2;
   regNameToValue["cm_core_bmhl7_part3"] = aie2ps::cm_core_bmhl7_part3;
   regNameToValue["cm_core_bmhl7_part4"] = aie2ps::cm_core_bmhl7_part4;
   regNameToValue["cm_core_bmhh7_part1"] = aie2ps::cm_core_bmhh7_part1;
   regNameToValue["cm_core_bmhh7_part2"] = aie2ps::cm_core_bmhh7_part2;
   regNameToValue["cm_core_bmhh7_part3"] = aie2ps::cm_core_bmhh7_part3;
   regNameToValue["cm_core_bmhh7_part4"] = aie2ps::cm_core_bmhh7_part4;
   regNameToValue["cm_core_x0_part1"] = aie2ps::cm_core_x0_part1;
   regNameToValue["cm_core_x0_part2"] = aie2ps::cm_core_x0_part2;
   regNameToValue["cm_core_x0_part3"] = aie2ps::cm_core_x0_part3;
   regNameToValue["cm_core_x0_part4"] = aie2ps::cm_core_x0_part4;
   regNameToValue["cm_core_x1_part1"] = aie2ps::cm_core_x1_part1;
   regNameToValue["cm_core_x1_part2"] = aie2ps::cm_core_x1_part2;
   regNameToValue["cm_core_x1_part3"] = aie2ps::cm_core_x1_part3;
   regNameToValue["cm_core_x1_part4"] = aie2ps::cm_core_x1_part4;
   regNameToValue["cm_core_x2_part1"] = aie2ps::cm_core_x2_part1;
   regNameToValue["cm_core_x2_part2"] = aie2ps::cm_core_x2_part2;
   regNameToValue["cm_core_x2_part3"] = aie2ps::cm_core_x2_part3;
   regNameToValue["cm_core_x2_part4"] = aie2ps::cm_core_x2_part4;
   regNameToValue["cm_core_x3_part1"] = aie2ps::cm_core_x3_part1;
   regNameToValue["cm_core_x3_part2"] = aie2ps::cm_core_x3_part2;
   regNameToValue["cm_core_x3_part3"] = aie2ps::cm_core_x3_part3;
   regNameToValue["cm_core_x3_part4"] = aie2ps::cm_core_x3_part4;
   regNameToValue["cm_core_x4_part1"] = aie2ps::cm_core_x4_part1;
   regNameToValue["cm_core_x4_part2"] = aie2ps::cm_core_x4_part2;
   regNameToValue["cm_core_x4_part3"] = aie2ps::cm_core_x4_part3;
   regNameToValue["cm_core_x4_part4"] = aie2ps::cm_core_x4_part4;
   regNameToValue["cm_core_x5_part1"] = aie2ps::cm_core_x5_part1;
   regNameToValue["cm_core_x5_part2"] = aie2ps::cm_core_x5_part2;
   regNameToValue["cm_core_x5_part3"] = aie2ps::cm_core_x5_part3;
   regNameToValue["cm_core_x5_part4"] = aie2ps::cm_core_x5_part4;
   regNameToValue["cm_core_x6_part1"] = aie2ps::cm_core_x6_part1;
   regNameToValue["cm_core_x6_part2"] = aie2ps::cm_core_x6_part2;
   regNameToValue["cm_core_x6_part3"] = aie2ps::cm_core_x6_part3;
   regNameToValue["cm_core_x6_part4"] = aie2ps::cm_core_x6_part4;
   regNameToValue["cm_core_x7_part1"] = aie2ps::cm_core_x7_part1;
   regNameToValue["cm_core_x7_part2"] = aie2ps::cm_core_x7_part2;
   regNameToValue["cm_core_x7_part3"] = aie2ps::cm_core_x7_part3;
   regNameToValue["cm_core_x7_part4"] = aie2ps::cm_core_x7_part4;
   regNameToValue["cm_core_x8_part1"] = aie2ps::cm_core_x8_part1;
   regNameToValue["cm_core_x8_part2"] = aie2ps::cm_core_x8_part2;
   regNameToValue["cm_core_x8_part3"] = aie2ps::cm_core_x8_part3;
   regNameToValue["cm_core_x8_part4"] = aie2ps::cm_core_x8_part4;
   regNameToValue["cm_core_x9_part1"] = aie2ps::cm_core_x9_part1;
   regNameToValue["cm_core_x9_part2"] = aie2ps::cm_core_x9_part2;
   regNameToValue["cm_core_x9_part3"] = aie2ps::cm_core_x9_part3;
   regNameToValue["cm_core_x9_part4"] = aie2ps::cm_core_x9_part4;
   regNameToValue["cm_core_x10_part1"] = aie2ps::cm_core_x10_part1;
   regNameToValue["cm_core_x10_part2"] = aie2ps::cm_core_x10_part2;
   regNameToValue["cm_core_x10_part3"] = aie2ps::cm_core_x10_part3;
   regNameToValue["cm_core_x10_part4"] = aie2ps::cm_core_x10_part4;
   regNameToValue["cm_core_x11_part1"] = aie2ps::cm_core_x11_part1;
   regNameToValue["cm_core_x11_part2"] = aie2ps::cm_core_x11_part2;
   regNameToValue["cm_core_x11_part3"] = aie2ps::cm_core_x11_part3;
   regNameToValue["cm_core_x11_part4"] = aie2ps::cm_core_x11_part4;
   regNameToValue["cm_core_ldfifol0_part1"] = aie2ps::cm_core_ldfifol0_part1;
   regNameToValue["cm_core_ldfifol0_part2"] = aie2ps::cm_core_ldfifol0_part2;
   regNameToValue["cm_core_ldfifol0_part3"] = aie2ps::cm_core_ldfifol0_part3;
   regNameToValue["cm_core_ldfifol0_part4"] = aie2ps::cm_core_ldfifol0_part4;
   regNameToValue["cm_core_ldfifoh0_part1"] = aie2ps::cm_core_ldfifoh0_part1;
   regNameToValue["cm_core_ldfifoh0_part2"] = aie2ps::cm_core_ldfifoh0_part2;
   regNameToValue["cm_core_ldfifoh0_part3"] = aie2ps::cm_core_ldfifoh0_part3;
   regNameToValue["cm_core_ldfifoh0_part4"] = aie2ps::cm_core_ldfifoh0_part4;
   regNameToValue["cm_core_ldfifol1_part1"] = aie2ps::cm_core_ldfifol1_part1;
   regNameToValue["cm_core_ldfifol1_part2"] = aie2ps::cm_core_ldfifol1_part2;
   regNameToValue["cm_core_ldfifol1_part3"] = aie2ps::cm_core_ldfifol1_part3;
   regNameToValue["cm_core_ldfifol1_part4"] = aie2ps::cm_core_ldfifol1_part4;
   regNameToValue["cm_core_ldfifoh1_part1"] = aie2ps::cm_core_ldfifoh1_part1;
   regNameToValue["cm_core_ldfifoh1_part2"] = aie2ps::cm_core_ldfifoh1_part2;
   regNameToValue["cm_core_ldfifoh1_part3"] = aie2ps::cm_core_ldfifoh1_part3;
   regNameToValue["cm_core_ldfifoh1_part4"] = aie2ps::cm_core_ldfifoh1_part4;
   regNameToValue["cm_core_stfifol_part1"] = aie2ps::cm_core_stfifol_part1;
   regNameToValue["cm_core_stfifol_part2"] = aie2ps::cm_core_stfifol_part2;
   regNameToValue["cm_core_stfifol_part3"] = aie2ps::cm_core_stfifol_part3;
   regNameToValue["cm_core_stfifol_part4"] = aie2ps::cm_core_stfifol_part4;
   regNameToValue["cm_core_stfifoh_part1"] = aie2ps::cm_core_stfifoh_part1;
   regNameToValue["cm_core_stfifoh_part2"] = aie2ps::cm_core_stfifoh_part2;
   regNameToValue["cm_core_stfifoh_part3"] = aie2ps::cm_core_stfifoh_part3;
   regNameToValue["cm_core_stfifoh_part4"] = aie2ps::cm_core_stfifoh_part4;
   regNameToValue["cm_core_fifoxtra_part1"] = aie2ps::cm_core_fifoxtra_part1;
   regNameToValue["cm_core_fifoxtra_part2"] = aie2ps::cm_core_fifoxtra_part2;
   regNameToValue["cm_core_fifoxtra_part3"] = aie2ps::cm_core_fifoxtra_part3;
   regNameToValue["cm_core_fifoxtra_part4"] = aie2ps::cm_core_fifoxtra_part4;
   regNameToValue["cm_core_eg0"] = aie2ps::cm_core_eg0;
   regNameToValue["cm_core_eg1"] = aie2ps::cm_core_eg1;
   regNameToValue["cm_core_eg2"] = aie2ps::cm_core_eg2;
   regNameToValue["cm_core_eg3"] = aie2ps::cm_core_eg3;
   regNameToValue["cm_core_eg4"] = aie2ps::cm_core_eg4;
   regNameToValue["cm_core_eg5"] = aie2ps::cm_core_eg5;
   regNameToValue["cm_core_eg6"] = aie2ps::cm_core_eg6;
   regNameToValue["cm_core_eg7"] = aie2ps::cm_core_eg7;
   regNameToValue["cm_core_eg8"] = aie2ps::cm_core_eg8;
   regNameToValue["cm_core_eg9"] = aie2ps::cm_core_eg9;
   regNameToValue["cm_core_eg10"] = aie2ps::cm_core_eg10;
   regNameToValue["cm_core_eg11"] = aie2ps::cm_core_eg11;
   regNameToValue["cm_core_f0"] = aie2ps::cm_core_f0;
   regNameToValue["cm_core_f1"] = aie2ps::cm_core_f1;
   regNameToValue["cm_core_f2"] = aie2ps::cm_core_f2;
   regNameToValue["cm_core_f3"] = aie2ps::cm_core_f3;
   regNameToValue["cm_core_f4"] = aie2ps::cm_core_f4;
   regNameToValue["cm_core_f5"] = aie2ps::cm_core_f5;
   regNameToValue["cm_core_f6"] = aie2ps::cm_core_f6;
   regNameToValue["cm_core_f7"] = aie2ps::cm_core_f7;
   regNameToValue["cm_core_f8"] = aie2ps::cm_core_f8;
   regNameToValue["cm_core_f9"] = aie2ps::cm_core_f9;
   regNameToValue["cm_core_f10"] = aie2ps::cm_core_f10;
   regNameToValue["cm_core_f11"] = aie2ps::cm_core_f11;
   regNameToValue["cm_core_r0"] = aie2ps::cm_core_r0;
   regNameToValue["cm_core_r1"] = aie2ps::cm_core_r1;
   regNameToValue["cm_core_r2"] = aie2ps::cm_core_r2;
   regNameToValue["cm_core_r3"] = aie2ps::cm_core_r3;
   regNameToValue["cm_core_r4"] = aie2ps::cm_core_r4;
   regNameToValue["cm_core_r5"] = aie2ps::cm_core_r5;
   regNameToValue["cm_core_r6"] = aie2ps::cm_core_r6;
   regNameToValue["cm_core_r7"] = aie2ps::cm_core_r7;
   regNameToValue["cm_core_r8"] = aie2ps::cm_core_r8;
   regNameToValue["cm_core_r9"] = aie2ps::cm_core_r9;
   regNameToValue["cm_core_r10"] = aie2ps::cm_core_r10;
   regNameToValue["cm_core_r11"] = aie2ps::cm_core_r11;
   regNameToValue["cm_core_r12"] = aie2ps::cm_core_r12;
   regNameToValue["cm_core_r13"] = aie2ps::cm_core_r13;
   regNameToValue["cm_core_r14"] = aie2ps::cm_core_r14;
   regNameToValue["cm_core_r15"] = aie2ps::cm_core_r15;
   regNameToValue["cm_core_r16"] = aie2ps::cm_core_r16;
   regNameToValue["cm_core_r17"] = aie2ps::cm_core_r17;
   regNameToValue["cm_core_r18"] = aie2ps::cm_core_r18;
   regNameToValue["cm_core_r19"] = aie2ps::cm_core_r19;
   regNameToValue["cm_core_r20"] = aie2ps::cm_core_r20;
   regNameToValue["cm_core_r21"] = aie2ps::cm_core_r21;
   regNameToValue["cm_core_r22"] = aie2ps::cm_core_r22;
   regNameToValue["cm_core_r23"] = aie2ps::cm_core_r23;
   regNameToValue["cm_core_r24"] = aie2ps::cm_core_r24;
   regNameToValue["cm_core_r25"] = aie2ps::cm_core_r25;
   regNameToValue["cm_core_r26"] = aie2ps::cm_core_r26;
   regNameToValue["cm_core_r27"] = aie2ps::cm_core_r27;
   regNameToValue["cm_core_r28"] = aie2ps::cm_core_r28;
   regNameToValue["cm_core_r29"] = aie2ps::cm_core_r29;
   regNameToValue["cm_core_r30"] = aie2ps::cm_core_r30;
   regNameToValue["cm_core_r31"] = aie2ps::cm_core_r31;
   regNameToValue["cm_core_m0"] = aie2ps::cm_core_m0;
   regNameToValue["cm_core_m1"] = aie2ps::cm_core_m1;
   regNameToValue["cm_core_m2"] = aie2ps::cm_core_m2;
   regNameToValue["cm_core_m3"] = aie2ps::cm_core_m3;
   regNameToValue["cm_core_m4"] = aie2ps::cm_core_m4;
   regNameToValue["cm_core_m5"] = aie2ps::cm_core_m5;
   regNameToValue["cm_core_m6"] = aie2ps::cm_core_m6;
   regNameToValue["cm_core_m7"] = aie2ps::cm_core_m7;
   regNameToValue["cm_core_dn0"] = aie2ps::cm_core_dn0;
   regNameToValue["cm_core_dn1"] = aie2ps::cm_core_dn1;
   regNameToValue["cm_core_dn2"] = aie2ps::cm_core_dn2;
   regNameToValue["cm_core_dn3"] = aie2ps::cm_core_dn3;
   regNameToValue["cm_core_dn4"] = aie2ps::cm_core_dn4;
   regNameToValue["cm_core_dn5"] = aie2ps::cm_core_dn5;
   regNameToValue["cm_core_dn6"] = aie2ps::cm_core_dn6;
   regNameToValue["cm_core_dn7"] = aie2ps::cm_core_dn7;
   regNameToValue["cm_core_dj0"] = aie2ps::cm_core_dj0;
   regNameToValue["cm_core_dj1"] = aie2ps::cm_core_dj1;
   regNameToValue["cm_core_dj2"] = aie2ps::cm_core_dj2;
   regNameToValue["cm_core_dj3"] = aie2ps::cm_core_dj3;
   regNameToValue["cm_core_dj4"] = aie2ps::cm_core_dj4;
   regNameToValue["cm_core_dj5"] = aie2ps::cm_core_dj5;
   regNameToValue["cm_core_dj6"] = aie2ps::cm_core_dj6;
   regNameToValue["cm_core_dj7"] = aie2ps::cm_core_dj7;
   regNameToValue["cm_core_dc0"] = aie2ps::cm_core_dc0;
   regNameToValue["cm_core_dc1"] = aie2ps::cm_core_dc1;
   regNameToValue["cm_core_dc2"] = aie2ps::cm_core_dc2;
   regNameToValue["cm_core_dc3"] = aie2ps::cm_core_dc3;
   regNameToValue["cm_core_dc4"] = aie2ps::cm_core_dc4;
   regNameToValue["cm_core_dc5"] = aie2ps::cm_core_dc5;
   regNameToValue["cm_core_dc6"] = aie2ps::cm_core_dc6;
   regNameToValue["cm_core_dc7"] = aie2ps::cm_core_dc7;
   regNameToValue["cm_core_p0"] = aie2ps::cm_core_p0;
   regNameToValue["cm_core_p1"] = aie2ps::cm_core_p1;
   regNameToValue["cm_core_p2"] = aie2ps::cm_core_p2;
   regNameToValue["cm_core_p3"] = aie2ps::cm_core_p3;
   regNameToValue["cm_core_p4"] = aie2ps::cm_core_p4;
   regNameToValue["cm_core_p5"] = aie2ps::cm_core_p5;
   regNameToValue["cm_core_p6"] = aie2ps::cm_core_p6;
   regNameToValue["cm_core_p7"] = aie2ps::cm_core_p7;
   regNameToValue["cm_core_s0"] = aie2ps::cm_core_s0;
   regNameToValue["cm_core_s1"] = aie2ps::cm_core_s1;
   regNameToValue["cm_core_s2"] = aie2ps::cm_core_s2;
   regNameToValue["cm_core_s3"] = aie2ps::cm_core_s3;
   regNameToValue["cm_program_counter"] = aie2ps::cm_program_counter;
   regNameToValue["cm_core_fc"] = aie2ps::cm_core_fc;
   regNameToValue["cm_core_sp"] = aie2ps::cm_core_sp;
   regNameToValue["cm_core_lr"] = aie2ps::cm_core_lr;
   regNameToValue["cm_core_ls"] = aie2ps::cm_core_ls;
   regNameToValue["cm_core_le"] = aie2ps::cm_core_le;
   regNameToValue["cm_core_lc"] = aie2ps::cm_core_lc;
   regNameToValue["cm_core_lci"] = aie2ps::cm_core_lci;
   regNameToValue["cm_core_cr1"] = aie2ps::cm_core_cr1;
   regNameToValue["cm_core_cr2"] = aie2ps::cm_core_cr2;
   regNameToValue["cm_core_cr3"] = aie2ps::cm_core_cr3;
   regNameToValue["cm_core_sr1"] = aie2ps::cm_core_sr1;
   regNameToValue["cm_core_sr2"] = aie2ps::cm_core_sr2;
   regNameToValue["cm_timer_control"] = aie2ps::cm_timer_control;
   regNameToValue["cm_event_generate"] = aie2ps::cm_event_generate;
   regNameToValue["cm_event_broadcast0"] = aie2ps::cm_event_broadcast0;
   regNameToValue["cm_event_broadcast1"] = aie2ps::cm_event_broadcast1;
   regNameToValue["cm_event_broadcast2"] = aie2ps::cm_event_broadcast2;
   regNameToValue["cm_event_broadcast3"] = aie2ps::cm_event_broadcast3;
   regNameToValue["cm_event_broadcast4"] = aie2ps::cm_event_broadcast4;
   regNameToValue["cm_event_broadcast5"] = aie2ps::cm_event_broadcast5;
   regNameToValue["cm_event_broadcast6"] = aie2ps::cm_event_broadcast6;
   regNameToValue["cm_event_broadcast7"] = aie2ps::cm_event_broadcast7;
   regNameToValue["cm_event_broadcast8"] = aie2ps::cm_event_broadcast8;
   regNameToValue["cm_event_broadcast9"] = aie2ps::cm_event_broadcast9;
   regNameToValue["cm_event_broadcast10"] = aie2ps::cm_event_broadcast10;
   regNameToValue["cm_event_broadcast11"] = aie2ps::cm_event_broadcast11;
   regNameToValue["cm_event_broadcast12"] = aie2ps::cm_event_broadcast12;
   regNameToValue["cm_event_broadcast13"] = aie2ps::cm_event_broadcast13;
   regNameToValue["cm_event_broadcast14"] = aie2ps::cm_event_broadcast14;
   regNameToValue["cm_event_broadcast15"] = aie2ps::cm_event_broadcast15;
   regNameToValue["cm_event_broadcast_block_south_set"] = aie2ps::cm_event_broadcast_block_south_set;
   regNameToValue["cm_event_broadcast_block_south_clr"] = aie2ps::cm_event_broadcast_block_south_clr;
   regNameToValue["cm_event_broadcast_block_south_value"] = aie2ps::cm_event_broadcast_block_south_value;
   regNameToValue["cm_event_broadcast_block_west_set"] = aie2ps::cm_event_broadcast_block_west_set;
   regNameToValue["cm_event_broadcast_block_west_clr"] = aie2ps::cm_event_broadcast_block_west_clr;
   regNameToValue["cm_event_broadcast_block_west_value"] = aie2ps::cm_event_broadcast_block_west_value;
   regNameToValue["cm_event_broadcast_block_north_set"] = aie2ps::cm_event_broadcast_block_north_set;
   regNameToValue["cm_event_broadcast_block_north_clr"] = aie2ps::cm_event_broadcast_block_north_clr;
   regNameToValue["cm_event_broadcast_block_north_value"] = aie2ps::cm_event_broadcast_block_north_value;
   regNameToValue["cm_event_broadcast_block_east_set"] = aie2ps::cm_event_broadcast_block_east_set;
   regNameToValue["cm_event_broadcast_block_east_clr"] = aie2ps::cm_event_broadcast_block_east_clr;
   regNameToValue["cm_event_broadcast_block_east_value"] = aie2ps::cm_event_broadcast_block_east_value;
   regNameToValue["cm_trace_control0"] = aie2ps::cm_trace_control0;
   regNameToValue["cm_trace_control1"] = aie2ps::cm_trace_control1;
   regNameToValue["cm_trace_status"] = aie2ps::cm_trace_status;
   regNameToValue["cm_trace_event0"] = aie2ps::cm_trace_event0;
   regNameToValue["cm_trace_event1"] = aie2ps::cm_trace_event1;
   regNameToValue["cm_timer_trig_event_low_value"] = aie2ps::cm_timer_trig_event_low_value;
   regNameToValue["cm_timer_trig_event_high_value"] = aie2ps::cm_timer_trig_event_high_value;
   regNameToValue["cm_timer_low"] = aie2ps::cm_timer_low;
   regNameToValue["cm_timer_high"] = aie2ps::cm_timer_high;
   regNameToValue["cm_event_status0"] = aie2ps::cm_event_status0;
   regNameToValue["cm_event_status1"] = aie2ps::cm_event_status1;
   regNameToValue["cm_event_status2"] = aie2ps::cm_event_status2;
   regNameToValue["cm_event_status3"] = aie2ps::cm_event_status3;
   regNameToValue["cm_combo_event_inputs"] = aie2ps::cm_combo_event_inputs;
   regNameToValue["cm_combo_event_control"] = aie2ps::cm_combo_event_control;
   regNameToValue["cm_edge_detection_event_control"] = aie2ps::cm_edge_detection_event_control;
   regNameToValue["cm_event_group_0_enable"] = aie2ps::cm_event_group_0_enable;
   regNameToValue["cm_event_group_pc_enable"] = aie2ps::cm_event_group_pc_enable;
   regNameToValue["cm_event_group_core_stall_enable"] = aie2ps::cm_event_group_core_stall_enable;
   regNameToValue["cm_event_group_core_program_flow_enable"] = aie2ps::cm_event_group_core_program_flow_enable;
   regNameToValue["cm_event_group_errors0_enable"] = aie2ps::cm_event_group_errors0_enable;
   regNameToValue["cm_event_group_errors1_enable"] = aie2ps::cm_event_group_errors1_enable;
   regNameToValue["cm_event_group_stream_switch_enable"] = aie2ps::cm_event_group_stream_switch_enable;
   regNameToValue["cm_event_group_broadcast_enable"] = aie2ps::cm_event_group_broadcast_enable;
   regNameToValue["cm_event_group_user_event_enable"] = aie2ps::cm_event_group_user_event_enable;
   regNameToValue["cm_cssd_trigger"] = aie2ps::cm_cssd_trigger;
   regNameToValue["cm_accumulator_control"] = aie2ps::cm_accumulator_control;
   regNameToValue["cm_memory_control"] = aie2ps::cm_memory_control;
   regNameToValue["cm_performance_control0"] = aie2ps::cm_performance_control0;
   regNameToValue["cm_performance_control1"] = aie2ps::cm_performance_control1;
   regNameToValue["cm_performance_control2"] = aie2ps::cm_performance_control2;
   regNameToValue["cm_performance_counter0"] = aie2ps::cm_performance_counter0;
   regNameToValue["cm_performance_counter1"] = aie2ps::cm_performance_counter1;
   regNameToValue["cm_performance_counter2"] = aie2ps::cm_performance_counter2;
   regNameToValue["cm_performance_counter3"] = aie2ps::cm_performance_counter3;
   regNameToValue["cm_performance_counter0_event_value"] = aie2ps::cm_performance_counter0_event_value;
   regNameToValue["cm_performance_counter1_event_value"] = aie2ps::cm_performance_counter1_event_value;
   regNameToValue["cm_performance_counter2_event_value"] = aie2ps::cm_performance_counter2_event_value;
   regNameToValue["cm_performance_counter3_event_value"] = aie2ps::cm_performance_counter3_event_value;
   regNameToValue["cm_core_control"] = aie2ps::cm_core_control;
   regNameToValue["cm_core_status"] = aie2ps::cm_core_status;
   regNameToValue["cm_enable_events"] = aie2ps::cm_enable_events;
   regNameToValue["cm_reset_event"] = aie2ps::cm_reset_event;
   regNameToValue["cm_debug_control0"] = aie2ps::cm_debug_control0;
   regNameToValue["cm_debug_control1"] = aie2ps::cm_debug_control1;
   regNameToValue["cm_debug_control2"] = aie2ps::cm_debug_control2;
   regNameToValue["cm_debug_status"] = aie2ps::cm_debug_status;
   regNameToValue["cm_pc_event0"] = aie2ps::cm_pc_event0;
   regNameToValue["cm_pc_event1"] = aie2ps::cm_pc_event1;
   regNameToValue["cm_pc_event2"] = aie2ps::cm_pc_event2;
   regNameToValue["cm_pc_event3"] = aie2ps::cm_pc_event3;
   regNameToValue["cm_error_halt_control"] = aie2ps::cm_error_halt_control;
   regNameToValue["cm_error_halt_event"] = aie2ps::cm_error_halt_event;
   regNameToValue["cm_core_processor_bus"] = aie2ps::cm_core_processor_bus;
   regNameToValue["cm_ecc_control"] = aie2ps::cm_ecc_control;
   regNameToValue["cm_ecc_scrubbing_event"] = aie2ps::cm_ecc_scrubbing_event;
   regNameToValue["cm_ecc_failing_address"] = aie2ps::cm_ecc_failing_address;
   regNameToValue["cm_stream_switch_master_config_aie_core0"] = aie2ps::cm_stream_switch_master_config_aie_core0;
   regNameToValue["cm_stream_switch_master_config_dma0"] = aie2ps::cm_stream_switch_master_config_dma0;
   regNameToValue["cm_stream_switch_master_config_dma1"] = aie2ps::cm_stream_switch_master_config_dma1;
   regNameToValue["cm_stream_switch_master_config_tile_ctrl"] = aie2ps::cm_stream_switch_master_config_tile_ctrl;
   regNameToValue["cm_stream_switch_master_config_fifo0"] = aie2ps::cm_stream_switch_master_config_fifo0;
   regNameToValue["cm_stream_switch_master_config_south0"] = aie2ps::cm_stream_switch_master_config_south0;
   regNameToValue["cm_stream_switch_master_config_south1"] = aie2ps::cm_stream_switch_master_config_south1;
   regNameToValue["cm_stream_switch_master_config_south2"] = aie2ps::cm_stream_switch_master_config_south2;
   regNameToValue["cm_stream_switch_master_config_south3"] = aie2ps::cm_stream_switch_master_config_south3;
   regNameToValue["cm_stream_switch_master_config_west0"] = aie2ps::cm_stream_switch_master_config_west0;
   regNameToValue["cm_stream_switch_master_config_west1"] = aie2ps::cm_stream_switch_master_config_west1;
   regNameToValue["cm_stream_switch_master_config_west2"] = aie2ps::cm_stream_switch_master_config_west2;
   regNameToValue["cm_stream_switch_master_config_west3"] = aie2ps::cm_stream_switch_master_config_west3;
   regNameToValue["cm_stream_switch_master_config_north0"] = aie2ps::cm_stream_switch_master_config_north0;
   regNameToValue["cm_stream_switch_master_config_north1"] = aie2ps::cm_stream_switch_master_config_north1;
   regNameToValue["cm_stream_switch_master_config_north2"] = aie2ps::cm_stream_switch_master_config_north2;
   regNameToValue["cm_stream_switch_master_config_north3"] = aie2ps::cm_stream_switch_master_config_north3;
   regNameToValue["cm_stream_switch_master_config_north4"] = aie2ps::cm_stream_switch_master_config_north4;
   regNameToValue["cm_stream_switch_master_config_north5"] = aie2ps::cm_stream_switch_master_config_north5;
   regNameToValue["cm_stream_switch_master_config_east0"] = aie2ps::cm_stream_switch_master_config_east0;
   regNameToValue["cm_stream_switch_master_config_east1"] = aie2ps::cm_stream_switch_master_config_east1;
   regNameToValue["cm_stream_switch_master_config_east2"] = aie2ps::cm_stream_switch_master_config_east2;
   regNameToValue["cm_stream_switch_master_config_east3"] = aie2ps::cm_stream_switch_master_config_east3;
   regNameToValue["cm_stream_switch_slave_config_aie_core0"] = aie2ps::cm_stream_switch_slave_config_aie_core0;
   regNameToValue["cm_stream_switch_slave_config_dma_0"] = aie2ps::cm_stream_switch_slave_config_dma_0;
   regNameToValue["cm_stream_switch_slave_config_dma_1"] = aie2ps::cm_stream_switch_slave_config_dma_1;
   regNameToValue["cm_stream_switch_slave_config_tile_ctrl"] = aie2ps::cm_stream_switch_slave_config_tile_ctrl;
   regNameToValue["cm_stream_switch_slave_config_fifo_0"] = aie2ps::cm_stream_switch_slave_config_fifo_0;
   regNameToValue["cm_stream_switch_slave_config_south_0"] = aie2ps::cm_stream_switch_slave_config_south_0;
   regNameToValue["cm_stream_switch_slave_config_south_1"] = aie2ps::cm_stream_switch_slave_config_south_1;
   regNameToValue["cm_stream_switch_slave_config_south_2"] = aie2ps::cm_stream_switch_slave_config_south_2;
   regNameToValue["cm_stream_switch_slave_config_south_3"] = aie2ps::cm_stream_switch_slave_config_south_3;
   regNameToValue["cm_stream_switch_slave_config_south_4"] = aie2ps::cm_stream_switch_slave_config_south_4;
   regNameToValue["cm_stream_switch_slave_config_south_5"] = aie2ps::cm_stream_switch_slave_config_south_5;
   regNameToValue["cm_stream_switch_slave_config_west_0"] = aie2ps::cm_stream_switch_slave_config_west_0;
   regNameToValue["cm_stream_switch_slave_config_west_1"] = aie2ps::cm_stream_switch_slave_config_west_1;
   regNameToValue["cm_stream_switch_slave_config_west_2"] = aie2ps::cm_stream_switch_slave_config_west_2;
   regNameToValue["cm_stream_switch_slave_config_west_3"] = aie2ps::cm_stream_switch_slave_config_west_3;
   regNameToValue["cm_stream_switch_slave_config_north_0"] = aie2ps::cm_stream_switch_slave_config_north_0;
   regNameToValue["cm_stream_switch_slave_config_north_1"] = aie2ps::cm_stream_switch_slave_config_north_1;
   regNameToValue["cm_stream_switch_slave_config_north_2"] = aie2ps::cm_stream_switch_slave_config_north_2;
   regNameToValue["cm_stream_switch_slave_config_north_3"] = aie2ps::cm_stream_switch_slave_config_north_3;
   regNameToValue["cm_stream_switch_slave_config_east_0"] = aie2ps::cm_stream_switch_slave_config_east_0;
   regNameToValue["cm_stream_switch_slave_config_east_1"] = aie2ps::cm_stream_switch_slave_config_east_1;
   regNameToValue["cm_stream_switch_slave_config_east_2"] = aie2ps::cm_stream_switch_slave_config_east_2;
   regNameToValue["cm_stream_switch_slave_config_east_3"] = aie2ps::cm_stream_switch_slave_config_east_3;
   regNameToValue["cm_stream_switch_slave_config_aie_trace"] = aie2ps::cm_stream_switch_slave_config_aie_trace;
   regNameToValue["cm_stream_switch_slave_config_mem_trace"] = aie2ps::cm_stream_switch_slave_config_mem_trace;
   regNameToValue["cm_stream_switch_slave_aie_core0_slot0"] = aie2ps::cm_stream_switch_slave_aie_core0_slot0;
   regNameToValue["cm_stream_switch_slave_aie_core0_slot1"] = aie2ps::cm_stream_switch_slave_aie_core0_slot1;
   regNameToValue["cm_stream_switch_slave_aie_core0_slot2"] = aie2ps::cm_stream_switch_slave_aie_core0_slot2;
   regNameToValue["cm_stream_switch_slave_aie_core0_slot3"] = aie2ps::cm_stream_switch_slave_aie_core0_slot3;
   regNameToValue["cm_stream_switch_slave_dma_0_slot0"] = aie2ps::cm_stream_switch_slave_dma_0_slot0;
   regNameToValue["cm_stream_switch_slave_dma_0_slot1"] = aie2ps::cm_stream_switch_slave_dma_0_slot1;
   regNameToValue["cm_stream_switch_slave_dma_0_slot2"] = aie2ps::cm_stream_switch_slave_dma_0_slot2;
   regNameToValue["cm_stream_switch_slave_dma_0_slot3"] = aie2ps::cm_stream_switch_slave_dma_0_slot3;
   regNameToValue["cm_stream_switch_slave_dma_1_slot0"] = aie2ps::cm_stream_switch_slave_dma_1_slot0;
   regNameToValue["cm_stream_switch_slave_dma_1_slot1"] = aie2ps::cm_stream_switch_slave_dma_1_slot1;
   regNameToValue["cm_stream_switch_slave_dma_1_slot2"] = aie2ps::cm_stream_switch_slave_dma_1_slot2;
   regNameToValue["cm_stream_switch_slave_dma_1_slot3"] = aie2ps::cm_stream_switch_slave_dma_1_slot3;
   regNameToValue["cm_stream_switch_slave_tile_ctrl_slot0"] = aie2ps::cm_stream_switch_slave_tile_ctrl_slot0;
   regNameToValue["cm_stream_switch_slave_tile_ctrl_slot1"] = aie2ps::cm_stream_switch_slave_tile_ctrl_slot1;
   regNameToValue["cm_stream_switch_slave_tile_ctrl_slot2"] = aie2ps::cm_stream_switch_slave_tile_ctrl_slot2;
   regNameToValue["cm_stream_switch_slave_tile_ctrl_slot3"] = aie2ps::cm_stream_switch_slave_tile_ctrl_slot3;
   regNameToValue["cm_stream_switch_slave_fifo_0_slot0"] = aie2ps::cm_stream_switch_slave_fifo_0_slot0;
   regNameToValue["cm_stream_switch_slave_fifo_0_slot1"] = aie2ps::cm_stream_switch_slave_fifo_0_slot1;
   regNameToValue["cm_stream_switch_slave_fifo_0_slot2"] = aie2ps::cm_stream_switch_slave_fifo_0_slot2;
   regNameToValue["cm_stream_switch_slave_fifo_0_slot3"] = aie2ps::cm_stream_switch_slave_fifo_0_slot3;
   regNameToValue["cm_stream_switch_slave_south_0_slot0"] = aie2ps::cm_stream_switch_slave_south_0_slot0;
   regNameToValue["cm_stream_switch_slave_south_0_slot1"] = aie2ps::cm_stream_switch_slave_south_0_slot1;
   regNameToValue["cm_stream_switch_slave_south_0_slot2"] = aie2ps::cm_stream_switch_slave_south_0_slot2;
   regNameToValue["cm_stream_switch_slave_south_0_slot3"] = aie2ps::cm_stream_switch_slave_south_0_slot3;
   regNameToValue["cm_stream_switch_slave_south_1_slot0"] = aie2ps::cm_stream_switch_slave_south_1_slot0;
   regNameToValue["cm_stream_switch_slave_south_1_slot1"] = aie2ps::cm_stream_switch_slave_south_1_slot1;
   regNameToValue["cm_stream_switch_slave_south_1_slot2"] = aie2ps::cm_stream_switch_slave_south_1_slot2;
   regNameToValue["cm_stream_switch_slave_south_1_slot3"] = aie2ps::cm_stream_switch_slave_south_1_slot3;
   regNameToValue["cm_stream_switch_slave_south_2_slot0"] = aie2ps::cm_stream_switch_slave_south_2_slot0;
   regNameToValue["cm_stream_switch_slave_south_2_slot1"] = aie2ps::cm_stream_switch_slave_south_2_slot1;
   regNameToValue["cm_stream_switch_slave_south_2_slot2"] = aie2ps::cm_stream_switch_slave_south_2_slot2;
   regNameToValue["cm_stream_switch_slave_south_2_slot3"] = aie2ps::cm_stream_switch_slave_south_2_slot3;
   regNameToValue["cm_stream_switch_slave_south_3_slot0"] = aie2ps::cm_stream_switch_slave_south_3_slot0;
   regNameToValue["cm_stream_switch_slave_south_3_slot1"] = aie2ps::cm_stream_switch_slave_south_3_slot1;
   regNameToValue["cm_stream_switch_slave_south_3_slot2"] = aie2ps::cm_stream_switch_slave_south_3_slot2;
   regNameToValue["cm_stream_switch_slave_south_3_slot3"] = aie2ps::cm_stream_switch_slave_south_3_slot3;
   regNameToValue["cm_stream_switch_slave_south_4_slot0"] = aie2ps::cm_stream_switch_slave_south_4_slot0;
   regNameToValue["cm_stream_switch_slave_south_4_slot1"] = aie2ps::cm_stream_switch_slave_south_4_slot1;
   regNameToValue["cm_stream_switch_slave_south_4_slot2"] = aie2ps::cm_stream_switch_slave_south_4_slot2;
   regNameToValue["cm_stream_switch_slave_south_4_slot3"] = aie2ps::cm_stream_switch_slave_south_4_slot3;
   regNameToValue["cm_stream_switch_slave_south_5_slot0"] = aie2ps::cm_stream_switch_slave_south_5_slot0;
   regNameToValue["cm_stream_switch_slave_south_5_slot1"] = aie2ps::cm_stream_switch_slave_south_5_slot1;
   regNameToValue["cm_stream_switch_slave_south_5_slot2"] = aie2ps::cm_stream_switch_slave_south_5_slot2;
   regNameToValue["cm_stream_switch_slave_south_5_slot3"] = aie2ps::cm_stream_switch_slave_south_5_slot3;
   regNameToValue["cm_stream_switch_slave_west_0_slot0"] = aie2ps::cm_stream_switch_slave_west_0_slot0;
   regNameToValue["cm_stream_switch_slave_west_0_slot1"] = aie2ps::cm_stream_switch_slave_west_0_slot1;
   regNameToValue["cm_stream_switch_slave_west_0_slot2"] = aie2ps::cm_stream_switch_slave_west_0_slot2;
   regNameToValue["cm_stream_switch_slave_west_0_slot3"] = aie2ps::cm_stream_switch_slave_west_0_slot3;
   regNameToValue["cm_stream_switch_slave_west_1_slot0"] = aie2ps::cm_stream_switch_slave_west_1_slot0;
   regNameToValue["cm_stream_switch_slave_west_1_slot1"] = aie2ps::cm_stream_switch_slave_west_1_slot1;
   regNameToValue["cm_stream_switch_slave_west_1_slot2"] = aie2ps::cm_stream_switch_slave_west_1_slot2;
   regNameToValue["cm_stream_switch_slave_west_1_slot3"] = aie2ps::cm_stream_switch_slave_west_1_slot3;
   regNameToValue["cm_stream_switch_slave_west_2_slot0"] = aie2ps::cm_stream_switch_slave_west_2_slot0;
   regNameToValue["cm_stream_switch_slave_west_2_slot1"] = aie2ps::cm_stream_switch_slave_west_2_slot1;
   regNameToValue["cm_stream_switch_slave_west_2_slot2"] = aie2ps::cm_stream_switch_slave_west_2_slot2;
   regNameToValue["cm_stream_switch_slave_west_2_slot3"] = aie2ps::cm_stream_switch_slave_west_2_slot3;
   regNameToValue["cm_stream_switch_slave_west_3_slot0"] = aie2ps::cm_stream_switch_slave_west_3_slot0;
   regNameToValue["cm_stream_switch_slave_west_3_slot1"] = aie2ps::cm_stream_switch_slave_west_3_slot1;
   regNameToValue["cm_stream_switch_slave_west_3_slot2"] = aie2ps::cm_stream_switch_slave_west_3_slot2;
   regNameToValue["cm_stream_switch_slave_west_3_slot3"] = aie2ps::cm_stream_switch_slave_west_3_slot3;
   regNameToValue["cm_stream_switch_slave_north_0_slot0"] = aie2ps::cm_stream_switch_slave_north_0_slot0;
   regNameToValue["cm_stream_switch_slave_north_0_slot1"] = aie2ps::cm_stream_switch_slave_north_0_slot1;
   regNameToValue["cm_stream_switch_slave_north_0_slot2"] = aie2ps::cm_stream_switch_slave_north_0_slot2;
   regNameToValue["cm_stream_switch_slave_north_0_slot3"] = aie2ps::cm_stream_switch_slave_north_0_slot3;
   regNameToValue["cm_stream_switch_slave_north_1_slot0"] = aie2ps::cm_stream_switch_slave_north_1_slot0;
   regNameToValue["cm_stream_switch_slave_north_1_slot1"] = aie2ps::cm_stream_switch_slave_north_1_slot1;
   regNameToValue["cm_stream_switch_slave_north_1_slot2"] = aie2ps::cm_stream_switch_slave_north_1_slot2;
   regNameToValue["cm_stream_switch_slave_north_1_slot3"] = aie2ps::cm_stream_switch_slave_north_1_slot3;
   regNameToValue["cm_stream_switch_slave_north_2_slot0"] = aie2ps::cm_stream_switch_slave_north_2_slot0;
   regNameToValue["cm_stream_switch_slave_north_2_slot1"] = aie2ps::cm_stream_switch_slave_north_2_slot1;
   regNameToValue["cm_stream_switch_slave_north_2_slot2"] = aie2ps::cm_stream_switch_slave_north_2_slot2;
   regNameToValue["cm_stream_switch_slave_north_2_slot3"] = aie2ps::cm_stream_switch_slave_north_2_slot3;
   regNameToValue["cm_stream_switch_slave_north_3_slot0"] = aie2ps::cm_stream_switch_slave_north_3_slot0;
   regNameToValue["cm_stream_switch_slave_north_3_slot1"] = aie2ps::cm_stream_switch_slave_north_3_slot1;
   regNameToValue["cm_stream_switch_slave_north_3_slot2"] = aie2ps::cm_stream_switch_slave_north_3_slot2;
   regNameToValue["cm_stream_switch_slave_north_3_slot3"] = aie2ps::cm_stream_switch_slave_north_3_slot3;
   regNameToValue["cm_stream_switch_slave_east_0_slot0"] = aie2ps::cm_stream_switch_slave_east_0_slot0;
   regNameToValue["cm_stream_switch_slave_east_0_slot1"] = aie2ps::cm_stream_switch_slave_east_0_slot1;
   regNameToValue["cm_stream_switch_slave_east_0_slot2"] = aie2ps::cm_stream_switch_slave_east_0_slot2;
   regNameToValue["cm_stream_switch_slave_east_0_slot3"] = aie2ps::cm_stream_switch_slave_east_0_slot3;
   regNameToValue["cm_stream_switch_slave_east_1_slot0"] = aie2ps::cm_stream_switch_slave_east_1_slot0;
   regNameToValue["cm_stream_switch_slave_east_1_slot1"] = aie2ps::cm_stream_switch_slave_east_1_slot1;
   regNameToValue["cm_stream_switch_slave_east_1_slot2"] = aie2ps::cm_stream_switch_slave_east_1_slot2;
   regNameToValue["cm_stream_switch_slave_east_1_slot3"] = aie2ps::cm_stream_switch_slave_east_1_slot3;
   regNameToValue["cm_stream_switch_slave_east_2_slot0"] = aie2ps::cm_stream_switch_slave_east_2_slot0;
   regNameToValue["cm_stream_switch_slave_east_2_slot1"] = aie2ps::cm_stream_switch_slave_east_2_slot1;
   regNameToValue["cm_stream_switch_slave_east_2_slot2"] = aie2ps::cm_stream_switch_slave_east_2_slot2;
   regNameToValue["cm_stream_switch_slave_east_2_slot3"] = aie2ps::cm_stream_switch_slave_east_2_slot3;
   regNameToValue["cm_stream_switch_slave_east_3_slot0"] = aie2ps::cm_stream_switch_slave_east_3_slot0;
   regNameToValue["cm_stream_switch_slave_east_3_slot1"] = aie2ps::cm_stream_switch_slave_east_3_slot1;
   regNameToValue["cm_stream_switch_slave_east_3_slot2"] = aie2ps::cm_stream_switch_slave_east_3_slot2;
   regNameToValue["cm_stream_switch_slave_east_3_slot3"] = aie2ps::cm_stream_switch_slave_east_3_slot3;
   regNameToValue["cm_stream_switch_slave_aie_trace_slot0"] = aie2ps::cm_stream_switch_slave_aie_trace_slot0;
   regNameToValue["cm_stream_switch_slave_aie_trace_slot1"] = aie2ps::cm_stream_switch_slave_aie_trace_slot1;
   regNameToValue["cm_stream_switch_slave_aie_trace_slot2"] = aie2ps::cm_stream_switch_slave_aie_trace_slot2;
   regNameToValue["cm_stream_switch_slave_aie_trace_slot3"] = aie2ps::cm_stream_switch_slave_aie_trace_slot3;
   regNameToValue["cm_stream_switch_slave_mem_trace_slot0"] = aie2ps::cm_stream_switch_slave_mem_trace_slot0;
   regNameToValue["cm_stream_switch_slave_mem_trace_slot1"] = aie2ps::cm_stream_switch_slave_mem_trace_slot1;
   regNameToValue["cm_stream_switch_slave_mem_trace_slot2"] = aie2ps::cm_stream_switch_slave_mem_trace_slot2;
   regNameToValue["cm_stream_switch_slave_mem_trace_slot3"] = aie2ps::cm_stream_switch_slave_mem_trace_slot3;
   regNameToValue["cm_stream_switch_deterministic_merge_arb0_slave0_1"] = aie2ps::cm_stream_switch_deterministic_merge_arb0_slave0_1;
   regNameToValue["cm_stream_switch_deterministic_merge_arb0_slave2_3"] = aie2ps::cm_stream_switch_deterministic_merge_arb0_slave2_3;
   regNameToValue["cm_stream_switch_deterministic_merge_arb0_ctrl"] = aie2ps::cm_stream_switch_deterministic_merge_arb0_ctrl;
   regNameToValue["cm_stream_switch_deterministic_merge_arb1_slave0_1"] = aie2ps::cm_stream_switch_deterministic_merge_arb1_slave0_1;
   regNameToValue["cm_stream_switch_deterministic_merge_arb1_slave2_3"] = aie2ps::cm_stream_switch_deterministic_merge_arb1_slave2_3;
   regNameToValue["cm_stream_switch_deterministic_merge_arb1_ctrl"] = aie2ps::cm_stream_switch_deterministic_merge_arb1_ctrl;
   regNameToValue["cm_stream_switch_event_port_selection_0"] = aie2ps::cm_stream_switch_event_port_selection_0;
   regNameToValue["cm_stream_switch_event_port_selection_1"] = aie2ps::cm_stream_switch_event_port_selection_1;
   regNameToValue["cm_stream_switch_parity_status"] = aie2ps::cm_stream_switch_parity_status;
   regNameToValue["cm_stream_switch_parity_injection"] = aie2ps::cm_stream_switch_parity_injection;
   regNameToValue["cm_tile_control_packet_handler_status"] = aie2ps::cm_tile_control_packet_handler_status;
   regNameToValue["cm_stream_switch_adaptive_clock_gate_status"] = aie2ps::cm_stream_switch_adaptive_clock_gate_status;
   regNameToValue["cm_stream_switch_adaptive_clock_gate_abort_period"] = aie2ps::cm_stream_switch_adaptive_clock_gate_abort_period;
   regNameToValue["cm_module_clock_control"] = aie2ps::cm_module_clock_control;
   regNameToValue["cm_module_reset_control"] = aie2ps::cm_module_reset_control;
   regNameToValue["cm_tile_control"] = aie2ps::cm_tile_control;
   regNameToValue["cm_core_reset_defeature"] = aie2ps::cm_core_reset_defeature;
   regNameToValue["cm_spare_reg_privileged"] = aie2ps::cm_spare_reg_privileged;
   regNameToValue["cm_spare_reg"] = aie2ps::cm_spare_reg;
   regNameToValue["mm_checkbit_error_generation"] = aie2ps::mm_checkbit_error_generation;
   regNameToValue["mm_combo_event_control"] = aie2ps::mm_combo_event_control;
   regNameToValue["mm_combo_event_inputs"] = aie2ps::mm_combo_event_inputs;
   regNameToValue["mm_dma_bd0_0"] = aie2ps::mm_dma_bd0_0;
   regNameToValue["mm_dma_bd0_1"] = aie2ps::mm_dma_bd0_1;
   regNameToValue["mm_dma_bd0_2"] = aie2ps::mm_dma_bd0_2;
   regNameToValue["mm_dma_bd0_3"] = aie2ps::mm_dma_bd0_3;
   regNameToValue["mm_dma_bd0_4"] = aie2ps::mm_dma_bd0_4;
   regNameToValue["mm_dma_bd0_5"] = aie2ps::mm_dma_bd0_5;
   regNameToValue["mm_dma_bd10_0"] = aie2ps::mm_dma_bd10_0;
   regNameToValue["mm_dma_bd10_1"] = aie2ps::mm_dma_bd10_1;
   regNameToValue["mm_dma_bd10_2"] = aie2ps::mm_dma_bd10_2;
   regNameToValue["mm_dma_bd10_3"] = aie2ps::mm_dma_bd10_3;
   regNameToValue["mm_dma_bd10_4"] = aie2ps::mm_dma_bd10_4;
   regNameToValue["mm_dma_bd10_5"] = aie2ps::mm_dma_bd10_5;
   regNameToValue["mm_dma_bd11_0"] = aie2ps::mm_dma_bd11_0;
   regNameToValue["mm_dma_bd11_1"] = aie2ps::mm_dma_bd11_1;
   regNameToValue["mm_dma_bd11_2"] = aie2ps::mm_dma_bd11_2;
   regNameToValue["mm_dma_bd11_3"] = aie2ps::mm_dma_bd11_3;
   regNameToValue["mm_dma_bd11_4"] = aie2ps::mm_dma_bd11_4;
   regNameToValue["mm_dma_bd11_5"] = aie2ps::mm_dma_bd11_5;
   regNameToValue["mm_dma_bd12_0"] = aie2ps::mm_dma_bd12_0;
   regNameToValue["mm_dma_bd12_1"] = aie2ps::mm_dma_bd12_1;
   regNameToValue["mm_dma_bd12_2"] = aie2ps::mm_dma_bd12_2;
   regNameToValue["mm_dma_bd12_3"] = aie2ps::mm_dma_bd12_3;
   regNameToValue["mm_dma_bd12_4"] = aie2ps::mm_dma_bd12_4;
   regNameToValue["mm_dma_bd12_5"] = aie2ps::mm_dma_bd12_5;
   regNameToValue["mm_dma_bd13_0"] = aie2ps::mm_dma_bd13_0;
   regNameToValue["mm_dma_bd13_1"] = aie2ps::mm_dma_bd13_1;
   regNameToValue["mm_dma_bd13_2"] = aie2ps::mm_dma_bd13_2;
   regNameToValue["mm_dma_bd13_3"] = aie2ps::mm_dma_bd13_3;
   regNameToValue["mm_dma_bd13_4"] = aie2ps::mm_dma_bd13_4;
   regNameToValue["mm_dma_bd13_5"] = aie2ps::mm_dma_bd13_5;
   regNameToValue["mm_dma_bd14_0"] = aie2ps::mm_dma_bd14_0;
   regNameToValue["mm_dma_bd14_1"] = aie2ps::mm_dma_bd14_1;
   regNameToValue["mm_dma_bd14_2"] = aie2ps::mm_dma_bd14_2;
   regNameToValue["mm_dma_bd14_3"] = aie2ps::mm_dma_bd14_3;
   regNameToValue["mm_dma_bd14_4"] = aie2ps::mm_dma_bd14_4;
   regNameToValue["mm_dma_bd14_5"] = aie2ps::mm_dma_bd14_5;
   regNameToValue["mm_dma_bd15_0"] = aie2ps::mm_dma_bd15_0;
   regNameToValue["mm_dma_bd15_1"] = aie2ps::mm_dma_bd15_1;
   regNameToValue["mm_dma_bd15_2"] = aie2ps::mm_dma_bd15_2;
   regNameToValue["mm_dma_bd15_3"] = aie2ps::mm_dma_bd15_3;
   regNameToValue["mm_dma_bd15_4"] = aie2ps::mm_dma_bd15_4;
   regNameToValue["mm_dma_bd15_5"] = aie2ps::mm_dma_bd15_5;
   regNameToValue["mm_dma_bd1_0"] = aie2ps::mm_dma_bd1_0;
   regNameToValue["mm_dma_bd1_1"] = aie2ps::mm_dma_bd1_1;
   regNameToValue["mm_dma_bd1_2"] = aie2ps::mm_dma_bd1_2;
   regNameToValue["mm_dma_bd1_3"] = aie2ps::mm_dma_bd1_3;
   regNameToValue["mm_dma_bd1_4"] = aie2ps::mm_dma_bd1_4;
   regNameToValue["mm_dma_bd1_5"] = aie2ps::mm_dma_bd1_5;
   regNameToValue["mm_dma_bd2_0"] = aie2ps::mm_dma_bd2_0;
   regNameToValue["mm_dma_bd2_1"] = aie2ps::mm_dma_bd2_1;
   regNameToValue["mm_dma_bd2_2"] = aie2ps::mm_dma_bd2_2;
   regNameToValue["mm_dma_bd2_3"] = aie2ps::mm_dma_bd2_3;
   regNameToValue["mm_dma_bd2_4"] = aie2ps::mm_dma_bd2_4;
   regNameToValue["mm_dma_bd2_5"] = aie2ps::mm_dma_bd2_5;
   regNameToValue["mm_dma_bd3_0"] = aie2ps::mm_dma_bd3_0;
   regNameToValue["mm_dma_bd3_1"] = aie2ps::mm_dma_bd3_1;
   regNameToValue["mm_dma_bd3_2"] = aie2ps::mm_dma_bd3_2;
   regNameToValue["mm_dma_bd3_3"] = aie2ps::mm_dma_bd3_3;
   regNameToValue["mm_dma_bd3_4"] = aie2ps::mm_dma_bd3_4;
   regNameToValue["mm_dma_bd3_5"] = aie2ps::mm_dma_bd3_5;
   regNameToValue["mm_dma_bd4_0"] = aie2ps::mm_dma_bd4_0;
   regNameToValue["mm_dma_bd4_1"] = aie2ps::mm_dma_bd4_1;
   regNameToValue["mm_dma_bd4_2"] = aie2ps::mm_dma_bd4_2;
   regNameToValue["mm_dma_bd4_3"] = aie2ps::mm_dma_bd4_3;
   regNameToValue["mm_dma_bd4_4"] = aie2ps::mm_dma_bd4_4;
   regNameToValue["mm_dma_bd4_5"] = aie2ps::mm_dma_bd4_5;
   regNameToValue["mm_dma_bd5_0"] = aie2ps::mm_dma_bd5_0;
   regNameToValue["mm_dma_bd5_1"] = aie2ps::mm_dma_bd5_1;
   regNameToValue["mm_dma_bd5_2"] = aie2ps::mm_dma_bd5_2;
   regNameToValue["mm_dma_bd5_3"] = aie2ps::mm_dma_bd5_3;
   regNameToValue["mm_dma_bd5_4"] = aie2ps::mm_dma_bd5_4;
   regNameToValue["mm_dma_bd5_5"] = aie2ps::mm_dma_bd5_5;
   regNameToValue["mm_dma_bd6_0"] = aie2ps::mm_dma_bd6_0;
   regNameToValue["mm_dma_bd6_1"] = aie2ps::mm_dma_bd6_1;
   regNameToValue["mm_dma_bd6_2"] = aie2ps::mm_dma_bd6_2;
   regNameToValue["mm_dma_bd6_3"] = aie2ps::mm_dma_bd6_3;
   regNameToValue["mm_dma_bd6_4"] = aie2ps::mm_dma_bd6_4;
   regNameToValue["mm_dma_bd6_5"] = aie2ps::mm_dma_bd6_5;
   regNameToValue["mm_dma_bd7_0"] = aie2ps::mm_dma_bd7_0;
   regNameToValue["mm_dma_bd7_1"] = aie2ps::mm_dma_bd7_1;
   regNameToValue["mm_dma_bd7_2"] = aie2ps::mm_dma_bd7_2;
   regNameToValue["mm_dma_bd7_3"] = aie2ps::mm_dma_bd7_3;
   regNameToValue["mm_dma_bd7_4"] = aie2ps::mm_dma_bd7_4;
   regNameToValue["mm_dma_bd7_5"] = aie2ps::mm_dma_bd7_5;
   regNameToValue["mm_dma_bd8_0"] = aie2ps::mm_dma_bd8_0;
   regNameToValue["mm_dma_bd8_1"] = aie2ps::mm_dma_bd8_1;
   regNameToValue["mm_dma_bd8_2"] = aie2ps::mm_dma_bd8_2;
   regNameToValue["mm_dma_bd8_3"] = aie2ps::mm_dma_bd8_3;
   regNameToValue["mm_dma_bd8_4"] = aie2ps::mm_dma_bd8_4;
   regNameToValue["mm_dma_bd8_5"] = aie2ps::mm_dma_bd8_5;
   regNameToValue["mm_dma_bd9_0"] = aie2ps::mm_dma_bd9_0;
   regNameToValue["mm_dma_bd9_1"] = aie2ps::mm_dma_bd9_1;
   regNameToValue["mm_dma_bd9_2"] = aie2ps::mm_dma_bd9_2;
   regNameToValue["mm_dma_bd9_3"] = aie2ps::mm_dma_bd9_3;
   regNameToValue["mm_dma_bd9_4"] = aie2ps::mm_dma_bd9_4;
   regNameToValue["mm_dma_bd9_5"] = aie2ps::mm_dma_bd9_5;
   regNameToValue["mm_dma_mm2s_0_ctrl"] = aie2ps::mm_dma_mm2s_0_ctrl;
   regNameToValue["mm_dma_mm2s_0_start_queue"] = aie2ps::mm_dma_mm2s_0_start_queue;
   regNameToValue["mm_dma_mm2s_1_ctrl"] = aie2ps::mm_dma_mm2s_1_ctrl;
   regNameToValue["mm_dma_mm2s_1_start_queue"] = aie2ps::mm_dma_mm2s_1_start_queue;
   regNameToValue["mm_dma_mm2s_status_0"] = aie2ps::mm_dma_mm2s_status_0;
   regNameToValue["mm_dma_mm2s_status_1"] = aie2ps::mm_dma_mm2s_status_1;
   regNameToValue["mm_dma_s2mm_0_ctrl"] = aie2ps::mm_dma_s2mm_0_ctrl;
   regNameToValue["mm_dma_s2mm_0_start_queue"] = aie2ps::mm_dma_s2mm_0_start_queue;
   regNameToValue["mm_dma_s2mm_1_ctrl"] = aie2ps::mm_dma_s2mm_1_ctrl;
   regNameToValue["mm_dma_s2mm_1_start_queue"] = aie2ps::mm_dma_s2mm_1_start_queue;
   regNameToValue["mm_dma_s2mm_current_write_count_0"] = aie2ps::mm_dma_s2mm_current_write_count_0;
   regNameToValue["mm_dma_s2mm_current_write_count_1"] = aie2ps::mm_dma_s2mm_current_write_count_1;
   regNameToValue["mm_dma_s2mm_fot_count_fifo_pop_0"] = aie2ps::mm_dma_s2mm_fot_count_fifo_pop_0;
   regNameToValue["mm_dma_s2mm_fot_count_fifo_pop_1"] = aie2ps::mm_dma_s2mm_fot_count_fifo_pop_1;
   regNameToValue["mm_dma_s2mm_status_0"] = aie2ps::mm_dma_s2mm_status_0;
   regNameToValue["mm_dma_s2mm_status_1"] = aie2ps::mm_dma_s2mm_status_1;
   regNameToValue["mm_ecc_failing_address"] = aie2ps::mm_ecc_failing_address;
   regNameToValue["mm_ecc_scrubbing_event"] = aie2ps::mm_ecc_scrubbing_event;
   regNameToValue["mm_edge_detection_event_control"] = aie2ps::mm_edge_detection_event_control;
   regNameToValue["mm_event_broadcast0"] = aie2ps::mm_event_broadcast0;
   regNameToValue["mm_event_broadcast1"] = aie2ps::mm_event_broadcast1;
   regNameToValue["mm_event_broadcast10"] = aie2ps::mm_event_broadcast10;
   regNameToValue["mm_event_broadcast11"] = aie2ps::mm_event_broadcast11;
   regNameToValue["mm_event_broadcast12"] = aie2ps::mm_event_broadcast12;
   regNameToValue["mm_event_broadcast13"] = aie2ps::mm_event_broadcast13;
   regNameToValue["mm_event_broadcast14"] = aie2ps::mm_event_broadcast14;
   regNameToValue["mm_event_broadcast15"] = aie2ps::mm_event_broadcast15;
   regNameToValue["mm_event_broadcast2"] = aie2ps::mm_event_broadcast2;
   regNameToValue["mm_event_broadcast3"] = aie2ps::mm_event_broadcast3;
   regNameToValue["mm_event_broadcast4"] = aie2ps::mm_event_broadcast4;
   regNameToValue["mm_event_broadcast5"] = aie2ps::mm_event_broadcast5;
   regNameToValue["mm_event_broadcast6"] = aie2ps::mm_event_broadcast6;
   regNameToValue["mm_event_broadcast7"] = aie2ps::mm_event_broadcast7;
   regNameToValue["mm_event_broadcast8"] = aie2ps::mm_event_broadcast8;
   regNameToValue["mm_event_broadcast9"] = aie2ps::mm_event_broadcast9;
   regNameToValue["mm_event_broadcast_block_east_clr"] = aie2ps::mm_event_broadcast_block_east_clr;
   regNameToValue["mm_event_broadcast_block_east_set"] = aie2ps::mm_event_broadcast_block_east_set;
   regNameToValue["mm_event_broadcast_block_east_value"] = aie2ps::mm_event_broadcast_block_east_value;
   regNameToValue["mm_event_broadcast_block_north_clr"] = aie2ps::mm_event_broadcast_block_north_clr;
   regNameToValue["mm_event_broadcast_block_north_set"] = aie2ps::mm_event_broadcast_block_north_set;
   regNameToValue["mm_event_broadcast_block_north_value"] = aie2ps::mm_event_broadcast_block_north_value;
   regNameToValue["mm_event_broadcast_block_south_clr"] = aie2ps::mm_event_broadcast_block_south_clr;
   regNameToValue["mm_event_broadcast_block_south_set"] = aie2ps::mm_event_broadcast_block_south_set;
   regNameToValue["mm_event_broadcast_block_south_value"] = aie2ps::mm_event_broadcast_block_south_value;
   regNameToValue["mm_event_broadcast_block_west_clr"] = aie2ps::mm_event_broadcast_block_west_clr;
   regNameToValue["mm_event_broadcast_block_west_set"] = aie2ps::mm_event_broadcast_block_west_set;
   regNameToValue["mm_event_broadcast_block_west_value"] = aie2ps::mm_event_broadcast_block_west_value;
   regNameToValue["mm_event_generate"] = aie2ps::mm_event_generate;
   regNameToValue["mm_event_group_0_enable"] = aie2ps::mm_event_group_0_enable;
   regNameToValue["mm_event_group_broadcast_enable"] = aie2ps::mm_event_group_broadcast_enable;
   regNameToValue["mm_event_group_dma_enable"] = aie2ps::mm_event_group_dma_enable;
   regNameToValue["mm_event_group_error_enable"] = aie2ps::mm_event_group_error_enable;
   regNameToValue["mm_event_group_lock_enable"] = aie2ps::mm_event_group_lock_enable;
   regNameToValue["mm_event_group_memory_conflict_enable"] = aie2ps::mm_event_group_memory_conflict_enable;
   regNameToValue["mm_event_group_user_event_enable"] = aie2ps::mm_event_group_user_event_enable;
   regNameToValue["mm_event_group_watchpoint_enable"] = aie2ps::mm_event_group_watchpoint_enable;
   regNameToValue["mm_event_status0"] = aie2ps::mm_event_status0;
   regNameToValue["mm_event_status1"] = aie2ps::mm_event_status1;
   regNameToValue["mm_event_status2"] = aie2ps::mm_event_status2;
   regNameToValue["mm_event_status3"] = aie2ps::mm_event_status3;
   regNameToValue["mm_lock0_value"] = aie2ps::mm_lock0_value;
   regNameToValue["mm_lock10_value"] = aie2ps::mm_lock10_value;
   regNameToValue["mm_lock11_value"] = aie2ps::mm_lock11_value;
   regNameToValue["mm_lock12_value"] = aie2ps::mm_lock12_value;
   regNameToValue["mm_lock13_value"] = aie2ps::mm_lock13_value;
   regNameToValue["mm_lock14_value"] = aie2ps::mm_lock14_value;
   regNameToValue["mm_lock15_value"] = aie2ps::mm_lock15_value;
   regNameToValue["mm_lock1_value"] = aie2ps::mm_lock1_value;
   regNameToValue["mm_lock2_value"] = aie2ps::mm_lock2_value;
   regNameToValue["mm_lock3_value"] = aie2ps::mm_lock3_value;
   regNameToValue["mm_lock4_value"] = aie2ps::mm_lock4_value;
   regNameToValue["mm_lock5_value"] = aie2ps::mm_lock5_value;
   regNameToValue["mm_lock6_value"] = aie2ps::mm_lock6_value;
   regNameToValue["mm_lock7_value"] = aie2ps::mm_lock7_value;
   regNameToValue["mm_lock8_value"] = aie2ps::mm_lock8_value;
   regNameToValue["mm_lock9_value"] = aie2ps::mm_lock9_value;
   regNameToValue["mm_lock_request"] = aie2ps::mm_lock_request;
   regNameToValue["mm_locks_event_selection_0"] = aie2ps::mm_locks_event_selection_0;
   regNameToValue["mm_locks_event_selection_1"] = aie2ps::mm_locks_event_selection_1;
   regNameToValue["mm_locks_event_selection_2"] = aie2ps::mm_locks_event_selection_2;
   regNameToValue["mm_locks_event_selection_3"] = aie2ps::mm_locks_event_selection_3;
   regNameToValue["mm_locks_event_selection_4"] = aie2ps::mm_locks_event_selection_4;
   regNameToValue["mm_locks_event_selection_5"] = aie2ps::mm_locks_event_selection_5;
   regNameToValue["mm_locks_event_selection_6"] = aie2ps::mm_locks_event_selection_6;
   regNameToValue["mm_locks_event_selection_7"] = aie2ps::mm_locks_event_selection_7;
   regNameToValue["mm_locks_overflow"] = aie2ps::mm_locks_overflow;
   regNameToValue["mm_locks_underflow"] = aie2ps::mm_locks_underflow;
   regNameToValue["mm_memory_control"] = aie2ps::mm_memory_control;
   regNameToValue["mm_parity_failing_address"] = aie2ps::mm_parity_failing_address;
   regNameToValue["mm_performance_control0"] = aie2ps::mm_performance_control0;
   regNameToValue["mm_performance_control1"] = aie2ps::mm_performance_control1;
   regNameToValue["mm_performance_control2"] = aie2ps::mm_performance_control2;
   regNameToValue["mm_performance_control3"] = aie2ps::mm_performance_control3;
   regNameToValue["mm_performance_counter0"] = aie2ps::mm_performance_counter0;
   regNameToValue["mm_performance_counter0_event_value"] = aie2ps::mm_performance_counter0_event_value;
   regNameToValue["mm_performance_counter1"] = aie2ps::mm_performance_counter1;
   regNameToValue["mm_performance_counter1_event_value"] = aie2ps::mm_performance_counter1_event_value;
   regNameToValue["mm_performance_counter2"] = aie2ps::mm_performance_counter2;
   regNameToValue["mm_performance_counter3"] = aie2ps::mm_performance_counter3;
   regNameToValue["mm_spare_reg"] = aie2ps::mm_spare_reg;
   regNameToValue["mm_timer_control"] = aie2ps::mm_timer_control;
   regNameToValue["mm_timer_high"] = aie2ps::mm_timer_high;
   regNameToValue["mm_timer_low"] = aie2ps::mm_timer_low;
   regNameToValue["mm_timer_trig_event_high_value"] = aie2ps::mm_timer_trig_event_high_value;
   regNameToValue["mm_timer_trig_event_low_value"] = aie2ps::mm_timer_trig_event_low_value;
   regNameToValue["mm_trace_control0"] = aie2ps::mm_trace_control0;
   regNameToValue["mm_trace_control1"] = aie2ps::mm_trace_control1;
   regNameToValue["mm_trace_event0"] = aie2ps::mm_trace_event0;
   regNameToValue["mm_trace_event1"] = aie2ps::mm_trace_event1;
   regNameToValue["mm_trace_status"] = aie2ps::mm_trace_status;
   regNameToValue["mm_watchpoint0"] = aie2ps::mm_watchpoint0;
   regNameToValue["mm_watchpoint1"] = aie2ps::mm_watchpoint1;
   regNameToValue["mem_cssd_trigger"] = aie2ps::mem_cssd_trigger;
   regNameToValue["mem_checkbit_error_generation"] = aie2ps::mem_checkbit_error_generation;
   regNameToValue["mem_combo_event_control"] = aie2ps::mem_combo_event_control;
   regNameToValue["mem_combo_event_inputs"] = aie2ps::mem_combo_event_inputs;
   regNameToValue["mem_dma_bd0_0"] = aie2ps::mem_dma_bd0_0;
   regNameToValue["mem_dma_bd0_1"] = aie2ps::mem_dma_bd0_1;
   regNameToValue["mem_dma_bd0_2"] = aie2ps::mem_dma_bd0_2;
   regNameToValue["mem_dma_bd0_3"] = aie2ps::mem_dma_bd0_3;
   regNameToValue["mem_dma_bd0_4"] = aie2ps::mem_dma_bd0_4;
   regNameToValue["mem_dma_bd0_5"] = aie2ps::mem_dma_bd0_5;
   regNameToValue["mem_dma_bd0_6"] = aie2ps::mem_dma_bd0_6;
   regNameToValue["mem_dma_bd0_7"] = aie2ps::mem_dma_bd0_7;
   regNameToValue["mem_dma_bd10_0"] = aie2ps::mem_dma_bd10_0;
   regNameToValue["mem_dma_bd10_1"] = aie2ps::mem_dma_bd10_1;
   regNameToValue["mem_dma_bd10_2"] = aie2ps::mem_dma_bd10_2;
   regNameToValue["mem_dma_bd10_3"] = aie2ps::mem_dma_bd10_3;
   regNameToValue["mem_dma_bd10_4"] = aie2ps::mem_dma_bd10_4;
   regNameToValue["mem_dma_bd10_5"] = aie2ps::mem_dma_bd10_5;
   regNameToValue["mem_dma_bd10_6"] = aie2ps::mem_dma_bd10_6;
   regNameToValue["mem_dma_bd10_7"] = aie2ps::mem_dma_bd10_7;
   regNameToValue["mem_dma_bd11_0"] = aie2ps::mem_dma_bd11_0;
   regNameToValue["mem_dma_bd11_1"] = aie2ps::mem_dma_bd11_1;
   regNameToValue["mem_dma_bd11_2"] = aie2ps::mem_dma_bd11_2;
   regNameToValue["mem_dma_bd11_3"] = aie2ps::mem_dma_bd11_3;
   regNameToValue["mem_dma_bd11_4"] = aie2ps::mem_dma_bd11_4;
   regNameToValue["mem_dma_bd11_5"] = aie2ps::mem_dma_bd11_5;
   regNameToValue["mem_dma_bd11_6"] = aie2ps::mem_dma_bd11_6;
   regNameToValue["mem_dma_bd11_7"] = aie2ps::mem_dma_bd11_7;
   regNameToValue["mem_dma_bd12_0"] = aie2ps::mem_dma_bd12_0;
   regNameToValue["mem_dma_bd12_1"] = aie2ps::mem_dma_bd12_1;
   regNameToValue["mem_dma_bd12_2"] = aie2ps::mem_dma_bd12_2;
   regNameToValue["mem_dma_bd12_3"] = aie2ps::mem_dma_bd12_3;
   regNameToValue["mem_dma_bd12_4"] = aie2ps::mem_dma_bd12_4;
   regNameToValue["mem_dma_bd12_5"] = aie2ps::mem_dma_bd12_5;
   regNameToValue["mem_dma_bd12_6"] = aie2ps::mem_dma_bd12_6;
   regNameToValue["mem_dma_bd12_7"] = aie2ps::mem_dma_bd12_7;
   regNameToValue["mem_dma_bd13_0"] = aie2ps::mem_dma_bd13_0;
   regNameToValue["mem_dma_bd13_1"] = aie2ps::mem_dma_bd13_1;
   regNameToValue["mem_dma_bd13_2"] = aie2ps::mem_dma_bd13_2;
   regNameToValue["mem_dma_bd13_3"] = aie2ps::mem_dma_bd13_3;
   regNameToValue["mem_dma_bd13_4"] = aie2ps::mem_dma_bd13_4;
   regNameToValue["mem_dma_bd13_5"] = aie2ps::mem_dma_bd13_5;
   regNameToValue["mem_dma_bd13_6"] = aie2ps::mem_dma_bd13_6;
   regNameToValue["mem_dma_bd13_7"] = aie2ps::mem_dma_bd13_7;
   regNameToValue["mem_dma_bd14_0"] = aie2ps::mem_dma_bd14_0;
   regNameToValue["mem_dma_bd14_1"] = aie2ps::mem_dma_bd14_1;
   regNameToValue["mem_dma_bd14_2"] = aie2ps::mem_dma_bd14_2;
   regNameToValue["mem_dma_bd14_3"] = aie2ps::mem_dma_bd14_3;
   regNameToValue["mem_dma_bd14_4"] = aie2ps::mem_dma_bd14_4;
   regNameToValue["mem_dma_bd14_5"] = aie2ps::mem_dma_bd14_5;
   regNameToValue["mem_dma_bd14_6"] = aie2ps::mem_dma_bd14_6;
   regNameToValue["mem_dma_bd14_7"] = aie2ps::mem_dma_bd14_7;
   regNameToValue["mem_dma_bd15_0"] = aie2ps::mem_dma_bd15_0;
   regNameToValue["mem_dma_bd15_1"] = aie2ps::mem_dma_bd15_1;
   regNameToValue["mem_dma_bd15_2"] = aie2ps::mem_dma_bd15_2;
   regNameToValue["mem_dma_bd15_3"] = aie2ps::mem_dma_bd15_3;
   regNameToValue["mem_dma_bd15_4"] = aie2ps::mem_dma_bd15_4;
   regNameToValue["mem_dma_bd15_5"] = aie2ps::mem_dma_bd15_5;
   regNameToValue["mem_dma_bd15_6"] = aie2ps::mem_dma_bd15_6;
   regNameToValue["mem_dma_bd15_7"] = aie2ps::mem_dma_bd15_7;
   regNameToValue["mem_dma_bd16_0"] = aie2ps::mem_dma_bd16_0;
   regNameToValue["mem_dma_bd16_1"] = aie2ps::mem_dma_bd16_1;
   regNameToValue["mem_dma_bd16_2"] = aie2ps::mem_dma_bd16_2;
   regNameToValue["mem_dma_bd16_3"] = aie2ps::mem_dma_bd16_3;
   regNameToValue["mem_dma_bd16_4"] = aie2ps::mem_dma_bd16_4;
   regNameToValue["mem_dma_bd16_5"] = aie2ps::mem_dma_bd16_5;
   regNameToValue["mem_dma_bd16_6"] = aie2ps::mem_dma_bd16_6;
   regNameToValue["mem_dma_bd16_7"] = aie2ps::mem_dma_bd16_7;
   regNameToValue["mem_dma_bd17_0"] = aie2ps::mem_dma_bd17_0;
   regNameToValue["mem_dma_bd17_1"] = aie2ps::mem_dma_bd17_1;
   regNameToValue["mem_dma_bd17_2"] = aie2ps::mem_dma_bd17_2;
   regNameToValue["mem_dma_bd17_3"] = aie2ps::mem_dma_bd17_3;
   regNameToValue["mem_dma_bd17_4"] = aie2ps::mem_dma_bd17_4;
   regNameToValue["mem_dma_bd17_5"] = aie2ps::mem_dma_bd17_5;
   regNameToValue["mem_dma_bd17_6"] = aie2ps::mem_dma_bd17_6;
   regNameToValue["mem_dma_bd17_7"] = aie2ps::mem_dma_bd17_7;
   regNameToValue["mem_dma_bd18_0"] = aie2ps::mem_dma_bd18_0;
   regNameToValue["mem_dma_bd18_1"] = aie2ps::mem_dma_bd18_1;
   regNameToValue["mem_dma_bd18_2"] = aie2ps::mem_dma_bd18_2;
   regNameToValue["mem_dma_bd18_3"] = aie2ps::mem_dma_bd18_3;
   regNameToValue["mem_dma_bd18_4"] = aie2ps::mem_dma_bd18_4;
   regNameToValue["mem_dma_bd18_5"] = aie2ps::mem_dma_bd18_5;
   regNameToValue["mem_dma_bd18_6"] = aie2ps::mem_dma_bd18_6;
   regNameToValue["mem_dma_bd18_7"] = aie2ps::mem_dma_bd18_7;
   regNameToValue["mem_dma_bd19_0"] = aie2ps::mem_dma_bd19_0;
   regNameToValue["mem_dma_bd19_1"] = aie2ps::mem_dma_bd19_1;
   regNameToValue["mem_dma_bd19_2"] = aie2ps::mem_dma_bd19_2;
   regNameToValue["mem_dma_bd19_3"] = aie2ps::mem_dma_bd19_3;
   regNameToValue["mem_dma_bd19_4"] = aie2ps::mem_dma_bd19_4;
   regNameToValue["mem_dma_bd19_5"] = aie2ps::mem_dma_bd19_5;
   regNameToValue["mem_dma_bd19_6"] = aie2ps::mem_dma_bd19_6;
   regNameToValue["mem_dma_bd19_7"] = aie2ps::mem_dma_bd19_7;
   regNameToValue["mem_dma_bd1_0"] = aie2ps::mem_dma_bd1_0;
   regNameToValue["mem_dma_bd1_1"] = aie2ps::mem_dma_bd1_1;
   regNameToValue["mem_dma_bd1_2"] = aie2ps::mem_dma_bd1_2;
   regNameToValue["mem_dma_bd1_3"] = aie2ps::mem_dma_bd1_3;
   regNameToValue["mem_dma_bd1_4"] = aie2ps::mem_dma_bd1_4;
   regNameToValue["mem_dma_bd1_5"] = aie2ps::mem_dma_bd1_5;
   regNameToValue["mem_dma_bd1_6"] = aie2ps::mem_dma_bd1_6;
   regNameToValue["mem_dma_bd1_7"] = aie2ps::mem_dma_bd1_7;
   regNameToValue["mem_dma_bd20_0"] = aie2ps::mem_dma_bd20_0;
   regNameToValue["mem_dma_bd20_1"] = aie2ps::mem_dma_bd20_1;
   regNameToValue["mem_dma_bd20_2"] = aie2ps::mem_dma_bd20_2;
   regNameToValue["mem_dma_bd20_3"] = aie2ps::mem_dma_bd20_3;
   regNameToValue["mem_dma_bd20_4"] = aie2ps::mem_dma_bd20_4;
   regNameToValue["mem_dma_bd20_5"] = aie2ps::mem_dma_bd20_5;
   regNameToValue["mem_dma_bd20_6"] = aie2ps::mem_dma_bd20_6;
   regNameToValue["mem_dma_bd20_7"] = aie2ps::mem_dma_bd20_7;
   regNameToValue["mem_dma_bd21_0"] = aie2ps::mem_dma_bd21_0;
   regNameToValue["mem_dma_bd21_1"] = aie2ps::mem_dma_bd21_1;
   regNameToValue["mem_dma_bd21_2"] = aie2ps::mem_dma_bd21_2;
   regNameToValue["mem_dma_bd21_3"] = aie2ps::mem_dma_bd21_3;
   regNameToValue["mem_dma_bd21_4"] = aie2ps::mem_dma_bd21_4;
   regNameToValue["mem_dma_bd21_5"] = aie2ps::mem_dma_bd21_5;
   regNameToValue["mem_dma_bd21_6"] = aie2ps::mem_dma_bd21_6;
   regNameToValue["mem_dma_bd21_7"] = aie2ps::mem_dma_bd21_7;
   regNameToValue["mem_dma_bd22_0"] = aie2ps::mem_dma_bd22_0;
   regNameToValue["mem_dma_bd22_1"] = aie2ps::mem_dma_bd22_1;
   regNameToValue["mem_dma_bd22_2"] = aie2ps::mem_dma_bd22_2;
   regNameToValue["mem_dma_bd22_3"] = aie2ps::mem_dma_bd22_3;
   regNameToValue["mem_dma_bd22_4"] = aie2ps::mem_dma_bd22_4;
   regNameToValue["mem_dma_bd22_5"] = aie2ps::mem_dma_bd22_5;
   regNameToValue["mem_dma_bd22_6"] = aie2ps::mem_dma_bd22_6;
   regNameToValue["mem_dma_bd22_7"] = aie2ps::mem_dma_bd22_7;
   regNameToValue["mem_dma_bd23_0"] = aie2ps::mem_dma_bd23_0;
   regNameToValue["mem_dma_bd23_1"] = aie2ps::mem_dma_bd23_1;
   regNameToValue["mem_dma_bd23_2"] = aie2ps::mem_dma_bd23_2;
   regNameToValue["mem_dma_bd23_3"] = aie2ps::mem_dma_bd23_3;
   regNameToValue["mem_dma_bd23_4"] = aie2ps::mem_dma_bd23_4;
   regNameToValue["mem_dma_bd23_5"] = aie2ps::mem_dma_bd23_5;
   regNameToValue["mem_dma_bd23_6"] = aie2ps::mem_dma_bd23_6;
   regNameToValue["mem_dma_bd23_7"] = aie2ps::mem_dma_bd23_7;
   regNameToValue["mem_dma_bd24_0"] = aie2ps::mem_dma_bd24_0;
   regNameToValue["mem_dma_bd24_1"] = aie2ps::mem_dma_bd24_1;
   regNameToValue["mem_dma_bd24_2"] = aie2ps::mem_dma_bd24_2;
   regNameToValue["mem_dma_bd24_3"] = aie2ps::mem_dma_bd24_3;
   regNameToValue["mem_dma_bd24_4"] = aie2ps::mem_dma_bd24_4;
   regNameToValue["mem_dma_bd24_5"] = aie2ps::mem_dma_bd24_5;
   regNameToValue["mem_dma_bd24_6"] = aie2ps::mem_dma_bd24_6;
   regNameToValue["mem_dma_bd24_7"] = aie2ps::mem_dma_bd24_7;
   regNameToValue["mem_dma_bd25_0"] = aie2ps::mem_dma_bd25_0;
   regNameToValue["mem_dma_bd25_1"] = aie2ps::mem_dma_bd25_1;
   regNameToValue["mem_dma_bd25_2"] = aie2ps::mem_dma_bd25_2;
   regNameToValue["mem_dma_bd25_3"] = aie2ps::mem_dma_bd25_3;
   regNameToValue["mem_dma_bd25_4"] = aie2ps::mem_dma_bd25_4;
   regNameToValue["mem_dma_bd25_5"] = aie2ps::mem_dma_bd25_5;
   regNameToValue["mem_dma_bd25_6"] = aie2ps::mem_dma_bd25_6;
   regNameToValue["mem_dma_bd25_7"] = aie2ps::mem_dma_bd25_7;
   regNameToValue["mem_dma_bd26_0"] = aie2ps::mem_dma_bd26_0;
   regNameToValue["mem_dma_bd26_1"] = aie2ps::mem_dma_bd26_1;
   regNameToValue["mem_dma_bd26_2"] = aie2ps::mem_dma_bd26_2;
   regNameToValue["mem_dma_bd26_3"] = aie2ps::mem_dma_bd26_3;
   regNameToValue["mem_dma_bd26_4"] = aie2ps::mem_dma_bd26_4;
   regNameToValue["mem_dma_bd26_5"] = aie2ps::mem_dma_bd26_5;
   regNameToValue["mem_dma_bd26_6"] = aie2ps::mem_dma_bd26_6;
   regNameToValue["mem_dma_bd26_7"] = aie2ps::mem_dma_bd26_7;
   regNameToValue["mem_dma_bd27_0"] = aie2ps::mem_dma_bd27_0;
   regNameToValue["mem_dma_bd27_1"] = aie2ps::mem_dma_bd27_1;
   regNameToValue["mem_dma_bd27_2"] = aie2ps::mem_dma_bd27_2;
   regNameToValue["mem_dma_bd27_3"] = aie2ps::mem_dma_bd27_3;
   regNameToValue["mem_dma_bd27_4"] = aie2ps::mem_dma_bd27_4;
   regNameToValue["mem_dma_bd27_5"] = aie2ps::mem_dma_bd27_5;
   regNameToValue["mem_dma_bd27_6"] = aie2ps::mem_dma_bd27_6;
   regNameToValue["mem_dma_bd27_7"] = aie2ps::mem_dma_bd27_7;
   regNameToValue["mem_dma_bd28_0"] = aie2ps::mem_dma_bd28_0;
   regNameToValue["mem_dma_bd28_1"] = aie2ps::mem_dma_bd28_1;
   regNameToValue["mem_dma_bd28_2"] = aie2ps::mem_dma_bd28_2;
   regNameToValue["mem_dma_bd28_3"] = aie2ps::mem_dma_bd28_3;
   regNameToValue["mem_dma_bd28_4"] = aie2ps::mem_dma_bd28_4;
   regNameToValue["mem_dma_bd28_5"] = aie2ps::mem_dma_bd28_5;
   regNameToValue["mem_dma_bd28_6"] = aie2ps::mem_dma_bd28_6;
   regNameToValue["mem_dma_bd28_7"] = aie2ps::mem_dma_bd28_7;
   regNameToValue["mem_dma_bd29_0"] = aie2ps::mem_dma_bd29_0;
   regNameToValue["mem_dma_bd29_1"] = aie2ps::mem_dma_bd29_1;
   regNameToValue["mem_dma_bd29_2"] = aie2ps::mem_dma_bd29_2;
   regNameToValue["mem_dma_bd29_3"] = aie2ps::mem_dma_bd29_3;
   regNameToValue["mem_dma_bd29_4"] = aie2ps::mem_dma_bd29_4;
   regNameToValue["mem_dma_bd29_5"] = aie2ps::mem_dma_bd29_5;
   regNameToValue["mem_dma_bd29_6"] = aie2ps::mem_dma_bd29_6;
   regNameToValue["mem_dma_bd29_7"] = aie2ps::mem_dma_bd29_7;
   regNameToValue["mem_dma_bd2_0"] = aie2ps::mem_dma_bd2_0;
   regNameToValue["mem_dma_bd2_1"] = aie2ps::mem_dma_bd2_1;
   regNameToValue["mem_dma_bd2_2"] = aie2ps::mem_dma_bd2_2;
   regNameToValue["mem_dma_bd2_3"] = aie2ps::mem_dma_bd2_3;
   regNameToValue["mem_dma_bd2_4"] = aie2ps::mem_dma_bd2_4;
   regNameToValue["mem_dma_bd2_5"] = aie2ps::mem_dma_bd2_5;
   regNameToValue["mem_dma_bd2_6"] = aie2ps::mem_dma_bd2_6;
   regNameToValue["mem_dma_bd2_7"] = aie2ps::mem_dma_bd2_7;
   regNameToValue["mem_dma_bd30_0"] = aie2ps::mem_dma_bd30_0;
   regNameToValue["mem_dma_bd30_1"] = aie2ps::mem_dma_bd30_1;
   regNameToValue["mem_dma_bd30_2"] = aie2ps::mem_dma_bd30_2;
   regNameToValue["mem_dma_bd30_3"] = aie2ps::mem_dma_bd30_3;
   regNameToValue["mem_dma_bd30_4"] = aie2ps::mem_dma_bd30_4;
   regNameToValue["mem_dma_bd30_5"] = aie2ps::mem_dma_bd30_5;
   regNameToValue["mem_dma_bd30_6"] = aie2ps::mem_dma_bd30_6;
   regNameToValue["mem_dma_bd30_7"] = aie2ps::mem_dma_bd30_7;
   regNameToValue["mem_dma_bd31_0"] = aie2ps::mem_dma_bd31_0;
   regNameToValue["mem_dma_bd31_1"] = aie2ps::mem_dma_bd31_1;
   regNameToValue["mem_dma_bd31_2"] = aie2ps::mem_dma_bd31_2;
   regNameToValue["mem_dma_bd31_3"] = aie2ps::mem_dma_bd31_3;
   regNameToValue["mem_dma_bd31_4"] = aie2ps::mem_dma_bd31_4;
   regNameToValue["mem_dma_bd31_5"] = aie2ps::mem_dma_bd31_5;
   regNameToValue["mem_dma_bd31_6"] = aie2ps::mem_dma_bd31_6;
   regNameToValue["mem_dma_bd31_7"] = aie2ps::mem_dma_bd31_7;
   regNameToValue["mem_dma_bd32_0"] = aie2ps::mem_dma_bd32_0;
   regNameToValue["mem_dma_bd32_1"] = aie2ps::mem_dma_bd32_1;
   regNameToValue["mem_dma_bd32_2"] = aie2ps::mem_dma_bd32_2;
   regNameToValue["mem_dma_bd32_3"] = aie2ps::mem_dma_bd32_3;
   regNameToValue["mem_dma_bd32_4"] = aie2ps::mem_dma_bd32_4;
   regNameToValue["mem_dma_bd32_5"] = aie2ps::mem_dma_bd32_5;
   regNameToValue["mem_dma_bd32_6"] = aie2ps::mem_dma_bd32_6;
   regNameToValue["mem_dma_bd32_7"] = aie2ps::mem_dma_bd32_7;
   regNameToValue["mem_dma_bd33_0"] = aie2ps::mem_dma_bd33_0;
   regNameToValue["mem_dma_bd33_1"] = aie2ps::mem_dma_bd33_1;
   regNameToValue["mem_dma_bd33_2"] = aie2ps::mem_dma_bd33_2;
   regNameToValue["mem_dma_bd33_3"] = aie2ps::mem_dma_bd33_3;
   regNameToValue["mem_dma_bd33_4"] = aie2ps::mem_dma_bd33_4;
   regNameToValue["mem_dma_bd33_5"] = aie2ps::mem_dma_bd33_5;
   regNameToValue["mem_dma_bd33_6"] = aie2ps::mem_dma_bd33_6;
   regNameToValue["mem_dma_bd33_7"] = aie2ps::mem_dma_bd33_7;
   regNameToValue["mem_dma_bd34_0"] = aie2ps::mem_dma_bd34_0;
   regNameToValue["mem_dma_bd34_1"] = aie2ps::mem_dma_bd34_1;
   regNameToValue["mem_dma_bd34_2"] = aie2ps::mem_dma_bd34_2;
   regNameToValue["mem_dma_bd34_3"] = aie2ps::mem_dma_bd34_3;
   regNameToValue["mem_dma_bd34_4"] = aie2ps::mem_dma_bd34_4;
   regNameToValue["mem_dma_bd34_5"] = aie2ps::mem_dma_bd34_5;
   regNameToValue["mem_dma_bd34_6"] = aie2ps::mem_dma_bd34_6;
   regNameToValue["mem_dma_bd34_7"] = aie2ps::mem_dma_bd34_7;
   regNameToValue["mem_dma_bd35_0"] = aie2ps::mem_dma_bd35_0;
   regNameToValue["mem_dma_bd35_1"] = aie2ps::mem_dma_bd35_1;
   regNameToValue["mem_dma_bd35_2"] = aie2ps::mem_dma_bd35_2;
   regNameToValue["mem_dma_bd35_3"] = aie2ps::mem_dma_bd35_3;
   regNameToValue["mem_dma_bd35_4"] = aie2ps::mem_dma_bd35_4;
   regNameToValue["mem_dma_bd35_5"] = aie2ps::mem_dma_bd35_5;
   regNameToValue["mem_dma_bd35_6"] = aie2ps::mem_dma_bd35_6;
   regNameToValue["mem_dma_bd35_7"] = aie2ps::mem_dma_bd35_7;
   regNameToValue["mem_dma_bd36_0"] = aie2ps::mem_dma_bd36_0;
   regNameToValue["mem_dma_bd36_1"] = aie2ps::mem_dma_bd36_1;
   regNameToValue["mem_dma_bd36_2"] = aie2ps::mem_dma_bd36_2;
   regNameToValue["mem_dma_bd36_3"] = aie2ps::mem_dma_bd36_3;
   regNameToValue["mem_dma_bd36_4"] = aie2ps::mem_dma_bd36_4;
   regNameToValue["mem_dma_bd36_5"] = aie2ps::mem_dma_bd36_5;
   regNameToValue["mem_dma_bd36_6"] = aie2ps::mem_dma_bd36_6;
   regNameToValue["mem_dma_bd36_7"] = aie2ps::mem_dma_bd36_7;
   regNameToValue["mem_dma_bd37_0"] = aie2ps::mem_dma_bd37_0;
   regNameToValue["mem_dma_bd37_1"] = aie2ps::mem_dma_bd37_1;
   regNameToValue["mem_dma_bd37_2"] = aie2ps::mem_dma_bd37_2;
   regNameToValue["mem_dma_bd37_3"] = aie2ps::mem_dma_bd37_3;
   regNameToValue["mem_dma_bd37_4"] = aie2ps::mem_dma_bd37_4;
   regNameToValue["mem_dma_bd37_5"] = aie2ps::mem_dma_bd37_5;
   regNameToValue["mem_dma_bd37_6"] = aie2ps::mem_dma_bd37_6;
   regNameToValue["mem_dma_bd37_7"] = aie2ps::mem_dma_bd37_7;
   regNameToValue["mem_dma_bd38_0"] = aie2ps::mem_dma_bd38_0;
   regNameToValue["mem_dma_bd38_1"] = aie2ps::mem_dma_bd38_1;
   regNameToValue["mem_dma_bd38_2"] = aie2ps::mem_dma_bd38_2;
   regNameToValue["mem_dma_bd38_3"] = aie2ps::mem_dma_bd38_3;
   regNameToValue["mem_dma_bd38_4"] = aie2ps::mem_dma_bd38_4;
   regNameToValue["mem_dma_bd38_5"] = aie2ps::mem_dma_bd38_5;
   regNameToValue["mem_dma_bd38_6"] = aie2ps::mem_dma_bd38_6;
   regNameToValue["mem_dma_bd38_7"] = aie2ps::mem_dma_bd38_7;
   regNameToValue["mem_dma_bd39_0"] = aie2ps::mem_dma_bd39_0;
   regNameToValue["mem_dma_bd39_1"] = aie2ps::mem_dma_bd39_1;
   regNameToValue["mem_dma_bd39_2"] = aie2ps::mem_dma_bd39_2;
   regNameToValue["mem_dma_bd39_3"] = aie2ps::mem_dma_bd39_3;
   regNameToValue["mem_dma_bd39_4"] = aie2ps::mem_dma_bd39_4;
   regNameToValue["mem_dma_bd39_5"] = aie2ps::mem_dma_bd39_5;
   regNameToValue["mem_dma_bd39_6"] = aie2ps::mem_dma_bd39_6;
   regNameToValue["mem_dma_bd39_7"] = aie2ps::mem_dma_bd39_7;
   regNameToValue["mem_dma_bd3_0"] = aie2ps::mem_dma_bd3_0;
   regNameToValue["mem_dma_bd3_1"] = aie2ps::mem_dma_bd3_1;
   regNameToValue["mem_dma_bd3_2"] = aie2ps::mem_dma_bd3_2;
   regNameToValue["mem_dma_bd3_3"] = aie2ps::mem_dma_bd3_3;
   regNameToValue["mem_dma_bd3_4"] = aie2ps::mem_dma_bd3_4;
   regNameToValue["mem_dma_bd3_5"] = aie2ps::mem_dma_bd3_5;
   regNameToValue["mem_dma_bd3_6"] = aie2ps::mem_dma_bd3_6;
   regNameToValue["mem_dma_bd3_7"] = aie2ps::mem_dma_bd3_7;
   regNameToValue["mem_dma_bd40_0"] = aie2ps::mem_dma_bd40_0;
   regNameToValue["mem_dma_bd40_1"] = aie2ps::mem_dma_bd40_1;
   regNameToValue["mem_dma_bd40_2"] = aie2ps::mem_dma_bd40_2;
   regNameToValue["mem_dma_bd40_3"] = aie2ps::mem_dma_bd40_3;
   regNameToValue["mem_dma_bd40_4"] = aie2ps::mem_dma_bd40_4;
   regNameToValue["mem_dma_bd40_5"] = aie2ps::mem_dma_bd40_5;
   regNameToValue["mem_dma_bd40_6"] = aie2ps::mem_dma_bd40_6;
   regNameToValue["mem_dma_bd40_7"] = aie2ps::mem_dma_bd40_7;
   regNameToValue["mem_dma_bd41_0"] = aie2ps::mem_dma_bd41_0;
   regNameToValue["mem_dma_bd41_1"] = aie2ps::mem_dma_bd41_1;
   regNameToValue["mem_dma_bd41_2"] = aie2ps::mem_dma_bd41_2;
   regNameToValue["mem_dma_bd41_3"] = aie2ps::mem_dma_bd41_3;
   regNameToValue["mem_dma_bd41_4"] = aie2ps::mem_dma_bd41_4;
   regNameToValue["mem_dma_bd41_5"] = aie2ps::mem_dma_bd41_5;
   regNameToValue["mem_dma_bd41_6"] = aie2ps::mem_dma_bd41_6;
   regNameToValue["mem_dma_bd41_7"] = aie2ps::mem_dma_bd41_7;
   regNameToValue["mem_dma_bd42_0"] = aie2ps::mem_dma_bd42_0;
   regNameToValue["mem_dma_bd42_1"] = aie2ps::mem_dma_bd42_1;
   regNameToValue["mem_dma_bd42_2"] = aie2ps::mem_dma_bd42_2;
   regNameToValue["mem_dma_bd42_3"] = aie2ps::mem_dma_bd42_3;
   regNameToValue["mem_dma_bd42_4"] = aie2ps::mem_dma_bd42_4;
   regNameToValue["mem_dma_bd42_5"] = aie2ps::mem_dma_bd42_5;
   regNameToValue["mem_dma_bd42_6"] = aie2ps::mem_dma_bd42_6;
   regNameToValue["mem_dma_bd42_7"] = aie2ps::mem_dma_bd42_7;
   regNameToValue["mem_dma_bd43_0"] = aie2ps::mem_dma_bd43_0;
   regNameToValue["mem_dma_bd43_1"] = aie2ps::mem_dma_bd43_1;
   regNameToValue["mem_dma_bd43_2"] = aie2ps::mem_dma_bd43_2;
   regNameToValue["mem_dma_bd43_3"] = aie2ps::mem_dma_bd43_3;
   regNameToValue["mem_dma_bd43_4"] = aie2ps::mem_dma_bd43_4;
   regNameToValue["mem_dma_bd43_5"] = aie2ps::mem_dma_bd43_5;
   regNameToValue["mem_dma_bd43_6"] = aie2ps::mem_dma_bd43_6;
   regNameToValue["mem_dma_bd43_7"] = aie2ps::mem_dma_bd43_7;
   regNameToValue["mem_dma_bd44_0"] = aie2ps::mem_dma_bd44_0;
   regNameToValue["mem_dma_bd44_1"] = aie2ps::mem_dma_bd44_1;
   regNameToValue["mem_dma_bd44_2"] = aie2ps::mem_dma_bd44_2;
   regNameToValue["mem_dma_bd44_3"] = aie2ps::mem_dma_bd44_3;
   regNameToValue["mem_dma_bd44_4"] = aie2ps::mem_dma_bd44_4;
   regNameToValue["mem_dma_bd44_5"] = aie2ps::mem_dma_bd44_5;
   regNameToValue["mem_dma_bd44_6"] = aie2ps::mem_dma_bd44_6;
   regNameToValue["mem_dma_bd44_7"] = aie2ps::mem_dma_bd44_7;
   regNameToValue["mem_dma_bd45_0"] = aie2ps::mem_dma_bd45_0;
   regNameToValue["mem_dma_bd45_1"] = aie2ps::mem_dma_bd45_1;
   regNameToValue["mem_dma_bd45_2"] = aie2ps::mem_dma_bd45_2;
   regNameToValue["mem_dma_bd45_3"] = aie2ps::mem_dma_bd45_3;
   regNameToValue["mem_dma_bd45_4"] = aie2ps::mem_dma_bd45_4;
   regNameToValue["mem_dma_bd45_5"] = aie2ps::mem_dma_bd45_5;
   regNameToValue["mem_dma_bd45_6"] = aie2ps::mem_dma_bd45_6;
   regNameToValue["mem_dma_bd45_7"] = aie2ps::mem_dma_bd45_7;
   regNameToValue["mem_dma_bd46_0"] = aie2ps::mem_dma_bd46_0;
   regNameToValue["mem_dma_bd46_1"] = aie2ps::mem_dma_bd46_1;
   regNameToValue["mem_dma_bd46_2"] = aie2ps::mem_dma_bd46_2;
   regNameToValue["mem_dma_bd46_3"] = aie2ps::mem_dma_bd46_3;
   regNameToValue["mem_dma_bd46_4"] = aie2ps::mem_dma_bd46_4;
   regNameToValue["mem_dma_bd46_5"] = aie2ps::mem_dma_bd46_5;
   regNameToValue["mem_dma_bd46_6"] = aie2ps::mem_dma_bd46_6;
   regNameToValue["mem_dma_bd46_7"] = aie2ps::mem_dma_bd46_7;
   regNameToValue["mem_dma_bd47_0"] = aie2ps::mem_dma_bd47_0;
   regNameToValue["mem_dma_bd47_1"] = aie2ps::mem_dma_bd47_1;
   regNameToValue["mem_dma_bd47_2"] = aie2ps::mem_dma_bd47_2;
   regNameToValue["mem_dma_bd47_3"] = aie2ps::mem_dma_bd47_3;
   regNameToValue["mem_dma_bd47_4"] = aie2ps::mem_dma_bd47_4;
   regNameToValue["mem_dma_bd47_5"] = aie2ps::mem_dma_bd47_5;
   regNameToValue["mem_dma_bd47_6"] = aie2ps::mem_dma_bd47_6;
   regNameToValue["mem_dma_bd47_7"] = aie2ps::mem_dma_bd47_7;
   regNameToValue["mem_dma_bd4_0"] = aie2ps::mem_dma_bd4_0;
   regNameToValue["mem_dma_bd4_1"] = aie2ps::mem_dma_bd4_1;
   regNameToValue["mem_dma_bd4_2"] = aie2ps::mem_dma_bd4_2;
   regNameToValue["mem_dma_bd4_3"] = aie2ps::mem_dma_bd4_3;
   regNameToValue["mem_dma_bd4_4"] = aie2ps::mem_dma_bd4_4;
   regNameToValue["mem_dma_bd4_5"] = aie2ps::mem_dma_bd4_5;
   regNameToValue["mem_dma_bd4_6"] = aie2ps::mem_dma_bd4_6;
   regNameToValue["mem_dma_bd4_7"] = aie2ps::mem_dma_bd4_7;
   regNameToValue["mem_dma_bd5_0"] = aie2ps::mem_dma_bd5_0;
   regNameToValue["mem_dma_bd5_1"] = aie2ps::mem_dma_bd5_1;
   regNameToValue["mem_dma_bd5_2"] = aie2ps::mem_dma_bd5_2;
   regNameToValue["mem_dma_bd5_3"] = aie2ps::mem_dma_bd5_3;
   regNameToValue["mem_dma_bd5_4"] = aie2ps::mem_dma_bd5_4;
   regNameToValue["mem_dma_bd5_5"] = aie2ps::mem_dma_bd5_5;
   regNameToValue["mem_dma_bd5_6"] = aie2ps::mem_dma_bd5_6;
   regNameToValue["mem_dma_bd5_7"] = aie2ps::mem_dma_bd5_7;
   regNameToValue["mem_dma_bd6_0"] = aie2ps::mem_dma_bd6_0;
   regNameToValue["mem_dma_bd6_1"] = aie2ps::mem_dma_bd6_1;
   regNameToValue["mem_dma_bd6_2"] = aie2ps::mem_dma_bd6_2;
   regNameToValue["mem_dma_bd6_3"] = aie2ps::mem_dma_bd6_3;
   regNameToValue["mem_dma_bd6_4"] = aie2ps::mem_dma_bd6_4;
   regNameToValue["mem_dma_bd6_5"] = aie2ps::mem_dma_bd6_5;
   regNameToValue["mem_dma_bd6_6"] = aie2ps::mem_dma_bd6_6;
   regNameToValue["mem_dma_bd6_7"] = aie2ps::mem_dma_bd6_7;
   regNameToValue["mem_dma_bd7_0"] = aie2ps::mem_dma_bd7_0;
   regNameToValue["mem_dma_bd7_1"] = aie2ps::mem_dma_bd7_1;
   regNameToValue["mem_dma_bd7_2"] = aie2ps::mem_dma_bd7_2;
   regNameToValue["mem_dma_bd7_3"] = aie2ps::mem_dma_bd7_3;
   regNameToValue["mem_dma_bd7_4"] = aie2ps::mem_dma_bd7_4;
   regNameToValue["mem_dma_bd7_5"] = aie2ps::mem_dma_bd7_5;
   regNameToValue["mem_dma_bd7_6"] = aie2ps::mem_dma_bd7_6;
   regNameToValue["mem_dma_bd7_7"] = aie2ps::mem_dma_bd7_7;
   regNameToValue["mem_dma_bd8_0"] = aie2ps::mem_dma_bd8_0;
   regNameToValue["mem_dma_bd8_1"] = aie2ps::mem_dma_bd8_1;
   regNameToValue["mem_dma_bd8_2"] = aie2ps::mem_dma_bd8_2;
   regNameToValue["mem_dma_bd8_3"] = aie2ps::mem_dma_bd8_3;
   regNameToValue["mem_dma_bd8_4"] = aie2ps::mem_dma_bd8_4;
   regNameToValue["mem_dma_bd8_5"] = aie2ps::mem_dma_bd8_5;
   regNameToValue["mem_dma_bd8_6"] = aie2ps::mem_dma_bd8_6;
   regNameToValue["mem_dma_bd8_7"] = aie2ps::mem_dma_bd8_7;
   regNameToValue["mem_dma_bd9_0"] = aie2ps::mem_dma_bd9_0;
   regNameToValue["mem_dma_bd9_1"] = aie2ps::mem_dma_bd9_1;
   regNameToValue["mem_dma_bd9_2"] = aie2ps::mem_dma_bd9_2;
   regNameToValue["mem_dma_bd9_3"] = aie2ps::mem_dma_bd9_3;
   regNameToValue["mem_dma_bd9_4"] = aie2ps::mem_dma_bd9_4;
   regNameToValue["mem_dma_bd9_5"] = aie2ps::mem_dma_bd9_5;
   regNameToValue["mem_dma_bd9_6"] = aie2ps::mem_dma_bd9_6;
   regNameToValue["mem_dma_bd9_7"] = aie2ps::mem_dma_bd9_7;
   regNameToValue["mem_dma_event_channel_selection"] = aie2ps::mem_dma_event_channel_selection;
   regNameToValue["mem_dma_mm2s_0_constant_pad_value"] = aie2ps::mem_dma_mm2s_0_constant_pad_value;
   regNameToValue["mem_dma_mm2s_0_ctrl"] = aie2ps::mem_dma_mm2s_0_ctrl;
   regNameToValue["mem_dma_mm2s_0_start_queue"] = aie2ps::mem_dma_mm2s_0_start_queue;
   regNameToValue["mem_dma_mm2s_1_constant_pad_value"] = aie2ps::mem_dma_mm2s_1_constant_pad_value;
   regNameToValue["mem_dma_mm2s_1_ctrl"] = aie2ps::mem_dma_mm2s_1_ctrl;
   regNameToValue["mem_dma_mm2s_1_start_queue"] = aie2ps::mem_dma_mm2s_1_start_queue;
   regNameToValue["mem_dma_mm2s_2_constant_pad_value"] = aie2ps::mem_dma_mm2s_2_constant_pad_value;
   regNameToValue["mem_dma_mm2s_2_ctrl"] = aie2ps::mem_dma_mm2s_2_ctrl;
   regNameToValue["mem_dma_mm2s_2_start_queue"] = aie2ps::mem_dma_mm2s_2_start_queue;
   regNameToValue["mem_dma_mm2s_3_constant_pad_value"] = aie2ps::mem_dma_mm2s_3_constant_pad_value;
   regNameToValue["mem_dma_mm2s_3_ctrl"] = aie2ps::mem_dma_mm2s_3_ctrl;
   regNameToValue["mem_dma_mm2s_3_start_queue"] = aie2ps::mem_dma_mm2s_3_start_queue;
   regNameToValue["mem_dma_mm2s_4_constant_pad_value"] = aie2ps::mem_dma_mm2s_4_constant_pad_value;
   regNameToValue["mem_dma_mm2s_4_ctrl"] = aie2ps::mem_dma_mm2s_4_ctrl;
   regNameToValue["mem_dma_mm2s_4_start_queue"] = aie2ps::mem_dma_mm2s_4_start_queue;
   regNameToValue["mem_dma_mm2s_5_constant_pad_value"] = aie2ps::mem_dma_mm2s_5_constant_pad_value;
   regNameToValue["mem_dma_mm2s_5_ctrl"] = aie2ps::mem_dma_mm2s_5_ctrl;
   regNameToValue["mem_dma_mm2s_5_start_queue"] = aie2ps::mem_dma_mm2s_5_start_queue;
   regNameToValue["mem_dma_mm2s_status_0"] = aie2ps::mem_dma_mm2s_status_0;
   regNameToValue["mem_dma_mm2s_status_1"] = aie2ps::mem_dma_mm2s_status_1;
   regNameToValue["mem_dma_mm2s_status_2"] = aie2ps::mem_dma_mm2s_status_2;
   regNameToValue["mem_dma_mm2s_status_3"] = aie2ps::mem_dma_mm2s_status_3;
   regNameToValue["mem_dma_mm2s_status_4"] = aie2ps::mem_dma_mm2s_status_4;
   regNameToValue["mem_dma_mm2s_status_5"] = aie2ps::mem_dma_mm2s_status_5;
   regNameToValue["mem_dma_s2mm_0_ctrl"] = aie2ps::mem_dma_s2mm_0_ctrl;
   regNameToValue["mem_dma_s2mm_0_start_queue"] = aie2ps::mem_dma_s2mm_0_start_queue;
   regNameToValue["mem_dma_s2mm_1_ctrl"] = aie2ps::mem_dma_s2mm_1_ctrl;
   regNameToValue["mem_dma_s2mm_1_start_queue"] = aie2ps::mem_dma_s2mm_1_start_queue;
   regNameToValue["mem_dma_s2mm_2_ctrl"] = aie2ps::mem_dma_s2mm_2_ctrl;
   regNameToValue["mem_dma_s2mm_2_start_queue"] = aie2ps::mem_dma_s2mm_2_start_queue;
   regNameToValue["mem_dma_s2mm_3_ctrl"] = aie2ps::mem_dma_s2mm_3_ctrl;
   regNameToValue["mem_dma_s2mm_3_start_queue"] = aie2ps::mem_dma_s2mm_3_start_queue;
   regNameToValue["mem_dma_s2mm_4_ctrl"] = aie2ps::mem_dma_s2mm_4_ctrl;
   regNameToValue["mem_dma_s2mm_4_start_queue"] = aie2ps::mem_dma_s2mm_4_start_queue;
   regNameToValue["mem_dma_s2mm_5_ctrl"] = aie2ps::mem_dma_s2mm_5_ctrl;
   regNameToValue["mem_dma_s2mm_5_start_queue"] = aie2ps::mem_dma_s2mm_5_start_queue;
   regNameToValue["mem_dma_s2mm_current_write_count_0"] = aie2ps::mem_dma_s2mm_current_write_count_0;
   regNameToValue["mem_dma_s2mm_current_write_count_1"] = aie2ps::mem_dma_s2mm_current_write_count_1;
   regNameToValue["mem_dma_s2mm_current_write_count_2"] = aie2ps::mem_dma_s2mm_current_write_count_2;
   regNameToValue["mem_dma_s2mm_current_write_count_3"] = aie2ps::mem_dma_s2mm_current_write_count_3;
   regNameToValue["mem_dma_s2mm_current_write_count_4"] = aie2ps::mem_dma_s2mm_current_write_count_4;
   regNameToValue["mem_dma_s2mm_current_write_count_5"] = aie2ps::mem_dma_s2mm_current_write_count_5;
   regNameToValue["mem_dma_s2mm_fot_count_fifo_pop_0"] = aie2ps::mem_dma_s2mm_fot_count_fifo_pop_0;
   regNameToValue["mem_dma_s2mm_fot_count_fifo_pop_1"] = aie2ps::mem_dma_s2mm_fot_count_fifo_pop_1;
   regNameToValue["mem_dma_s2mm_fot_count_fifo_pop_2"] = aie2ps::mem_dma_s2mm_fot_count_fifo_pop_2;
   regNameToValue["mem_dma_s2mm_fot_count_fifo_pop_3"] = aie2ps::mem_dma_s2mm_fot_count_fifo_pop_3;
   regNameToValue["mem_dma_s2mm_fot_count_fifo_pop_4"] = aie2ps::mem_dma_s2mm_fot_count_fifo_pop_4;
   regNameToValue["mem_dma_s2mm_fot_count_fifo_pop_5"] = aie2ps::mem_dma_s2mm_fot_count_fifo_pop_5;
   regNameToValue["mem_dma_s2mm_status_0"] = aie2ps::mem_dma_s2mm_status_0;
   regNameToValue["mem_dma_s2mm_status_1"] = aie2ps::mem_dma_s2mm_status_1;
   regNameToValue["mem_dma_s2mm_status_2"] = aie2ps::mem_dma_s2mm_status_2;
   regNameToValue["mem_dma_s2mm_status_3"] = aie2ps::mem_dma_s2mm_status_3;
   regNameToValue["mem_dma_s2mm_status_4"] = aie2ps::mem_dma_s2mm_status_4;
   regNameToValue["mem_dma_s2mm_status_5"] = aie2ps::mem_dma_s2mm_status_5;
   regNameToValue["mem_ecc_failing_address"] = aie2ps::mem_ecc_failing_address;
   regNameToValue["mem_ecc_scrubbing_event"] = aie2ps::mem_ecc_scrubbing_event;
   regNameToValue["mem_edge_detection_event_control"] = aie2ps::mem_edge_detection_event_control;
   regNameToValue["mem_event_broadcast0"] = aie2ps::mem_event_broadcast0;
   regNameToValue["mem_event_broadcast1"] = aie2ps::mem_event_broadcast1;
   regNameToValue["mem_event_broadcast10"] = aie2ps::mem_event_broadcast10;
   regNameToValue["mem_event_broadcast11"] = aie2ps::mem_event_broadcast11;
   regNameToValue["mem_event_broadcast12"] = aie2ps::mem_event_broadcast12;
   regNameToValue["mem_event_broadcast13"] = aie2ps::mem_event_broadcast13;
   regNameToValue["mem_event_broadcast14"] = aie2ps::mem_event_broadcast14;
   regNameToValue["mem_event_broadcast15"] = aie2ps::mem_event_broadcast15;
   regNameToValue["mem_event_broadcast2"] = aie2ps::mem_event_broadcast2;
   regNameToValue["mem_event_broadcast3"] = aie2ps::mem_event_broadcast3;
   regNameToValue["mem_event_broadcast4"] = aie2ps::mem_event_broadcast4;
   regNameToValue["mem_event_broadcast5"] = aie2ps::mem_event_broadcast5;
   regNameToValue["mem_event_broadcast6"] = aie2ps::mem_event_broadcast6;
   regNameToValue["mem_event_broadcast7"] = aie2ps::mem_event_broadcast7;
   regNameToValue["mem_event_broadcast8"] = aie2ps::mem_event_broadcast8;
   regNameToValue["mem_event_broadcast9"] = aie2ps::mem_event_broadcast9;
   regNameToValue["mem_event_broadcast_a_block_east_clr"] = aie2ps::mem_event_broadcast_a_block_east_clr;
   regNameToValue["mem_event_broadcast_a_block_east_set"] = aie2ps::mem_event_broadcast_a_block_east_set;
   regNameToValue["mem_event_broadcast_a_block_east_value"] = aie2ps::mem_event_broadcast_a_block_east_value;
   regNameToValue["mem_event_broadcast_a_block_north_clr"] = aie2ps::mem_event_broadcast_a_block_north_clr;
   regNameToValue["mem_event_broadcast_a_block_north_set"] = aie2ps::mem_event_broadcast_a_block_north_set;
   regNameToValue["mem_event_broadcast_a_block_north_value"] = aie2ps::mem_event_broadcast_a_block_north_value;
   regNameToValue["mem_event_broadcast_a_block_south_clr"] = aie2ps::mem_event_broadcast_a_block_south_clr;
   regNameToValue["mem_event_broadcast_a_block_south_set"] = aie2ps::mem_event_broadcast_a_block_south_set;
   regNameToValue["mem_event_broadcast_a_block_south_value"] = aie2ps::mem_event_broadcast_a_block_south_value;
   regNameToValue["mem_event_broadcast_a_block_west_clr"] = aie2ps::mem_event_broadcast_a_block_west_clr;
   regNameToValue["mem_event_broadcast_a_block_west_set"] = aie2ps::mem_event_broadcast_a_block_west_set;
   regNameToValue["mem_event_broadcast_a_block_west_value"] = aie2ps::mem_event_broadcast_a_block_west_value;
   regNameToValue["mem_event_broadcast_b_block_east_clr"] = aie2ps::mem_event_broadcast_b_block_east_clr;
   regNameToValue["mem_event_broadcast_b_block_east_set"] = aie2ps::mem_event_broadcast_b_block_east_set;
   regNameToValue["mem_event_broadcast_b_block_east_value"] = aie2ps::mem_event_broadcast_b_block_east_value;
   regNameToValue["mem_event_broadcast_b_block_north_clr"] = aie2ps::mem_event_broadcast_b_block_north_clr;
   regNameToValue["mem_event_broadcast_b_block_north_set"] = aie2ps::mem_event_broadcast_b_block_north_set;
   regNameToValue["mem_event_broadcast_b_block_north_value"] = aie2ps::mem_event_broadcast_b_block_north_value;
   regNameToValue["mem_event_broadcast_b_block_south_clr"] = aie2ps::mem_event_broadcast_b_block_south_clr;
   regNameToValue["mem_event_broadcast_b_block_south_set"] = aie2ps::mem_event_broadcast_b_block_south_set;
   regNameToValue["mem_event_broadcast_b_block_south_value"] = aie2ps::mem_event_broadcast_b_block_south_value;
   regNameToValue["mem_event_broadcast_b_block_west_clr"] = aie2ps::mem_event_broadcast_b_block_west_clr;
   regNameToValue["mem_event_broadcast_b_block_west_set"] = aie2ps::mem_event_broadcast_b_block_west_set;
   regNameToValue["mem_event_broadcast_b_block_west_value"] = aie2ps::mem_event_broadcast_b_block_west_value;
   regNameToValue["mem_event_generate"] = aie2ps::mem_event_generate;
   regNameToValue["mem_event_group_0_enable"] = aie2ps::mem_event_group_0_enable;
   regNameToValue["mem_event_group_broadcast_enable"] = aie2ps::mem_event_group_broadcast_enable;
   regNameToValue["mem_event_group_dma_enable"] = aie2ps::mem_event_group_dma_enable;
   regNameToValue["mem_event_group_error_enable"] = aie2ps::mem_event_group_error_enable;
   regNameToValue["mem_event_group_lock_enable"] = aie2ps::mem_event_group_lock_enable;
   regNameToValue["mem_event_group_memory_conflict_enable"] = aie2ps::mem_event_group_memory_conflict_enable;
   regNameToValue["mem_event_group_stream_switch_enable"] = aie2ps::mem_event_group_stream_switch_enable;
   regNameToValue["mem_event_group_user_event_enable"] = aie2ps::mem_event_group_user_event_enable;
   regNameToValue["mem_event_group_watchpoint_enable"] = aie2ps::mem_event_group_watchpoint_enable;
   regNameToValue["mem_event_status0"] = aie2ps::mem_event_status0;
   regNameToValue["mem_event_status1"] = aie2ps::mem_event_status1;
   regNameToValue["mem_event_status2"] = aie2ps::mem_event_status2;
   regNameToValue["mem_event_status3"] = aie2ps::mem_event_status3;
   regNameToValue["mem_event_status4"] = aie2ps::mem_event_status4;
   regNameToValue["mem_event_status5"] = aie2ps::mem_event_status5;
   regNameToValue["mem_lock0_value"] = aie2ps::mem_lock0_value;
   regNameToValue["mem_lock10_value"] = aie2ps::mem_lock10_value;
   regNameToValue["mem_lock11_value"] = aie2ps::mem_lock11_value;
   regNameToValue["mem_lock12_value"] = aie2ps::mem_lock12_value;
   regNameToValue["mem_lock13_value"] = aie2ps::mem_lock13_value;
   regNameToValue["mem_lock14_value"] = aie2ps::mem_lock14_value;
   regNameToValue["mem_lock15_value"] = aie2ps::mem_lock15_value;
   regNameToValue["mem_lock16_value"] = aie2ps::mem_lock16_value;
   regNameToValue["mem_lock17_value"] = aie2ps::mem_lock17_value;
   regNameToValue["mem_lock18_value"] = aie2ps::mem_lock18_value;
   regNameToValue["mem_lock19_value"] = aie2ps::mem_lock19_value;
   regNameToValue["mem_lock1_value"] = aie2ps::mem_lock1_value;
   regNameToValue["mem_lock20_value"] = aie2ps::mem_lock20_value;
   regNameToValue["mem_lock21_value"] = aie2ps::mem_lock21_value;
   regNameToValue["mem_lock22_value"] = aie2ps::mem_lock22_value;
   regNameToValue["mem_lock23_value"] = aie2ps::mem_lock23_value;
   regNameToValue["mem_lock24_value"] = aie2ps::mem_lock24_value;
   regNameToValue["mem_lock25_value"] = aie2ps::mem_lock25_value;
   regNameToValue["mem_lock26_value"] = aie2ps::mem_lock26_value;
   regNameToValue["mem_lock27_value"] = aie2ps::mem_lock27_value;
   regNameToValue["mem_lock28_value"] = aie2ps::mem_lock28_value;
   regNameToValue["mem_lock29_value"] = aie2ps::mem_lock29_value;
   regNameToValue["mem_lock2_value"] = aie2ps::mem_lock2_value;
   regNameToValue["mem_lock30_value"] = aie2ps::mem_lock30_value;
   regNameToValue["mem_lock31_value"] = aie2ps::mem_lock31_value;
   regNameToValue["mem_lock32_value"] = aie2ps::mem_lock32_value;
   regNameToValue["mem_lock33_value"] = aie2ps::mem_lock33_value;
   regNameToValue["mem_lock34_value"] = aie2ps::mem_lock34_value;
   regNameToValue["mem_lock35_value"] = aie2ps::mem_lock35_value;
   regNameToValue["mem_lock36_value"] = aie2ps::mem_lock36_value;
   regNameToValue["mem_lock37_value"] = aie2ps::mem_lock37_value;
   regNameToValue["mem_lock38_value"] = aie2ps::mem_lock38_value;
   regNameToValue["mem_lock39_value"] = aie2ps::mem_lock39_value;
   regNameToValue["mem_lock3_value"] = aie2ps::mem_lock3_value;
   regNameToValue["mem_lock40_value"] = aie2ps::mem_lock40_value;
   regNameToValue["mem_lock41_value"] = aie2ps::mem_lock41_value;
   regNameToValue["mem_lock42_value"] = aie2ps::mem_lock42_value;
   regNameToValue["mem_lock43_value"] = aie2ps::mem_lock43_value;
   regNameToValue["mem_lock44_value"] = aie2ps::mem_lock44_value;
   regNameToValue["mem_lock45_value"] = aie2ps::mem_lock45_value;
   regNameToValue["mem_lock46_value"] = aie2ps::mem_lock46_value;
   regNameToValue["mem_lock47_value"] = aie2ps::mem_lock47_value;
   regNameToValue["mem_lock48_value"] = aie2ps::mem_lock48_value;
   regNameToValue["mem_lock49_value"] = aie2ps::mem_lock49_value;
   regNameToValue["mem_lock4_value"] = aie2ps::mem_lock4_value;
   regNameToValue["mem_lock50_value"] = aie2ps::mem_lock50_value;
   regNameToValue["mem_lock51_value"] = aie2ps::mem_lock51_value;
   regNameToValue["mem_lock52_value"] = aie2ps::mem_lock52_value;
   regNameToValue["mem_lock53_value"] = aie2ps::mem_lock53_value;
   regNameToValue["mem_lock54_value"] = aie2ps::mem_lock54_value;
   regNameToValue["mem_lock55_value"] = aie2ps::mem_lock55_value;
   regNameToValue["mem_lock56_value"] = aie2ps::mem_lock56_value;
   regNameToValue["mem_lock57_value"] = aie2ps::mem_lock57_value;
   regNameToValue["mem_lock58_value"] = aie2ps::mem_lock58_value;
   regNameToValue["mem_lock59_value"] = aie2ps::mem_lock59_value;
   regNameToValue["mem_lock5_value"] = aie2ps::mem_lock5_value;
   regNameToValue["mem_lock60_value"] = aie2ps::mem_lock60_value;
   regNameToValue["mem_lock61_value"] = aie2ps::mem_lock61_value;
   regNameToValue["mem_lock62_value"] = aie2ps::mem_lock62_value;
   regNameToValue["mem_lock63_value"] = aie2ps::mem_lock63_value;
   regNameToValue["mem_lock6_value"] = aie2ps::mem_lock6_value;
   regNameToValue["mem_lock7_value"] = aie2ps::mem_lock7_value;
   regNameToValue["mem_lock8_value"] = aie2ps::mem_lock8_value;
   regNameToValue["mem_lock9_value"] = aie2ps::mem_lock9_value;
   regNameToValue["mem_lock_request"] = aie2ps::mem_lock_request;
   regNameToValue["mem_locks_event_selection_0"] = aie2ps::mem_locks_event_selection_0;
   regNameToValue["mem_locks_event_selection_1"] = aie2ps::mem_locks_event_selection_1;
   regNameToValue["mem_locks_event_selection_2"] = aie2ps::mem_locks_event_selection_2;
   regNameToValue["mem_locks_event_selection_3"] = aie2ps::mem_locks_event_selection_3;
   regNameToValue["mem_locks_event_selection_4"] = aie2ps::mem_locks_event_selection_4;
   regNameToValue["mem_locks_event_selection_5"] = aie2ps::mem_locks_event_selection_5;
   regNameToValue["mem_locks_event_selection_6"] = aie2ps::mem_locks_event_selection_6;
   regNameToValue["mem_locks_event_selection_7"] = aie2ps::mem_locks_event_selection_7;
   regNameToValue["mem_locks_overflow_0"] = aie2ps::mem_locks_overflow_0;
   regNameToValue["mem_locks_overflow_1"] = aie2ps::mem_locks_overflow_1;
   regNameToValue["mem_locks_underflow_0"] = aie2ps::mem_locks_underflow_0;
   regNameToValue["mem_locks_underflow_1"] = aie2ps::mem_locks_underflow_1;
   regNameToValue["mem_memory_control"] = aie2ps::mem_memory_control;
   regNameToValue["mem_module_clock_control"] = aie2ps::mem_module_clock_control;
   regNameToValue["mem_module_reset_control"] = aie2ps::mem_module_reset_control;
   regNameToValue["mem_performance_control0"] = aie2ps::mem_performance_control0;
   regNameToValue["mem_performance_control1"] = aie2ps::mem_performance_control1;
   regNameToValue["mem_performance_control2"] = aie2ps::mem_performance_control2;
   regNameToValue["mem_performance_control3"] = aie2ps::mem_performance_control3;
   regNameToValue["mem_performance_control4"] = aie2ps::mem_performance_control4;
   regNameToValue["mem_performance_counter0"] = aie2ps::mem_performance_counter0;
   regNameToValue["mem_performance_counter0_event_value"] = aie2ps::mem_performance_counter0_event_value;
   regNameToValue["mem_performance_counter1"] = aie2ps::mem_performance_counter1;
   regNameToValue["mem_performance_counter1_event_value"] = aie2ps::mem_performance_counter1_event_value;
   regNameToValue["mem_performance_counter2"] = aie2ps::mem_performance_counter2;
   regNameToValue["mem_performance_counter2_event_value"] = aie2ps::mem_performance_counter2_event_value;
   regNameToValue["mem_performance_counter3"] = aie2ps::mem_performance_counter3;
   regNameToValue["mem_performance_counter3_event_value"] = aie2ps::mem_performance_counter3_event_value;
   regNameToValue["mem_performance_counter4"] = aie2ps::mem_performance_counter4;
   regNameToValue["mem_performance_counter5"] = aie2ps::mem_performance_counter5;
   regNameToValue["mem_spare_reg"] = aie2ps::mem_spare_reg;
   regNameToValue["mem_spare_reg_privileged"] = aie2ps::mem_spare_reg_privileged;
   regNameToValue["mem_stream_switch_adaptive_clock_gate_abort_period"] = aie2ps::mem_stream_switch_adaptive_clock_gate_abort_period;
   regNameToValue["mem_stream_switch_adaptive_clock_gate_status"] = aie2ps::mem_stream_switch_adaptive_clock_gate_status;
   regNameToValue["mem_stream_switch_deterministic_merge_arb0_ctrl"] = aie2ps::mem_stream_switch_deterministic_merge_arb0_ctrl;
   regNameToValue["mem_stream_switch_deterministic_merge_arb0_slave0_1"] = aie2ps::mem_stream_switch_deterministic_merge_arb0_slave0_1;
   regNameToValue["mem_stream_switch_deterministic_merge_arb0_slave2_3"] = aie2ps::mem_stream_switch_deterministic_merge_arb0_slave2_3;
   regNameToValue["mem_stream_switch_deterministic_merge_arb1_ctrl"] = aie2ps::mem_stream_switch_deterministic_merge_arb1_ctrl;
   regNameToValue["mem_stream_switch_deterministic_merge_arb1_slave0_1"] = aie2ps::mem_stream_switch_deterministic_merge_arb1_slave0_1;
   regNameToValue["mem_stream_switch_deterministic_merge_arb1_slave2_3"] = aie2ps::mem_stream_switch_deterministic_merge_arb1_slave2_3;
   regNameToValue["mem_stream_switch_event_port_selection_0"] = aie2ps::mem_stream_switch_event_port_selection_0;
   regNameToValue["mem_stream_switch_event_port_selection_1"] = aie2ps::mem_stream_switch_event_port_selection_1;
   regNameToValue["mem_stream_switch_master_config_dma0"] = aie2ps::mem_stream_switch_master_config_dma0;
   regNameToValue["mem_stream_switch_master_config_dma1"] = aie2ps::mem_stream_switch_master_config_dma1;
   regNameToValue["mem_stream_switch_master_config_dma2"] = aie2ps::mem_stream_switch_master_config_dma2;
   regNameToValue["mem_stream_switch_master_config_dma3"] = aie2ps::mem_stream_switch_master_config_dma3;
   regNameToValue["mem_stream_switch_master_config_dma4"] = aie2ps::mem_stream_switch_master_config_dma4;
   regNameToValue["mem_stream_switch_master_config_dma5"] = aie2ps::mem_stream_switch_master_config_dma5;
   regNameToValue["mem_stream_switch_master_config_north0"] = aie2ps::mem_stream_switch_master_config_north0;
   regNameToValue["mem_stream_switch_master_config_north1"] = aie2ps::mem_stream_switch_master_config_north1;
   regNameToValue["mem_stream_switch_master_config_north2"] = aie2ps::mem_stream_switch_master_config_north2;
   regNameToValue["mem_stream_switch_master_config_north3"] = aie2ps::mem_stream_switch_master_config_north3;
   regNameToValue["mem_stream_switch_master_config_north4"] = aie2ps::mem_stream_switch_master_config_north4;
   regNameToValue["mem_stream_switch_master_config_north5"] = aie2ps::mem_stream_switch_master_config_north5;
   regNameToValue["mem_stream_switch_master_config_south0"] = aie2ps::mem_stream_switch_master_config_south0;
   regNameToValue["mem_stream_switch_master_config_south1"] = aie2ps::mem_stream_switch_master_config_south1;
   regNameToValue["mem_stream_switch_master_config_south2"] = aie2ps::mem_stream_switch_master_config_south2;
   regNameToValue["mem_stream_switch_master_config_south3"] = aie2ps::mem_stream_switch_master_config_south3;
   regNameToValue["mem_stream_switch_master_config_tile_ctrl"] = aie2ps::mem_stream_switch_master_config_tile_ctrl;
   regNameToValue["mem_stream_switch_parity_injection"] = aie2ps::mem_stream_switch_parity_injection;
   regNameToValue["mem_stream_switch_parity_status"] = aie2ps::mem_stream_switch_parity_status;
   regNameToValue["mem_stream_switch_slave_config_dma_0"] = aie2ps::mem_stream_switch_slave_config_dma_0;
   regNameToValue["mem_stream_switch_slave_config_dma_1"] = aie2ps::mem_stream_switch_slave_config_dma_1;
   regNameToValue["mem_stream_switch_slave_config_dma_2"] = aie2ps::mem_stream_switch_slave_config_dma_2;
   regNameToValue["mem_stream_switch_slave_config_dma_3"] = aie2ps::mem_stream_switch_slave_config_dma_3;
   regNameToValue["mem_stream_switch_slave_config_dma_4"] = aie2ps::mem_stream_switch_slave_config_dma_4;
   regNameToValue["mem_stream_switch_slave_config_dma_5"] = aie2ps::mem_stream_switch_slave_config_dma_5;
   regNameToValue["mem_stream_switch_slave_config_north_0"] = aie2ps::mem_stream_switch_slave_config_north_0;
   regNameToValue["mem_stream_switch_slave_config_north_1"] = aie2ps::mem_stream_switch_slave_config_north_1;
   regNameToValue["mem_stream_switch_slave_config_north_2"] = aie2ps::mem_stream_switch_slave_config_north_2;
   regNameToValue["mem_stream_switch_slave_config_north_3"] = aie2ps::mem_stream_switch_slave_config_north_3;
   regNameToValue["mem_stream_switch_slave_config_south_0"] = aie2ps::mem_stream_switch_slave_config_south_0;
   regNameToValue["mem_stream_switch_slave_config_south_1"] = aie2ps::mem_stream_switch_slave_config_south_1;
   regNameToValue["mem_stream_switch_slave_config_south_2"] = aie2ps::mem_stream_switch_slave_config_south_2;
   regNameToValue["mem_stream_switch_slave_config_south_3"] = aie2ps::mem_stream_switch_slave_config_south_3;
   regNameToValue["mem_stream_switch_slave_config_south_4"] = aie2ps::mem_stream_switch_slave_config_south_4;
   regNameToValue["mem_stream_switch_slave_config_south_5"] = aie2ps::mem_stream_switch_slave_config_south_5;
   regNameToValue["mem_stream_switch_slave_config_tile_ctrl"] = aie2ps::mem_stream_switch_slave_config_tile_ctrl;
   regNameToValue["mem_stream_switch_slave_config_trace"] = aie2ps::mem_stream_switch_slave_config_trace;
   regNameToValue["mem_stream_switch_slave_dma_0_slot0"] = aie2ps::mem_stream_switch_slave_dma_0_slot0;
   regNameToValue["mem_stream_switch_slave_dma_0_slot1"] = aie2ps::mem_stream_switch_slave_dma_0_slot1;
   regNameToValue["mem_stream_switch_slave_dma_0_slot2"] = aie2ps::mem_stream_switch_slave_dma_0_slot2;
   regNameToValue["mem_stream_switch_slave_dma_0_slot3"] = aie2ps::mem_stream_switch_slave_dma_0_slot3;
   regNameToValue["mem_stream_switch_slave_dma_1_slot0"] = aie2ps::mem_stream_switch_slave_dma_1_slot0;
   regNameToValue["mem_stream_switch_slave_dma_1_slot1"] = aie2ps::mem_stream_switch_slave_dma_1_slot1;
   regNameToValue["mem_stream_switch_slave_dma_1_slot2"] = aie2ps::mem_stream_switch_slave_dma_1_slot2;
   regNameToValue["mem_stream_switch_slave_dma_1_slot3"] = aie2ps::mem_stream_switch_slave_dma_1_slot3;
   regNameToValue["mem_stream_switch_slave_dma_2_slot0"] = aie2ps::mem_stream_switch_slave_dma_2_slot0;
   regNameToValue["mem_stream_switch_slave_dma_2_slot1"] = aie2ps::mem_stream_switch_slave_dma_2_slot1;
   regNameToValue["mem_stream_switch_slave_dma_2_slot2"] = aie2ps::mem_stream_switch_slave_dma_2_slot2;
   regNameToValue["mem_stream_switch_slave_dma_2_slot3"] = aie2ps::mem_stream_switch_slave_dma_2_slot3;
   regNameToValue["mem_stream_switch_slave_dma_3_slot0"] = aie2ps::mem_stream_switch_slave_dma_3_slot0;
   regNameToValue["mem_stream_switch_slave_dma_3_slot1"] = aie2ps::mem_stream_switch_slave_dma_3_slot1;
   regNameToValue["mem_stream_switch_slave_dma_3_slot2"] = aie2ps::mem_stream_switch_slave_dma_3_slot2;
   regNameToValue["mem_stream_switch_slave_dma_3_slot3"] = aie2ps::mem_stream_switch_slave_dma_3_slot3;
   regNameToValue["mem_stream_switch_slave_dma_4_slot0"] = aie2ps::mem_stream_switch_slave_dma_4_slot0;
   regNameToValue["mem_stream_switch_slave_dma_4_slot1"] = aie2ps::mem_stream_switch_slave_dma_4_slot1;
   regNameToValue["mem_stream_switch_slave_dma_4_slot2"] = aie2ps::mem_stream_switch_slave_dma_4_slot2;
   regNameToValue["mem_stream_switch_slave_dma_4_slot3"] = aie2ps::mem_stream_switch_slave_dma_4_slot3;
   regNameToValue["mem_stream_switch_slave_dma_5_slot0"] = aie2ps::mem_stream_switch_slave_dma_5_slot0;
   regNameToValue["mem_stream_switch_slave_dma_5_slot1"] = aie2ps::mem_stream_switch_slave_dma_5_slot1;
   regNameToValue["mem_stream_switch_slave_dma_5_slot2"] = aie2ps::mem_stream_switch_slave_dma_5_slot2;
   regNameToValue["mem_stream_switch_slave_dma_5_slot3"] = aie2ps::mem_stream_switch_slave_dma_5_slot3;
   regNameToValue["mem_stream_switch_slave_north_0_slot0"] = aie2ps::mem_stream_switch_slave_north_0_slot0;
   regNameToValue["mem_stream_switch_slave_north_0_slot1"] = aie2ps::mem_stream_switch_slave_north_0_slot1;
   regNameToValue["mem_stream_switch_slave_north_0_slot2"] = aie2ps::mem_stream_switch_slave_north_0_slot2;
   regNameToValue["mem_stream_switch_slave_north_0_slot3"] = aie2ps::mem_stream_switch_slave_north_0_slot3;
   regNameToValue["mem_stream_switch_slave_north_1_slot0"] = aie2ps::mem_stream_switch_slave_north_1_slot0;
   regNameToValue["mem_stream_switch_slave_north_1_slot1"] = aie2ps::mem_stream_switch_slave_north_1_slot1;
   regNameToValue["mem_stream_switch_slave_north_1_slot2"] = aie2ps::mem_stream_switch_slave_north_1_slot2;
   regNameToValue["mem_stream_switch_slave_north_1_slot3"] = aie2ps::mem_stream_switch_slave_north_1_slot3;
   regNameToValue["mem_stream_switch_slave_north_2_slot0"] = aie2ps::mem_stream_switch_slave_north_2_slot0;
   regNameToValue["mem_stream_switch_slave_north_2_slot1"] = aie2ps::mem_stream_switch_slave_north_2_slot1;
   regNameToValue["mem_stream_switch_slave_north_2_slot2"] = aie2ps::mem_stream_switch_slave_north_2_slot2;
   regNameToValue["mem_stream_switch_slave_north_2_slot3"] = aie2ps::mem_stream_switch_slave_north_2_slot3;
   regNameToValue["mem_stream_switch_slave_north_3_slot0"] = aie2ps::mem_stream_switch_slave_north_3_slot0;
   regNameToValue["mem_stream_switch_slave_north_3_slot1"] = aie2ps::mem_stream_switch_slave_north_3_slot1;
   regNameToValue["mem_stream_switch_slave_north_3_slot2"] = aie2ps::mem_stream_switch_slave_north_3_slot2;
   regNameToValue["mem_stream_switch_slave_north_3_slot3"] = aie2ps::mem_stream_switch_slave_north_3_slot3;
   regNameToValue["mem_stream_switch_slave_south_0_slot0"] = aie2ps::mem_stream_switch_slave_south_0_slot0;
   regNameToValue["mem_stream_switch_slave_south_0_slot1"] = aie2ps::mem_stream_switch_slave_south_0_slot1;
   regNameToValue["mem_stream_switch_slave_south_0_slot2"] = aie2ps::mem_stream_switch_slave_south_0_slot2;
   regNameToValue["mem_stream_switch_slave_south_0_slot3"] = aie2ps::mem_stream_switch_slave_south_0_slot3;
   regNameToValue["mem_stream_switch_slave_south_1_slot0"] = aie2ps::mem_stream_switch_slave_south_1_slot0;
   regNameToValue["mem_stream_switch_slave_south_1_slot1"] = aie2ps::mem_stream_switch_slave_south_1_slot1;
   regNameToValue["mem_stream_switch_slave_south_1_slot2"] = aie2ps::mem_stream_switch_slave_south_1_slot2;
   regNameToValue["mem_stream_switch_slave_south_1_slot3"] = aie2ps::mem_stream_switch_slave_south_1_slot3;
   regNameToValue["mem_stream_switch_slave_south_2_slot0"] = aie2ps::mem_stream_switch_slave_south_2_slot0;
   regNameToValue["mem_stream_switch_slave_south_2_slot1"] = aie2ps::mem_stream_switch_slave_south_2_slot1;
   regNameToValue["mem_stream_switch_slave_south_2_slot2"] = aie2ps::mem_stream_switch_slave_south_2_slot2;
   regNameToValue["mem_stream_switch_slave_south_2_slot3"] = aie2ps::mem_stream_switch_slave_south_2_slot3;
   regNameToValue["mem_stream_switch_slave_south_3_slot0"] = aie2ps::mem_stream_switch_slave_south_3_slot0;
   regNameToValue["mem_stream_switch_slave_south_3_slot1"] = aie2ps::mem_stream_switch_slave_south_3_slot1;
   regNameToValue["mem_stream_switch_slave_south_3_slot2"] = aie2ps::mem_stream_switch_slave_south_3_slot2;
   regNameToValue["mem_stream_switch_slave_south_3_slot3"] = aie2ps::mem_stream_switch_slave_south_3_slot3;
   regNameToValue["mem_stream_switch_slave_south_4_slot0"] = aie2ps::mem_stream_switch_slave_south_4_slot0;
   regNameToValue["mem_stream_switch_slave_south_4_slot1"] = aie2ps::mem_stream_switch_slave_south_4_slot1;
   regNameToValue["mem_stream_switch_slave_south_4_slot2"] = aie2ps::mem_stream_switch_slave_south_4_slot2;
   regNameToValue["mem_stream_switch_slave_south_4_slot3"] = aie2ps::mem_stream_switch_slave_south_4_slot3;
   regNameToValue["mem_stream_switch_slave_south_5_slot0"] = aie2ps::mem_stream_switch_slave_south_5_slot0;
   regNameToValue["mem_stream_switch_slave_south_5_slot1"] = aie2ps::mem_stream_switch_slave_south_5_slot1;
   regNameToValue["mem_stream_switch_slave_south_5_slot2"] = aie2ps::mem_stream_switch_slave_south_5_slot2;
   regNameToValue["mem_stream_switch_slave_south_5_slot3"] = aie2ps::mem_stream_switch_slave_south_5_slot3;
   regNameToValue["mem_stream_switch_slave_tile_ctrl_slot0"] = aie2ps::mem_stream_switch_slave_tile_ctrl_slot0;
   regNameToValue["mem_stream_switch_slave_tile_ctrl_slot1"] = aie2ps::mem_stream_switch_slave_tile_ctrl_slot1;
   regNameToValue["mem_stream_switch_slave_tile_ctrl_slot2"] = aie2ps::mem_stream_switch_slave_tile_ctrl_slot2;
   regNameToValue["mem_stream_switch_slave_tile_ctrl_slot3"] = aie2ps::mem_stream_switch_slave_tile_ctrl_slot3;
   regNameToValue["mem_stream_switch_slave_trace_slot0"] = aie2ps::mem_stream_switch_slave_trace_slot0;
   regNameToValue["mem_stream_switch_slave_trace_slot1"] = aie2ps::mem_stream_switch_slave_trace_slot1;
   regNameToValue["mem_stream_switch_slave_trace_slot2"] = aie2ps::mem_stream_switch_slave_trace_slot2;
   regNameToValue["mem_stream_switch_slave_trace_slot3"] = aie2ps::mem_stream_switch_slave_trace_slot3;
   regNameToValue["mem_tile_control"] = aie2ps::mem_tile_control;
   regNameToValue["mem_tile_control_packet_handler_status"] = aie2ps::mem_tile_control_packet_handler_status;
   regNameToValue["mem_timer_control"] = aie2ps::mem_timer_control;
   regNameToValue["mem_timer_high"] = aie2ps::mem_timer_high;
   regNameToValue["mem_timer_low"] = aie2ps::mem_timer_low;
   regNameToValue["mem_timer_trig_event_high_value"] = aie2ps::mem_timer_trig_event_high_value;
   regNameToValue["mem_timer_trig_event_low_value"] = aie2ps::mem_timer_trig_event_low_value;
   regNameToValue["mem_trace_control0"] = aie2ps::mem_trace_control0;
   regNameToValue["mem_trace_control1"] = aie2ps::mem_trace_control1;
   regNameToValue["mem_trace_event0"] = aie2ps::mem_trace_event0;
   regNameToValue["mem_trace_event1"] = aie2ps::mem_trace_event1;
   regNameToValue["mem_trace_status"] = aie2ps::mem_trace_status;
   regNameToValue["mem_watchpoint0"] = aie2ps::mem_watchpoint0;
   regNameToValue["mem_watchpoint1"] = aie2ps::mem_watchpoint1;
   regNameToValue["mem_watchpoint2"] = aie2ps::mem_watchpoint2;
   regNameToValue["mem_watchpoint3"] = aie2ps::mem_watchpoint3;
   regNameToValue["shim_lock0_value"] = aie2ps::shim_lock0_value;
   regNameToValue["shim_lock1_value"] = aie2ps::shim_lock1_value;
   regNameToValue["shim_lock2_value"] = aie2ps::shim_lock2_value;
   regNameToValue["shim_lock3_value"] = aie2ps::shim_lock3_value;
   regNameToValue["shim_lock4_value"] = aie2ps::shim_lock4_value;
   regNameToValue["shim_lock5_value"] = aie2ps::shim_lock5_value;
   regNameToValue["shim_lock6_value"] = aie2ps::shim_lock6_value;
   regNameToValue["shim_lock7_value"] = aie2ps::shim_lock7_value;
   regNameToValue["shim_lock8_value"] = aie2ps::shim_lock8_value;
   regNameToValue["shim_lock9_value"] = aie2ps::shim_lock9_value;
   regNameToValue["shim_lock10_value"] = aie2ps::shim_lock10_value;
   regNameToValue["shim_lock11_value"] = aie2ps::shim_lock11_value;
   regNameToValue["shim_lock12_value"] = aie2ps::shim_lock12_value;
   regNameToValue["shim_lock13_value"] = aie2ps::shim_lock13_value;
   regNameToValue["shim_lock14_value"] = aie2ps::shim_lock14_value;
   regNameToValue["shim_lock15_value"] = aie2ps::shim_lock15_value;
   regNameToValue["shim_locks_event_selection_0"] = aie2ps::shim_locks_event_selection_0;
   regNameToValue["shim_locks_event_selection_1"] = aie2ps::shim_locks_event_selection_1;
   regNameToValue["shim_locks_event_selection_2"] = aie2ps::shim_locks_event_selection_2;
   regNameToValue["shim_locks_event_selection_3"] = aie2ps::shim_locks_event_selection_3;
   regNameToValue["shim_locks_event_selection_4"] = aie2ps::shim_locks_event_selection_4;
   regNameToValue["shim_locks_event_selection_5"] = aie2ps::shim_locks_event_selection_5;
   regNameToValue["shim_locks_overflow"] = aie2ps::shim_locks_overflow;
   regNameToValue["shim_locks_underflow"] = aie2ps::shim_locks_underflow;
   regNameToValue["shim_interrupt_controller_2nd_level_mask"] = aie2ps::shim_interrupt_controller_2nd_level_mask;
   regNameToValue["shim_interrupt_controller_2nd_level_enable"] = aie2ps::shim_interrupt_controller_2nd_level_enable;
   regNameToValue["shim_interrupt_controller_2nd_level_disable"] = aie2ps::shim_interrupt_controller_2nd_level_disable;
   regNameToValue["shim_interrupt_controller_2nd_level_status"] = aie2ps::shim_interrupt_controller_2nd_level_status;
   regNameToValue["shim_interrupt_controller_2nd_level_interrupt"] = aie2ps::shim_interrupt_controller_2nd_level_interrupt;
   regNameToValue["shim_spare_reg"] = aie2ps::shim_spare_reg;
   regNameToValue["shim_me_aximm_config"] = aie2ps::shim_me_aximm_config;
   regNameToValue["shim_mux_config"] = aie2ps::shim_mux_config;
   regNameToValue["shim_demux_config"] = aie2ps::shim_demux_config;
   regNameToValue["shim_axi_mm_outstanding_transactions"] = aie2ps::shim_axi_mm_outstanding_transactions;
   regNameToValue["shim_smid"] = aie2ps::shim_smid;
   regNameToValue["shim_dma_bd0_0"] = aie2ps::shim_dma_bd0_0;
   regNameToValue["shim_dma_bd0_1"] = aie2ps::shim_dma_bd0_1;
   regNameToValue["shim_dma_bd0_2"] = aie2ps::shim_dma_bd0_2;
   regNameToValue["shim_dma_bd0_3"] = aie2ps::shim_dma_bd0_3;
   regNameToValue["shim_dma_bd0_4"] = aie2ps::shim_dma_bd0_4;
   regNameToValue["shim_dma_bd0_5"] = aie2ps::shim_dma_bd0_5;
   regNameToValue["shim_dma_bd0_6"] = aie2ps::shim_dma_bd0_6;
   regNameToValue["shim_dma_bd0_7"] = aie2ps::shim_dma_bd0_7;
   regNameToValue["shim_dma_bd0_8"] = aie2ps::shim_dma_bd0_8;
   regNameToValue["shim_dma_bd1_0"] = aie2ps::shim_dma_bd1_0;
   regNameToValue["shim_dma_bd1_1"] = aie2ps::shim_dma_bd1_1;
   regNameToValue["shim_dma_bd1_2"] = aie2ps::shim_dma_bd1_2;
   regNameToValue["shim_dma_bd1_3"] = aie2ps::shim_dma_bd1_3;
   regNameToValue["shim_dma_bd1_4"] = aie2ps::shim_dma_bd1_4;
   regNameToValue["shim_dma_bd1_5"] = aie2ps::shim_dma_bd1_5;
   regNameToValue["shim_dma_bd1_6"] = aie2ps::shim_dma_bd1_6;
   regNameToValue["shim_dma_bd1_7"] = aie2ps::shim_dma_bd1_7;
   regNameToValue["shim_dma_bd1_8"] = aie2ps::shim_dma_bd1_8;
   regNameToValue["shim_dma_bd2_0"] = aie2ps::shim_dma_bd2_0;
   regNameToValue["shim_dma_bd2_1"] = aie2ps::shim_dma_bd2_1;
   regNameToValue["shim_dma_bd2_2"] = aie2ps::shim_dma_bd2_2;
   regNameToValue["shim_dma_bd2_3"] = aie2ps::shim_dma_bd2_3;
   regNameToValue["shim_dma_bd2_4"] = aie2ps::shim_dma_bd2_4;
   regNameToValue["shim_dma_bd2_5"] = aie2ps::shim_dma_bd2_5;
   regNameToValue["shim_dma_bd2_6"] = aie2ps::shim_dma_bd2_6;
   regNameToValue["shim_dma_bd2_7"] = aie2ps::shim_dma_bd2_7;
   regNameToValue["shim_dma_bd2_8"] = aie2ps::shim_dma_bd2_8;
   regNameToValue["shim_dma_bd3_0"] = aie2ps::shim_dma_bd3_0;
   regNameToValue["shim_dma_bd3_1"] = aie2ps::shim_dma_bd3_1;
   regNameToValue["shim_dma_bd3_2"] = aie2ps::shim_dma_bd3_2;
   regNameToValue["shim_dma_bd3_3"] = aie2ps::shim_dma_bd3_3;
   regNameToValue["shim_dma_bd3_4"] = aie2ps::shim_dma_bd3_4;
   regNameToValue["shim_dma_bd3_5"] = aie2ps::shim_dma_bd3_5;
   regNameToValue["shim_dma_bd3_6"] = aie2ps::shim_dma_bd3_6;
   regNameToValue["shim_dma_bd3_7"] = aie2ps::shim_dma_bd3_7;
   regNameToValue["shim_dma_bd3_8"] = aie2ps::shim_dma_bd3_8;
   regNameToValue["shim_dma_bd4_0"] = aie2ps::shim_dma_bd4_0;
   regNameToValue["shim_dma_bd4_1"] = aie2ps::shim_dma_bd4_1;
   regNameToValue["shim_dma_bd4_2"] = aie2ps::shim_dma_bd4_2;
   regNameToValue["shim_dma_bd4_3"] = aie2ps::shim_dma_bd4_3;
   regNameToValue["shim_dma_bd4_4"] = aie2ps::shim_dma_bd4_4;
   regNameToValue["shim_dma_bd4_5"] = aie2ps::shim_dma_bd4_5;
   regNameToValue["shim_dma_bd4_6"] = aie2ps::shim_dma_bd4_6;
   regNameToValue["shim_dma_bd4_7"] = aie2ps::shim_dma_bd4_7;
   regNameToValue["shim_dma_bd4_8"] = aie2ps::shim_dma_bd4_8;
   regNameToValue["shim_dma_bd5_0"] = aie2ps::shim_dma_bd5_0;
   regNameToValue["shim_dma_bd5_1"] = aie2ps::shim_dma_bd5_1;
   regNameToValue["shim_dma_bd5_2"] = aie2ps::shim_dma_bd5_2;
   regNameToValue["shim_dma_bd5_3"] = aie2ps::shim_dma_bd5_3;
   regNameToValue["shim_dma_bd5_4"] = aie2ps::shim_dma_bd5_4;
   regNameToValue["shim_dma_bd5_5"] = aie2ps::shim_dma_bd5_5;
   regNameToValue["shim_dma_bd5_6"] = aie2ps::shim_dma_bd5_6;
   regNameToValue["shim_dma_bd5_7"] = aie2ps::shim_dma_bd5_7;
   regNameToValue["shim_dma_bd5_8"] = aie2ps::shim_dma_bd5_8;
   regNameToValue["shim_dma_bd6_0"] = aie2ps::shim_dma_bd6_0;
   regNameToValue["shim_dma_bd6_1"] = aie2ps::shim_dma_bd6_1;
   regNameToValue["shim_dma_bd6_2"] = aie2ps::shim_dma_bd6_2;
   regNameToValue["shim_dma_bd6_3"] = aie2ps::shim_dma_bd6_3;
   regNameToValue["shim_dma_bd6_4"] = aie2ps::shim_dma_bd6_4;
   regNameToValue["shim_dma_bd6_5"] = aie2ps::shim_dma_bd6_5;
   regNameToValue["shim_dma_bd6_6"] = aie2ps::shim_dma_bd6_6;
   regNameToValue["shim_dma_bd6_7"] = aie2ps::shim_dma_bd6_7;
   regNameToValue["shim_dma_bd6_8"] = aie2ps::shim_dma_bd6_8;
   regNameToValue["shim_dma_bd7_0"] = aie2ps::shim_dma_bd7_0;
   regNameToValue["shim_dma_bd7_1"] = aie2ps::shim_dma_bd7_1;
   regNameToValue["shim_dma_bd7_2"] = aie2ps::shim_dma_bd7_2;
   regNameToValue["shim_dma_bd7_3"] = aie2ps::shim_dma_bd7_3;
   regNameToValue["shim_dma_bd7_4"] = aie2ps::shim_dma_bd7_4;
   regNameToValue["shim_dma_bd7_5"] = aie2ps::shim_dma_bd7_5;
   regNameToValue["shim_dma_bd7_6"] = aie2ps::shim_dma_bd7_6;
   regNameToValue["shim_dma_bd7_7"] = aie2ps::shim_dma_bd7_7;
   regNameToValue["shim_dma_bd7_8"] = aie2ps::shim_dma_bd7_8;
   regNameToValue["shim_dma_bd8_0"] = aie2ps::shim_dma_bd8_0;
   regNameToValue["shim_dma_bd8_1"] = aie2ps::shim_dma_bd8_1;
   regNameToValue["shim_dma_bd8_2"] = aie2ps::shim_dma_bd8_2;
   regNameToValue["shim_dma_bd8_3"] = aie2ps::shim_dma_bd8_3;
   regNameToValue["shim_dma_bd8_4"] = aie2ps::shim_dma_bd8_4;
   regNameToValue["shim_dma_bd8_5"] = aie2ps::shim_dma_bd8_5;
   regNameToValue["shim_dma_bd8_6"] = aie2ps::shim_dma_bd8_6;
   regNameToValue["shim_dma_bd8_7"] = aie2ps::shim_dma_bd8_7;
   regNameToValue["shim_dma_bd8_8"] = aie2ps::shim_dma_bd8_8;
   regNameToValue["shim_dma_bd9_0"] = aie2ps::shim_dma_bd9_0;
   regNameToValue["shim_dma_bd9_1"] = aie2ps::shim_dma_bd9_1;
   regNameToValue["shim_dma_bd9_2"] = aie2ps::shim_dma_bd9_2;
   regNameToValue["shim_dma_bd9_3"] = aie2ps::shim_dma_bd9_3;
   regNameToValue["shim_dma_bd9_4"] = aie2ps::shim_dma_bd9_4;
   regNameToValue["shim_dma_bd9_5"] = aie2ps::shim_dma_bd9_5;
   regNameToValue["shim_dma_bd9_6"] = aie2ps::shim_dma_bd9_6;
   regNameToValue["shim_dma_bd9_7"] = aie2ps::shim_dma_bd9_7;
   regNameToValue["shim_dma_bd9_8"] = aie2ps::shim_dma_bd9_8;
   regNameToValue["shim_dma_bd10_0"] = aie2ps::shim_dma_bd10_0;
   regNameToValue["shim_dma_bd10_1"] = aie2ps::shim_dma_bd10_1;
   regNameToValue["shim_dma_bd10_2"] = aie2ps::shim_dma_bd10_2;
   regNameToValue["shim_dma_bd10_3"] = aie2ps::shim_dma_bd10_3;
   regNameToValue["shim_dma_bd10_4"] = aie2ps::shim_dma_bd10_4;
   regNameToValue["shim_dma_bd10_5"] = aie2ps::shim_dma_bd10_5;
   regNameToValue["shim_dma_bd10_6"] = aie2ps::shim_dma_bd10_6;
   regNameToValue["shim_dma_bd10_7"] = aie2ps::shim_dma_bd10_7;
   regNameToValue["shim_dma_bd10_8"] = aie2ps::shim_dma_bd10_8;
   regNameToValue["shim_dma_bd11_0"] = aie2ps::shim_dma_bd11_0;
   regNameToValue["shim_dma_bd11_1"] = aie2ps::shim_dma_bd11_1;
   regNameToValue["shim_dma_bd11_2"] = aie2ps::shim_dma_bd11_2;
   regNameToValue["shim_dma_bd11_3"] = aie2ps::shim_dma_bd11_3;
   regNameToValue["shim_dma_bd11_4"] = aie2ps::shim_dma_bd11_4;
   regNameToValue["shim_dma_bd11_5"] = aie2ps::shim_dma_bd11_5;
   regNameToValue["shim_dma_bd11_6"] = aie2ps::shim_dma_bd11_6;
   regNameToValue["shim_dma_bd11_7"] = aie2ps::shim_dma_bd11_7;
   regNameToValue["shim_dma_bd11_8"] = aie2ps::shim_dma_bd11_8;
   regNameToValue["shim_dma_bd12_0"] = aie2ps::shim_dma_bd12_0;
   regNameToValue["shim_dma_bd12_1"] = aie2ps::shim_dma_bd12_1;
   regNameToValue["shim_dma_bd12_2"] = aie2ps::shim_dma_bd12_2;
   regNameToValue["shim_dma_bd12_3"] = aie2ps::shim_dma_bd12_3;
   regNameToValue["shim_dma_bd12_4"] = aie2ps::shim_dma_bd12_4;
   regNameToValue["shim_dma_bd12_5"] = aie2ps::shim_dma_bd12_5;
   regNameToValue["shim_dma_bd12_6"] = aie2ps::shim_dma_bd12_6;
   regNameToValue["shim_dma_bd12_7"] = aie2ps::shim_dma_bd12_7;
   regNameToValue["shim_dma_bd12_8"] = aie2ps::shim_dma_bd12_8;
   regNameToValue["shim_dma_bd13_0"] = aie2ps::shim_dma_bd13_0;
   regNameToValue["shim_dma_bd13_1"] = aie2ps::shim_dma_bd13_1;
   regNameToValue["shim_dma_bd13_2"] = aie2ps::shim_dma_bd13_2;
   regNameToValue["shim_dma_bd13_3"] = aie2ps::shim_dma_bd13_3;
   regNameToValue["shim_dma_bd13_4"] = aie2ps::shim_dma_bd13_4;
   regNameToValue["shim_dma_bd13_5"] = aie2ps::shim_dma_bd13_5;
   regNameToValue["shim_dma_bd13_6"] = aie2ps::shim_dma_bd13_6;
   regNameToValue["shim_dma_bd13_7"] = aie2ps::shim_dma_bd13_7;
   regNameToValue["shim_dma_bd13_8"] = aie2ps::shim_dma_bd13_8;
   regNameToValue["shim_dma_bd14_0"] = aie2ps::shim_dma_bd14_0;
   regNameToValue["shim_dma_bd14_1"] = aie2ps::shim_dma_bd14_1;
   regNameToValue["shim_dma_bd14_2"] = aie2ps::shim_dma_bd14_2;
   regNameToValue["shim_dma_bd14_3"] = aie2ps::shim_dma_bd14_3;
   regNameToValue["shim_dma_bd14_4"] = aie2ps::shim_dma_bd14_4;
   regNameToValue["shim_dma_bd14_5"] = aie2ps::shim_dma_bd14_5;
   regNameToValue["shim_dma_bd14_6"] = aie2ps::shim_dma_bd14_6;
   regNameToValue["shim_dma_bd14_7"] = aie2ps::shim_dma_bd14_7;
   regNameToValue["shim_dma_bd14_8"] = aie2ps::shim_dma_bd14_8;
   regNameToValue["shim_dma_bd15_0"] = aie2ps::shim_dma_bd15_0;
   regNameToValue["shim_dma_bd15_1"] = aie2ps::shim_dma_bd15_1;
   regNameToValue["shim_dma_bd15_2"] = aie2ps::shim_dma_bd15_2;
   regNameToValue["shim_dma_bd15_3"] = aie2ps::shim_dma_bd15_3;
   regNameToValue["shim_dma_bd15_4"] = aie2ps::shim_dma_bd15_4;
   regNameToValue["shim_dma_bd15_5"] = aie2ps::shim_dma_bd15_5;
   regNameToValue["shim_dma_bd15_6"] = aie2ps::shim_dma_bd15_6;
   regNameToValue["shim_dma_bd15_7"] = aie2ps::shim_dma_bd15_7;
   regNameToValue["shim_dma_bd15_8"] = aie2ps::shim_dma_bd15_8;
   regNameToValue["shim_dma_s2mm_0_ctrl"] = aie2ps::shim_dma_s2mm_0_ctrl;
   regNameToValue["shim_dma_s2mm_0_task_queue"] = aie2ps::shim_dma_s2mm_0_task_queue;
   regNameToValue["shim_dma_s2mm_1_ctrl"] = aie2ps::shim_dma_s2mm_1_ctrl;
   regNameToValue["shim_dma_s2mm_1_task_queue"] = aie2ps::shim_dma_s2mm_1_task_queue;
   regNameToValue["shim_dma_mm2s_0_ctrl"] = aie2ps::shim_dma_mm2s_0_ctrl;
   regNameToValue["shim_dma_mm2s_0_task_queue"] = aie2ps::shim_dma_mm2s_0_task_queue;
   regNameToValue["shim_dma_mm2s_1_ctrl"] = aie2ps::shim_dma_mm2s_1_ctrl;
   regNameToValue["shim_dma_mm2s_1_task_queue"] = aie2ps::shim_dma_mm2s_1_task_queue;
   regNameToValue["shim_dma_s2mm_status_0"] = aie2ps::shim_dma_s2mm_status_0;
   regNameToValue["shim_dma_s2mm_status_1"] = aie2ps::shim_dma_s2mm_status_1;
   regNameToValue["shim_dma_mm2s_status_0"] = aie2ps::shim_dma_mm2s_status_0;
   regNameToValue["shim_dma_mm2s_status_1"] = aie2ps::shim_dma_mm2s_status_1;
   regNameToValue["shim_dma_s2mm_current_write_count_0"] = aie2ps::shim_dma_s2mm_current_write_count_0;
   regNameToValue["shim_dma_s2mm_current_write_count_1"] = aie2ps::shim_dma_s2mm_current_write_count_1;
   regNameToValue["shim_dma_s2mm_fot_count_fifo_pop_0"] = aie2ps::shim_dma_s2mm_fot_count_fifo_pop_0;
   regNameToValue["shim_dma_s2mm_fot_count_fifo_pop_1"] = aie2ps::shim_dma_s2mm_fot_count_fifo_pop_1;
   regNameToValue["shim_dma_mm2s_0_response_fifo_parity_error_injection"] = aie2ps::shim_dma_mm2s_0_response_fifo_parity_error_injection;
   regNameToValue["shim_dma_mm2s_1_response_fifo_parity_error_injection"] = aie2ps::shim_dma_mm2s_1_response_fifo_parity_error_injection;
   regNameToValue["shim_dma_pause"] = aie2ps::shim_dma_pause;
   regNameToValue["shim_lock_request"] = aie2ps::shim_lock_request;
   regNameToValue["shim_pl_interface_upsizer_config"] = aie2ps::shim_pl_interface_upsizer_config;
   regNameToValue["shim_pl_interface_downsizer_config"] = aie2ps::shim_pl_interface_downsizer_config;
   regNameToValue["shim_pl_interface_downsizer_enable"] = aie2ps::shim_pl_interface_downsizer_enable;
   regNameToValue["shim_pl_interface_downsizer_bypass"] = aie2ps::shim_pl_interface_downsizer_bypass;
   regNameToValue["shim_performance_control0"] = aie2ps::shim_performance_control0;
   regNameToValue["shim_performance_start_stop_0_1"] = aie2ps::shim_performance_start_stop_0_1;
   regNameToValue["shim_performance_control1"] = aie2ps::shim_performance_control1;
   regNameToValue["shim_performance_reset_0_1"] = aie2ps::shim_performance_reset_0_1;
   regNameToValue["shim_performance_control2"] = aie2ps::shim_performance_control2;
   regNameToValue["shim_performance_start_stop_2_3"] = aie2ps::shim_performance_start_stop_2_3;
   regNameToValue["shim_performance_control3"] = aie2ps::shim_performance_control3;
   regNameToValue["shim_performance_reset_2_3"] = aie2ps::shim_performance_reset_2_3;
   regNameToValue["shim_performance_control4"] = aie2ps::shim_performance_control4;
   regNameToValue["shim_performance_start_stop_4_5"] = aie2ps::shim_performance_start_stop_4_5;
   regNameToValue["shim_performance_control5"] = aie2ps::shim_performance_control5;
   regNameToValue["shim_performance_reset_4_5"] = aie2ps::shim_performance_reset_4_5;
   regNameToValue["shim_performance_counter0"] = aie2ps::shim_performance_counter0;
   regNameToValue["shim_performance_counter1"] = aie2ps::shim_performance_counter1;
   regNameToValue["shim_performance_counter2"] = aie2ps::shim_performance_counter2;
   regNameToValue["shim_performance_counter3"] = aie2ps::shim_performance_counter3;
   regNameToValue["shim_performance_counter4"] = aie2ps::shim_performance_counter4;
   regNameToValue["shim_performance_counter5"] = aie2ps::shim_performance_counter5;
   regNameToValue["shim_performance_counter0_event_value"] = aie2ps::shim_performance_counter0_event_value;
   regNameToValue["shim_performance_counter1_event_value"] = aie2ps::shim_performance_counter1_event_value;
   regNameToValue["shim_timer_control"] = aie2ps::shim_timer_control;
   regNameToValue["shim_event_generate"] = aie2ps::shim_event_generate;
   regNameToValue["shim_event_broadcast_a_0"] = aie2ps::shim_event_broadcast_a_0;
   regNameToValue["shim_event_broadcast_a_1"] = aie2ps::shim_event_broadcast_a_1;
   regNameToValue["shim_event_broadcast_a_2"] = aie2ps::shim_event_broadcast_a_2;
   regNameToValue["shim_event_broadcast_a_3"] = aie2ps::shim_event_broadcast_a_3;
   regNameToValue["shim_event_broadcast_a_4"] = aie2ps::shim_event_broadcast_a_4;
   regNameToValue["shim_event_broadcast_a_5"] = aie2ps::shim_event_broadcast_a_5;
   regNameToValue["shim_event_broadcast_a_6"] = aie2ps::shim_event_broadcast_a_6;
   regNameToValue["shim_event_broadcast_a_7"] = aie2ps::shim_event_broadcast_a_7;
   regNameToValue["shim_event_broadcast_a_8"] = aie2ps::shim_event_broadcast_a_8;
   regNameToValue["shim_event_broadcast_a_9"] = aie2ps::shim_event_broadcast_a_9;
   regNameToValue["shim_event_broadcast_a_10"] = aie2ps::shim_event_broadcast_a_10;
   regNameToValue["shim_event_broadcast_a_11"] = aie2ps::shim_event_broadcast_a_11;
   regNameToValue["shim_event_broadcast_a_12"] = aie2ps::shim_event_broadcast_a_12;
   regNameToValue["shim_event_broadcast_a_13"] = aie2ps::shim_event_broadcast_a_13;
   regNameToValue["shim_event_broadcast_a_14"] = aie2ps::shim_event_broadcast_a_14;
   regNameToValue["shim_event_broadcast_a_15"] = aie2ps::shim_event_broadcast_a_15;
   regNameToValue["shim_event_broadcast_a_block_south_set"] = aie2ps::shim_event_broadcast_a_block_south_set;
   regNameToValue["shim_event_broadcast_a_block_south_clr"] = aie2ps::shim_event_broadcast_a_block_south_clr;
   regNameToValue["shim_event_broadcast_a_block_south_value"] = aie2ps::shim_event_broadcast_a_block_south_value;
   regNameToValue["shim_event_broadcast_a_block_west_set"] = aie2ps::shim_event_broadcast_a_block_west_set;
   regNameToValue["shim_event_broadcast_a_block_west_clr"] = aie2ps::shim_event_broadcast_a_block_west_clr;
   regNameToValue["shim_event_broadcast_a_block_west_value"] = aie2ps::shim_event_broadcast_a_block_west_value;
   regNameToValue["shim_event_broadcast_a_block_north_set"] = aie2ps::shim_event_broadcast_a_block_north_set;
   regNameToValue["shim_event_broadcast_a_block_north_clr"] = aie2ps::shim_event_broadcast_a_block_north_clr;
   regNameToValue["shim_event_broadcast_a_block_north_value"] = aie2ps::shim_event_broadcast_a_block_north_value;
   regNameToValue["shim_event_broadcast_a_block_east_set"] = aie2ps::shim_event_broadcast_a_block_east_set;
   regNameToValue["shim_event_broadcast_a_block_east_clr"] = aie2ps::shim_event_broadcast_a_block_east_clr;
   regNameToValue["shim_event_broadcast_a_block_east_value"] = aie2ps::shim_event_broadcast_a_block_east_value;
   regNameToValue["shim_event_broadcast_b_block_south_set"] = aie2ps::shim_event_broadcast_b_block_south_set;
   regNameToValue["shim_event_broadcast_b_block_south_clr"] = aie2ps::shim_event_broadcast_b_block_south_clr;
   regNameToValue["shim_event_broadcast_b_block_south_value"] = aie2ps::shim_event_broadcast_b_block_south_value;
   regNameToValue["shim_event_broadcast_b_block_west_set"] = aie2ps::shim_event_broadcast_b_block_west_set;
   regNameToValue["shim_event_broadcast_b_block_west_clr"] = aie2ps::shim_event_broadcast_b_block_west_clr;
   regNameToValue["shim_event_broadcast_b_block_west_value"] = aie2ps::shim_event_broadcast_b_block_west_value;
   regNameToValue["shim_event_broadcast_b_block_north_set"] = aie2ps::shim_event_broadcast_b_block_north_set;
   regNameToValue["shim_event_broadcast_b_block_north_clr"] = aie2ps::shim_event_broadcast_b_block_north_clr;
   regNameToValue["shim_event_broadcast_b_block_north_value"] = aie2ps::shim_event_broadcast_b_block_north_value;
   regNameToValue["shim_event_broadcast_b_block_east_set"] = aie2ps::shim_event_broadcast_b_block_east_set;
   regNameToValue["shim_event_broadcast_b_block_east_clr"] = aie2ps::shim_event_broadcast_b_block_east_clr;
   regNameToValue["shim_event_broadcast_b_block_east_value"] = aie2ps::shim_event_broadcast_b_block_east_value;
   regNameToValue["shim_trace_control0"] = aie2ps::shim_trace_control0;
   regNameToValue["shim_trace_control1"] = aie2ps::shim_trace_control1;
   regNameToValue["shim_trace_status"] = aie2ps::shim_trace_status;
   regNameToValue["shim_trace_event0"] = aie2ps::shim_trace_event0;
   regNameToValue["shim_trace_event1"] = aie2ps::shim_trace_event1;
   regNameToValue["shim_timer_trig_event_low_value"] = aie2ps::shim_timer_trig_event_low_value;
   regNameToValue["shim_timer_trig_event_high_value"] = aie2ps::shim_timer_trig_event_high_value;
   regNameToValue["shim_timer_low"] = aie2ps::shim_timer_low;
   regNameToValue["shim_timer_high"] = aie2ps::shim_timer_high;
   regNameToValue["shim_event_status0"] = aie2ps::shim_event_status0;
   regNameToValue["shim_event_status1"] = aie2ps::shim_event_status1;
   regNameToValue["shim_event_status2"] = aie2ps::shim_event_status2;
   regNameToValue["shim_event_status3"] = aie2ps::shim_event_status3;
   regNameToValue["shim_event_status4"] = aie2ps::shim_event_status4;
   regNameToValue["shim_event_status5"] = aie2ps::shim_event_status5;
   regNameToValue["shim_event_status6"] = aie2ps::shim_event_status6;
   regNameToValue["shim_event_status7"] = aie2ps::shim_event_status7;
   regNameToValue["shim_combo_event_inputs"] = aie2ps::shim_combo_event_inputs;
   regNameToValue["shim_combo_event_control"] = aie2ps::shim_combo_event_control;
   regNameToValue["shim_edge_detection_event_control"] = aie2ps::shim_edge_detection_event_control;
   regNameToValue["shim_event_group_0_enable"] = aie2ps::shim_event_group_0_enable;
   regNameToValue["shim_event_group_dma_enable"] = aie2ps::shim_event_group_dma_enable;
   regNameToValue["shim_event_group_noc_1_dma_activity_enable"] = aie2ps::shim_event_group_noc_1_dma_activity_enable;
   regNameToValue["shim_event_group_noc_0_lock_enable"] = aie2ps::shim_event_group_noc_0_lock_enable;
   regNameToValue["shim_event_group_noc_1_lock_enable"] = aie2ps::shim_event_group_noc_1_lock_enable;
   regNameToValue["shim_event_group_errors_enable"] = aie2ps::shim_event_group_errors_enable;
   regNameToValue["shim_event_group_stream_switch_enable"] = aie2ps::shim_event_group_stream_switch_enable;
   regNameToValue["shim_event_group_broadcast_a_enable"] = aie2ps::shim_event_group_broadcast_a_enable;
   regNameToValue["shim_event_group_uc_dma_activity_enable"] = aie2ps::shim_event_group_uc_dma_activity_enable;
   regNameToValue["shim_event_group_uc_module_errors_enable"] = aie2ps::shim_event_group_uc_module_errors_enable;
   regNameToValue["shim_event_group_uc_core_streams_enable"] = aie2ps::shim_event_group_uc_core_streams_enable;
   regNameToValue["shim_event_group_uc_core_program_flow_enable"] = aie2ps::shim_event_group_uc_core_program_flow_enable;
   regNameToValue["shim_uc_core_interrupt_event"] = aie2ps::shim_uc_core_interrupt_event;
   regNameToValue["shim_interrupt_controller_1st_level_mask_a"] = aie2ps::shim_interrupt_controller_1st_level_mask_a;
   regNameToValue["shim_interrupt_controller_1st_level_enable_a"] = aie2ps::shim_interrupt_controller_1st_level_enable_a;
   regNameToValue["shim_interrupt_controller_1st_level_disable_a"] = aie2ps::shim_interrupt_controller_1st_level_disable_a;
   regNameToValue["shim_interrupt_controller_1st_level_status_a"] = aie2ps::shim_interrupt_controller_1st_level_status_a;
   regNameToValue["shim_interrupt_controller_1st_level_irq_no_a"] = aie2ps::shim_interrupt_controller_1st_level_irq_no_a;
   regNameToValue["shim_interrupt_controller_1st_level_irq_event_a"] = aie2ps::shim_interrupt_controller_1st_level_irq_event_a;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_a_set"] = aie2ps::shim_interrupt_controller_1st_level_block_north_in_a_set;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_a_clear"] = aie2ps::shim_interrupt_controller_1st_level_block_north_in_a_clear;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_a_value"] = aie2ps::shim_interrupt_controller_1st_level_block_north_in_a_value;
   regNameToValue["shim_interrupt_controller_1st_level_mask_b"] = aie2ps::shim_interrupt_controller_1st_level_mask_b;
   regNameToValue["shim_interrupt_controller_1st_level_enable_b"] = aie2ps::shim_interrupt_controller_1st_level_enable_b;
   regNameToValue["shim_interrupt_controller_1st_level_disable_b"] = aie2ps::shim_interrupt_controller_1st_level_disable_b;
   regNameToValue["shim_interrupt_controller_1st_level_status_b"] = aie2ps::shim_interrupt_controller_1st_level_status_b;
   regNameToValue["shim_interrupt_controller_1st_level_irq_no_b"] = aie2ps::shim_interrupt_controller_1st_level_irq_no_b;
   regNameToValue["shim_interrupt_controller_1st_level_irq_event_b"] = aie2ps::shim_interrupt_controller_1st_level_irq_event_b;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_b_set"] = aie2ps::shim_interrupt_controller_1st_level_block_north_in_b_set;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_b_clear"] = aie2ps::shim_interrupt_controller_1st_level_block_north_in_b_clear;
   regNameToValue["shim_interrupt_controller_1st_level_block_north_in_b_value"] = aie2ps::shim_interrupt_controller_1st_level_block_north_in_b_value;
   regNameToValue["shim_bisr_cache_ctrl"] = aie2ps::shim_bisr_cache_ctrl;
   regNameToValue["shim_bisr_cache_status"] = aie2ps::shim_bisr_cache_status;
   regNameToValue["shim_bisr_cache_data0"] = aie2ps::shim_bisr_cache_data0;
   regNameToValue["shim_bisr_cache_data1"] = aie2ps::shim_bisr_cache_data1;
   regNameToValue["shim_bisr_cache_data2"] = aie2ps::shim_bisr_cache_data2;
   regNameToValue["shim_bisr_cache_data3"] = aie2ps::shim_bisr_cache_data3;
   regNameToValue["shim_bisr_cache_data4"] = aie2ps::shim_bisr_cache_data4;
   regNameToValue["shim_bisr_cache_data5"] = aie2ps::shim_bisr_cache_data5;
   regNameToValue["shim_bisr_cache_data6"] = aie2ps::shim_bisr_cache_data6;
   regNameToValue["shim_bisr_cache_data7"] = aie2ps::shim_bisr_cache_data7;
   regNameToValue["shim_bisr_test_data0"] = aie2ps::shim_bisr_test_data0;
   regNameToValue["shim_bisr_test_data1"] = aie2ps::shim_bisr_test_data1;
   regNameToValue["shim_bisr_test_data2"] = aie2ps::shim_bisr_test_data2;
   regNameToValue["shim_bisr_test_data3"] = aie2ps::shim_bisr_test_data3;
   regNameToValue["shim_bisr_test_data4"] = aie2ps::shim_bisr_test_data4;
   regNameToValue["shim_bisr_test_data5"] = aie2ps::shim_bisr_test_data5;
   regNameToValue["shim_bisr_test_data6"] = aie2ps::shim_bisr_test_data6;
   regNameToValue["shim_bisr_test_data7"] = aie2ps::shim_bisr_test_data7;
   regNameToValue["shim_stream_switch_master_config_tile_ctrl"] = aie2ps::shim_stream_switch_master_config_tile_ctrl;
   regNameToValue["shim_stream_switch_master_config_fifo0"] = aie2ps::shim_stream_switch_master_config_fifo0;
   regNameToValue["shim_stream_switch_master_config_south0"] = aie2ps::shim_stream_switch_master_config_south0;
   regNameToValue["shim_stream_switch_master_config_south1"] = aie2ps::shim_stream_switch_master_config_south1;
   regNameToValue["shim_stream_switch_master_config_south2"] = aie2ps::shim_stream_switch_master_config_south2;
   regNameToValue["shim_stream_switch_master_config_south3"] = aie2ps::shim_stream_switch_master_config_south3;
   regNameToValue["shim_stream_switch_master_config_south4"] = aie2ps::shim_stream_switch_master_config_south4;
   regNameToValue["shim_stream_switch_master_config_south5"] = aie2ps::shim_stream_switch_master_config_south5;
   regNameToValue["shim_stream_switch_master_config_west0"] = aie2ps::shim_stream_switch_master_config_west0;
   regNameToValue["shim_stream_switch_master_config_west1"] = aie2ps::shim_stream_switch_master_config_west1;
   regNameToValue["shim_stream_switch_master_config_west2"] = aie2ps::shim_stream_switch_master_config_west2;
   regNameToValue["shim_stream_switch_master_config_west3"] = aie2ps::shim_stream_switch_master_config_west3;
   regNameToValue["shim_stream_switch_master_config_north0"] = aie2ps::shim_stream_switch_master_config_north0;
   regNameToValue["shim_stream_switch_master_config_north1"] = aie2ps::shim_stream_switch_master_config_north1;
   regNameToValue["shim_stream_switch_master_config_north2"] = aie2ps::shim_stream_switch_master_config_north2;
   regNameToValue["shim_stream_switch_master_config_north3"] = aie2ps::shim_stream_switch_master_config_north3;
   regNameToValue["shim_stream_switch_master_config_north4"] = aie2ps::shim_stream_switch_master_config_north4;
   regNameToValue["shim_stream_switch_master_config_north5"] = aie2ps::shim_stream_switch_master_config_north5;
   regNameToValue["shim_stream_switch_master_config_east0"] = aie2ps::shim_stream_switch_master_config_east0;
   regNameToValue["shim_stream_switch_master_config_east1"] = aie2ps::shim_stream_switch_master_config_east1;
   regNameToValue["shim_stream_switch_master_config_east2"] = aie2ps::shim_stream_switch_master_config_east2;
   regNameToValue["shim_stream_switch_master_config_east3"] = aie2ps::shim_stream_switch_master_config_east3;
   regNameToValue["shim_stream_switch_master_config_ucontroller"] = aie2ps::shim_stream_switch_master_config_ucontroller;
   regNameToValue["shim_stream_switch_slave_config_tile_ctrl"] = aie2ps::shim_stream_switch_slave_config_tile_ctrl;
   regNameToValue["shim_stream_switch_slave_config_fifo_0"] = aie2ps::shim_stream_switch_slave_config_fifo_0;
   regNameToValue["shim_stream_switch_slave_config_south_0"] = aie2ps::shim_stream_switch_slave_config_south_0;
   regNameToValue["shim_stream_switch_slave_config_south_1"] = aie2ps::shim_stream_switch_slave_config_south_1;
   regNameToValue["shim_stream_switch_slave_config_south_2"] = aie2ps::shim_stream_switch_slave_config_south_2;
   regNameToValue["shim_stream_switch_slave_config_south_3"] = aie2ps::shim_stream_switch_slave_config_south_3;
   regNameToValue["shim_stream_switch_slave_config_south_4"] = aie2ps::shim_stream_switch_slave_config_south_4;
   regNameToValue["shim_stream_switch_slave_config_south_5"] = aie2ps::shim_stream_switch_slave_config_south_5;
   regNameToValue["shim_stream_switch_slave_config_south_6"] = aie2ps::shim_stream_switch_slave_config_south_6;
   regNameToValue["shim_stream_switch_slave_config_south_7"] = aie2ps::shim_stream_switch_slave_config_south_7;
   regNameToValue["shim_stream_switch_slave_config_west_0"] = aie2ps::shim_stream_switch_slave_config_west_0;
   regNameToValue["shim_stream_switch_slave_config_west_1"] = aie2ps::shim_stream_switch_slave_config_west_1;
   regNameToValue["shim_stream_switch_slave_config_west_2"] = aie2ps::shim_stream_switch_slave_config_west_2;
   regNameToValue["shim_stream_switch_slave_config_west_3"] = aie2ps::shim_stream_switch_slave_config_west_3;
   regNameToValue["shim_stream_switch_slave_config_north_0"] = aie2ps::shim_stream_switch_slave_config_north_0;
   regNameToValue["shim_stream_switch_slave_config_north_1"] = aie2ps::shim_stream_switch_slave_config_north_1;
   regNameToValue["shim_stream_switch_slave_config_north_2"] = aie2ps::shim_stream_switch_slave_config_north_2;
   regNameToValue["shim_stream_switch_slave_config_north_3"] = aie2ps::shim_stream_switch_slave_config_north_3;
   regNameToValue["shim_stream_switch_slave_config_east_0"] = aie2ps::shim_stream_switch_slave_config_east_0;
   regNameToValue["shim_stream_switch_slave_config_east_1"] = aie2ps::shim_stream_switch_slave_config_east_1;
   regNameToValue["shim_stream_switch_slave_config_east_2"] = aie2ps::shim_stream_switch_slave_config_east_2;
   regNameToValue["shim_stream_switch_slave_config_east_3"] = aie2ps::shim_stream_switch_slave_config_east_3;
   regNameToValue["shim_stream_switch_slave_config_trace"] = aie2ps::shim_stream_switch_slave_config_trace;
   regNameToValue["shim_stream_switch_slave_config_ucontroller"] = aie2ps::shim_stream_switch_slave_config_ucontroller;
   regNameToValue["shim_stream_switch_slave_tile_ctrl_slot0"] = aie2ps::shim_stream_switch_slave_tile_ctrl_slot0;
   regNameToValue["shim_stream_switch_slave_tile_ctrl_slot1"] = aie2ps::shim_stream_switch_slave_tile_ctrl_slot1;
   regNameToValue["shim_stream_switch_slave_tile_ctrl_slot2"] = aie2ps::shim_stream_switch_slave_tile_ctrl_slot2;
   regNameToValue["shim_stream_switch_slave_tile_ctrl_slot3"] = aie2ps::shim_stream_switch_slave_tile_ctrl_slot3;
   regNameToValue["shim_stream_switch_slave_fifo_0_slot0"] = aie2ps::shim_stream_switch_slave_fifo_0_slot0;
   regNameToValue["shim_stream_switch_slave_fifo_0_slot1"] = aie2ps::shim_stream_switch_slave_fifo_0_slot1;
   regNameToValue["shim_stream_switch_slave_fifo_0_slot2"] = aie2ps::shim_stream_switch_slave_fifo_0_slot2;
   regNameToValue["shim_stream_switch_slave_fifo_0_slot3"] = aie2ps::shim_stream_switch_slave_fifo_0_slot3;
   regNameToValue["shim_stream_switch_slave_south_0_slot0"] = aie2ps::shim_stream_switch_slave_south_0_slot0;
   regNameToValue["shim_stream_switch_slave_south_0_slot1"] = aie2ps::shim_stream_switch_slave_south_0_slot1;
   regNameToValue["shim_stream_switch_slave_south_0_slot2"] = aie2ps::shim_stream_switch_slave_south_0_slot2;
   regNameToValue["shim_stream_switch_slave_south_0_slot3"] = aie2ps::shim_stream_switch_slave_south_0_slot3;
   regNameToValue["shim_stream_switch_slave_south_1_slot0"] = aie2ps::shim_stream_switch_slave_south_1_slot0;
   regNameToValue["shim_stream_switch_slave_south_1_slot1"] = aie2ps::shim_stream_switch_slave_south_1_slot1;
   regNameToValue["shim_stream_switch_slave_south_1_slot2"] = aie2ps::shim_stream_switch_slave_south_1_slot2;
   regNameToValue["shim_stream_switch_slave_south_1_slot3"] = aie2ps::shim_stream_switch_slave_south_1_slot3;
   regNameToValue["shim_stream_switch_slave_south_2_slot0"] = aie2ps::shim_stream_switch_slave_south_2_slot0;
   regNameToValue["shim_stream_switch_slave_south_2_slot1"] = aie2ps::shim_stream_switch_slave_south_2_slot1;
   regNameToValue["shim_stream_switch_slave_south_2_slot2"] = aie2ps::shim_stream_switch_slave_south_2_slot2;
   regNameToValue["shim_stream_switch_slave_south_2_slot3"] = aie2ps::shim_stream_switch_slave_south_2_slot3;
   regNameToValue["shim_stream_switch_slave_south_3_slot0"] = aie2ps::shim_stream_switch_slave_south_3_slot0;
   regNameToValue["shim_stream_switch_slave_south_3_slot1"] = aie2ps::shim_stream_switch_slave_south_3_slot1;
   regNameToValue["shim_stream_switch_slave_south_3_slot2"] = aie2ps::shim_stream_switch_slave_south_3_slot2;
   regNameToValue["shim_stream_switch_slave_south_3_slot3"] = aie2ps::shim_stream_switch_slave_south_3_slot3;
   regNameToValue["shim_stream_switch_slave_south_4_slot0"] = aie2ps::shim_stream_switch_slave_south_4_slot0;
   regNameToValue["shim_stream_switch_slave_south_4_slot1"] = aie2ps::shim_stream_switch_slave_south_4_slot1;
   regNameToValue["shim_stream_switch_slave_south_4_slot2"] = aie2ps::shim_stream_switch_slave_south_4_slot2;
   regNameToValue["shim_stream_switch_slave_south_4_slot3"] = aie2ps::shim_stream_switch_slave_south_4_slot3;
   regNameToValue["shim_stream_switch_slave_south_5_slot0"] = aie2ps::shim_stream_switch_slave_south_5_slot0;
   regNameToValue["shim_stream_switch_slave_south_5_slot1"] = aie2ps::shim_stream_switch_slave_south_5_slot1;
   regNameToValue["shim_stream_switch_slave_south_5_slot2"] = aie2ps::shim_stream_switch_slave_south_5_slot2;
   regNameToValue["shim_stream_switch_slave_south_5_slot3"] = aie2ps::shim_stream_switch_slave_south_5_slot3;
   regNameToValue["shim_stream_switch_slave_south_6_slot0"] = aie2ps::shim_stream_switch_slave_south_6_slot0;
   regNameToValue["shim_stream_switch_slave_south_6_slot1"] = aie2ps::shim_stream_switch_slave_south_6_slot1;
   regNameToValue["shim_stream_switch_slave_south_6_slot2"] = aie2ps::shim_stream_switch_slave_south_6_slot2;
   regNameToValue["shim_stream_switch_slave_south_6_slot3"] = aie2ps::shim_stream_switch_slave_south_6_slot3;
   regNameToValue["shim_stream_switch_slave_south_7_slot0"] = aie2ps::shim_stream_switch_slave_south_7_slot0;
   regNameToValue["shim_stream_switch_slave_south_7_slot1"] = aie2ps::shim_stream_switch_slave_south_7_slot1;
   regNameToValue["shim_stream_switch_slave_south_7_slot2"] = aie2ps::shim_stream_switch_slave_south_7_slot2;
   regNameToValue["shim_stream_switch_slave_south_7_slot3"] = aie2ps::shim_stream_switch_slave_south_7_slot3;
   regNameToValue["shim_stream_switch_slave_west_0_slot0"] = aie2ps::shim_stream_switch_slave_west_0_slot0;
   regNameToValue["shim_stream_switch_slave_west_0_slot1"] = aie2ps::shim_stream_switch_slave_west_0_slot1;
   regNameToValue["shim_stream_switch_slave_west_0_slot2"] = aie2ps::shim_stream_switch_slave_west_0_slot2;
   regNameToValue["shim_stream_switch_slave_west_0_slot3"] = aie2ps::shim_stream_switch_slave_west_0_slot3;
   regNameToValue["shim_stream_switch_slave_west_1_slot0"] = aie2ps::shim_stream_switch_slave_west_1_slot0;
   regNameToValue["shim_stream_switch_slave_west_1_slot1"] = aie2ps::shim_stream_switch_slave_west_1_slot1;
   regNameToValue["shim_stream_switch_slave_west_1_slot2"] = aie2ps::shim_stream_switch_slave_west_1_slot2;
   regNameToValue["shim_stream_switch_slave_west_1_slot3"] = aie2ps::shim_stream_switch_slave_west_1_slot3;
   regNameToValue["shim_stream_switch_slave_west_2_slot0"] = aie2ps::shim_stream_switch_slave_west_2_slot0;
   regNameToValue["shim_stream_switch_slave_west_2_slot1"] = aie2ps::shim_stream_switch_slave_west_2_slot1;
   regNameToValue["shim_stream_switch_slave_west_2_slot2"] = aie2ps::shim_stream_switch_slave_west_2_slot2;
   regNameToValue["shim_stream_switch_slave_west_2_slot3"] = aie2ps::shim_stream_switch_slave_west_2_slot3;
   regNameToValue["shim_stream_switch_slave_west_3_slot0"] = aie2ps::shim_stream_switch_slave_west_3_slot0;
   regNameToValue["shim_stream_switch_slave_west_3_slot1"] = aie2ps::shim_stream_switch_slave_west_3_slot1;
   regNameToValue["shim_stream_switch_slave_west_3_slot2"] = aie2ps::shim_stream_switch_slave_west_3_slot2;
   regNameToValue["shim_stream_switch_slave_west_3_slot3"] = aie2ps::shim_stream_switch_slave_west_3_slot3;
   regNameToValue["shim_stream_switch_slave_north_0_slot0"] = aie2ps::shim_stream_switch_slave_north_0_slot0;
   regNameToValue["shim_stream_switch_slave_north_0_slot1"] = aie2ps::shim_stream_switch_slave_north_0_slot1;
   regNameToValue["shim_stream_switch_slave_north_0_slot2"] = aie2ps::shim_stream_switch_slave_north_0_slot2;
   regNameToValue["shim_stream_switch_slave_north_0_slot3"] = aie2ps::shim_stream_switch_slave_north_0_slot3;
   regNameToValue["shim_stream_switch_slave_north_1_slot0"] = aie2ps::shim_stream_switch_slave_north_1_slot0;
   regNameToValue["shim_stream_switch_slave_north_1_slot1"] = aie2ps::shim_stream_switch_slave_north_1_slot1;
   regNameToValue["shim_stream_switch_slave_north_1_slot2"] = aie2ps::shim_stream_switch_slave_north_1_slot2;
   regNameToValue["shim_stream_switch_slave_north_1_slot3"] = aie2ps::shim_stream_switch_slave_north_1_slot3;
   regNameToValue["shim_stream_switch_slave_north_2_slot0"] = aie2ps::shim_stream_switch_slave_north_2_slot0;
   regNameToValue["shim_stream_switch_slave_north_2_slot1"] = aie2ps::shim_stream_switch_slave_north_2_slot1;
   regNameToValue["shim_stream_switch_slave_north_2_slot2"] = aie2ps::shim_stream_switch_slave_north_2_slot2;
   regNameToValue["shim_stream_switch_slave_north_2_slot3"] = aie2ps::shim_stream_switch_slave_north_2_slot3;
   regNameToValue["shim_stream_switch_slave_north_3_slot0"] = aie2ps::shim_stream_switch_slave_north_3_slot0;
   regNameToValue["shim_stream_switch_slave_north_3_slot1"] = aie2ps::shim_stream_switch_slave_north_3_slot1;
   regNameToValue["shim_stream_switch_slave_north_3_slot2"] = aie2ps::shim_stream_switch_slave_north_3_slot2;
   regNameToValue["shim_stream_switch_slave_north_3_slot3"] = aie2ps::shim_stream_switch_slave_north_3_slot3;
   regNameToValue["shim_stream_switch_slave_east_0_slot0"] = aie2ps::shim_stream_switch_slave_east_0_slot0;
   regNameToValue["shim_stream_switch_slave_east_0_slot1"] = aie2ps::shim_stream_switch_slave_east_0_slot1;
   regNameToValue["shim_stream_switch_slave_east_0_slot2"] = aie2ps::shim_stream_switch_slave_east_0_slot2;
   regNameToValue["shim_stream_switch_slave_east_0_slot3"] = aie2ps::shim_stream_switch_slave_east_0_slot3;
   regNameToValue["shim_stream_switch_slave_east_1_slot0"] = aie2ps::shim_stream_switch_slave_east_1_slot0;
   regNameToValue["shim_stream_switch_slave_east_1_slot1"] = aie2ps::shim_stream_switch_slave_east_1_slot1;
   regNameToValue["shim_stream_switch_slave_east_1_slot2"] = aie2ps::shim_stream_switch_slave_east_1_slot2;
   regNameToValue["shim_stream_switch_slave_east_1_slot3"] = aie2ps::shim_stream_switch_slave_east_1_slot3;
   regNameToValue["shim_stream_switch_slave_east_2_slot0"] = aie2ps::shim_stream_switch_slave_east_2_slot0;
   regNameToValue["shim_stream_switch_slave_east_2_slot1"] = aie2ps::shim_stream_switch_slave_east_2_slot1;
   regNameToValue["shim_stream_switch_slave_east_2_slot2"] = aie2ps::shim_stream_switch_slave_east_2_slot2;
   regNameToValue["shim_stream_switch_slave_east_2_slot3"] = aie2ps::shim_stream_switch_slave_east_2_slot3;
   regNameToValue["shim_stream_switch_slave_east_3_slot0"] = aie2ps::shim_stream_switch_slave_east_3_slot0;
   regNameToValue["shim_stream_switch_slave_east_3_slot1"] = aie2ps::shim_stream_switch_slave_east_3_slot1;
   regNameToValue["shim_stream_switch_slave_east_3_slot2"] = aie2ps::shim_stream_switch_slave_east_3_slot2;
   regNameToValue["shim_stream_switch_slave_east_3_slot3"] = aie2ps::shim_stream_switch_slave_east_3_slot3;
   regNameToValue["shim_stream_switch_slave_trace_slot0"] = aie2ps::shim_stream_switch_slave_trace_slot0;
   regNameToValue["shim_stream_switch_slave_trace_slot1"] = aie2ps::shim_stream_switch_slave_trace_slot1;
   regNameToValue["shim_stream_switch_slave_trace_slot2"] = aie2ps::shim_stream_switch_slave_trace_slot2;
   regNameToValue["shim_stream_switch_slave_trace_slot3"] = aie2ps::shim_stream_switch_slave_trace_slot3;
   regNameToValue["shim_stream_switch_slave_ucontroller_slot0"] = aie2ps::shim_stream_switch_slave_ucontroller_slot0;
   regNameToValue["shim_stream_switch_slave_ucontroller_slot1"] = aie2ps::shim_stream_switch_slave_ucontroller_slot1;
   regNameToValue["shim_stream_switch_slave_ucontroller_slot2"] = aie2ps::shim_stream_switch_slave_ucontroller_slot2;
   regNameToValue["shim_stream_switch_slave_ucontroller_slot3"] = aie2ps::shim_stream_switch_slave_ucontroller_slot3;
   regNameToValue["shim_stream_switch_deterministic_merge_arb0_slave0_1"] = aie2ps::shim_stream_switch_deterministic_merge_arb0_slave0_1;
   regNameToValue["shim_stream_switch_deterministic_merge_arb0_slave2_3"] = aie2ps::shim_stream_switch_deterministic_merge_arb0_slave2_3;
   regNameToValue["shim_stream_switch_deterministic_merge_arb0_ctrl"] = aie2ps::shim_stream_switch_deterministic_merge_arb0_ctrl;
   regNameToValue["shim_stream_switch_deterministic_merge_arb1_slave0_1"] = aie2ps::shim_stream_switch_deterministic_merge_arb1_slave0_1;
   regNameToValue["shim_stream_switch_deterministic_merge_arb1_slave2_3"] = aie2ps::shim_stream_switch_deterministic_merge_arb1_slave2_3;
   regNameToValue["shim_stream_switch_deterministic_merge_arb1_ctrl"] = aie2ps::shim_stream_switch_deterministic_merge_arb1_ctrl;
   regNameToValue["shim_stream_switch_event_port_selection_0"] = aie2ps::shim_stream_switch_event_port_selection_0;
   regNameToValue["shim_stream_switch_event_port_selection_1"] = aie2ps::shim_stream_switch_event_port_selection_1;
   regNameToValue["shim_stream_switch_parity_status"] = aie2ps::shim_stream_switch_parity_status;
   regNameToValue["shim_stream_switch_parity_injection"] = aie2ps::shim_stream_switch_parity_injection;
   regNameToValue["shim_control_packet_handler_status"] = aie2ps::shim_control_packet_handler_status;
   regNameToValue["shim_stream_switch_adaptive_clock_gate_status"] = aie2ps::shim_stream_switch_adaptive_clock_gate_status;
   regNameToValue["shim_stream_switch_adaptive_clock_gate_abort_period"] = aie2ps::shim_stream_switch_adaptive_clock_gate_abort_period;
   regNameToValue["shim_module_clock_control_0"] = aie2ps::shim_module_clock_control_0;
   regNameToValue["shim_module_clock_control_1"] = aie2ps::shim_module_clock_control_1;
   regNameToValue["shim_module_reset_control_0"] = aie2ps::shim_module_reset_control_0;
   regNameToValue["shim_module_reset_control_1"] = aie2ps::shim_module_reset_control_1;
   regNameToValue["shim_column_clock_control"] = aie2ps::shim_column_clock_control;
   regNameToValue["shim_column_reset_control"] = aie2ps::shim_column_reset_control;
   regNameToValue["shim_spare_reg_privileged"] = aie2ps::shim_spare_reg_privileged;
   regNameToValue["shim_spare_reg"] = aie2ps::shim_spare_reg;
   regNameToValue["shim_tile_control"] = aie2ps::shim_tile_control;
   regNameToValue["shim_tile_control_axi_mm"] = aie2ps::shim_tile_control_axi_mm;
   regNameToValue["shim_nmu_switches_config"] = aie2ps::shim_nmu_switches_config;
   regNameToValue["shim_cssd_trigger"] = aie2ps::shim_cssd_trigger;
   regNameToValue["shim_interrupt_controller_hw_error_mask"] = aie2ps::shim_interrupt_controller_hw_error_mask;
   regNameToValue["shim_interrupt_controller_hw_error_status"] = aie2ps::shim_interrupt_controller_hw_error_status;
   regNameToValue["shim_interrupt_controller_hw_error_interrupt"] = aie2ps::shim_interrupt_controller_hw_error_interrupt;
   regNameToValue["uc_core_control"] = aie2ps::uc_core_control;
   regNameToValue["uc_core_interrupt_status"] = aie2ps::uc_core_interrupt_status;
   regNameToValue["uc_core_status"] = aie2ps::uc_core_status;
   regNameToValue["uc_dma_dm2mm_axi_control"] = aie2ps::uc_dma_dm2mm_axi_control;
   regNameToValue["uc_dma_dm2mm_control"] = aie2ps::uc_dma_dm2mm_control;
   regNameToValue["uc_dma_dm2mm_status"] = aie2ps::uc_dma_dm2mm_status;
   regNameToValue["uc_dma_mm2dm_axi_control"] = aie2ps::uc_dma_mm2dm_axi_control;
   regNameToValue["uc_dma_mm2dm_control"] = aie2ps::uc_dma_mm2dm_control;
   regNameToValue["uc_dma_mm2dm_status"] = aie2ps::uc_dma_mm2dm_status;
   regNameToValue["uc_dma_pause"] = aie2ps::uc_dma_pause;
   regNameToValue["uc_mdm_dbg_ctrl_status"] = aie2ps::uc_mdm_dbg_ctrl_status;
   regNameToValue["uc_mdm_dbg_data"] = aie2ps::uc_mdm_dbg_data;
   regNameToValue["uc_mdm_dbg_lock"] = aie2ps::uc_mdm_dbg_lock;
   regNameToValue["uc_mdm_pccmdr"] = aie2ps::uc_mdm_pccmdr;
   regNameToValue["uc_mdm_pcctrlr"] = aie2ps::uc_mdm_pcctrlr;
   regNameToValue["uc_mdm_pcdrr"] = aie2ps::uc_mdm_pcdrr;
   regNameToValue["uc_mdm_pcsr"] = aie2ps::uc_mdm_pcsr;
   regNameToValue["uc_mdm_pcwr"] = aie2ps::uc_mdm_pcwr;
   regNameToValue["uc_memory_dm_ecc_error_generation"] = aie2ps::uc_memory_dm_ecc_error_generation;
   regNameToValue["uc_memory_dm_ecc_scrubbing_period"] = aie2ps::uc_memory_dm_ecc_scrubbing_period;
   regNameToValue["uc_memory_privileged"] = aie2ps::uc_memory_privileged;
   regNameToValue["uc_memory_zeroization"] = aie2ps::uc_memory_zeroization;
   regNameToValue["uc_module_aximm_offset"] = aie2ps::uc_module_aximm_offset;
   regNameToValue["uc_module_axi_mm_outstanding_transactions"] = aie2ps::uc_module_axi_mm_outstanding_transactions;
   regNameToValue["shim_lock_step_size"] = aie2ps::shim_lock_step_size;
   regNameToValue["shim_dma_bd_step_size"] = aie2ps::shim_dma_bd_step_size;
   regNameToValue["shim_dma_s2mm_step_size"] = aie2ps::shim_dma_s2mm_step_size;
   regNameToValue["npi_me_isr"] = aie2ps::npi_me_isr;
   regNameToValue["npi_me_itr"] = aie2ps::npi_me_itr;
   regNameToValue["npi_me_imr0"] = aie2ps::npi_me_imr0;
   regNameToValue["npi_me_ier0"] = aie2ps::npi_me_ier0;
   regNameToValue["npi_me_idr0"] = aie2ps::npi_me_idr0;
   regNameToValue["npi_me_imr1"] = aie2ps::npi_me_imr1;
   regNameToValue["npi_me_ier1"] = aie2ps::npi_me_ier1;
   regNameToValue["npi_me_idr1"] = aie2ps::npi_me_idr1;
   regNameToValue["npi_me_imr2"] = aie2ps::npi_me_imr2;
   regNameToValue["npi_me_ier2"] = aie2ps::npi_me_ier2;
   regNameToValue["npi_me_idr2"] = aie2ps::npi_me_idr2;
   regNameToValue["npi_me_imr3"] = aie2ps::npi_me_imr3;
   regNameToValue["npi_me_ier3"] = aie2ps::npi_me_ier3;
   regNameToValue["npi_me_idr3"] = aie2ps::npi_me_idr3;
   regNameToValue["npi_me_ior"] = aie2ps::npi_me_ior;
   regNameToValue["npi_me_pll_status"] = aie2ps::npi_me_pll_status;
   regNameToValue["npi_me_secure_reg"] = aie2ps::npi_me_secure_reg;
   regNameToValue["uc_base_address"] = aie2ps::uc_base_address;

}

void AIE2psUsedRegisters::populateRegValueToNameMap() {
   // core_module registers
   coreRegValueToName = {
      {0x00030000, "cm_core_bmll0_part1"},
      {0x00030010, "cm_core_bmll0_part2"},
      {0x00030020, "cm_core_bmll0_part3"},
      {0x00030030, "cm_core_bmll0_part4"},
      {0x00030040, "cm_core_bmlh0_part1"},
      {0x00030050, "cm_core_bmlh0_part2"},
      {0x00030060, "cm_core_bmlh0_part3"},
      {0x00030070, "cm_core_bmlh0_part4"},
      {0x00030080, "cm_core_bmhl0_part1"},
      {0x00030090, "cm_core_bmhl0_part2"},
      {0x000300A0, "cm_core_bmhl0_part3"},
      {0x000300B0, "cm_core_bmhl0_part4"},
      {0x000300C0, "cm_core_bmhh0_part1"},
      {0x000300D0, "cm_core_bmhh0_part2"},
      {0x000300E0, "cm_core_bmhh0_part3"},
      {0x000300F0, "cm_core_bmhh0_part4"},
      {0x00030100, "cm_core_bmll1_part1"},
      {0x00030110, "cm_core_bmll1_part2"},
      {0x00030120, "cm_core_bmll1_part3"},
      {0x00030130, "cm_core_bmll1_part4"},
      {0x00030140, "cm_core_bmlh1_part1"},
      {0x00030150, "cm_core_bmlh1_part2"},
      {0x00030160, "cm_core_bmlh1_part3"},
      {0x00030170, "cm_core_bmlh1_part4"},
      {0x00030180, "cm_core_bmhl1_part1"},
      {0x00030190, "cm_core_bmhl1_part2"},
      {0x000301A0, "cm_core_bmhl1_part3"},
      {0x000301B0, "cm_core_bmhl1_part4"},
      {0x000301C0, "cm_core_bmhh1_part1"},
      {0x000301D0, "cm_core_bmhh1_part2"},
      {0x000301E0, "cm_core_bmhh1_part3"},
      {0x000301F0, "cm_core_bmhh1_part4"},
      {0x00030200, "cm_core_bmll2_part1"},
      {0x00030210, "cm_core_bmll2_part2"},
      {0x00030220, "cm_core_bmll2_part3"},
      {0x00030230, "cm_core_bmll2_part4"},
      {0x00030240, "cm_core_bmlh2_part1"},
      {0x00030250, "cm_core_bmlh2_part2"},
      {0x00030260, "cm_core_bmlh2_part3"},
      {0x00030270, "cm_core_bmlh2_part4"},
      {0x00030280, "cm_core_bmhl2_part1"},
      {0x00030290, "cm_core_bmhl2_part2"},
      {0x000302A0, "cm_core_bmhl2_part3"},
      {0x000302B0, "cm_core_bmhl2_part4"},
      {0x000302C0, "cm_core_bmhh2_part1"},
      {0x000302D0, "cm_core_bmhh2_part2"},
      {0x000302E0, "cm_core_bmhh2_part3"},
      {0x000302F0, "cm_core_bmhh2_part4"},
      {0x00030300, "cm_core_bmll3_part1"},
      {0x00030310, "cm_core_bmll3_part2"},
      {0x00030320, "cm_core_bmll3_part3"},
      {0x00030330, "cm_core_bmll3_part4"},
      {0x00030340, "cm_core_bmlh3_part1"},
      {0x00030350, "cm_core_bmlh3_part2"},
      {0x00030360, "cm_core_bmlh3_part3"},
      {0x00030370, "cm_core_bmlh3_part4"},
      {0x00030380, "cm_core_bmhl3_part1"},
      {0x00030390, "cm_core_bmhl3_part2"},
      {0x000303A0, "cm_core_bmhl3_part3"},
      {0x000303B0, "cm_core_bmhl3_part4"},
      {0x000303C0, "cm_core_bmhh3_part1"},
      {0x000303D0, "cm_core_bmhh3_part2"},
      {0x000303E0, "cm_core_bmhh3_part3"},
      {0x000303F0, "cm_core_bmhh3_part4"},
      {0x00030400, "cm_core_bmll4_part1"},
      {0x00030410, "cm_core_bmll4_part2"},
      {0x00030420, "cm_core_bmll4_part3"},
      {0x00030430, "cm_core_bmll4_part4"},
      {0x00030440, "cm_core_bmlh4_part1"},
      {0x00030450, "cm_core_bmlh4_part2"},
      {0x00030460, "cm_core_bmlh4_part3"},
      {0x00030470, "cm_core_bmlh4_part4"},
      {0x00030480, "cm_core_bmhl4_part1"},
      {0x00030490, "cm_core_bmhl4_part2"},
      {0x000304A0, "cm_core_bmhl4_part3"},
      {0x000304B0, "cm_core_bmhl4_part4"},
      {0x000304C0, "cm_core_bmhh4_part1"},
      {0x000304D0, "cm_core_bmhh4_part2"},
      {0x000304E0, "cm_core_bmhh4_part3"},
      {0x000304F0, "cm_core_bmhh4_part4"},
      {0x00030500, "cm_core_bmll5_part1"},
      {0x00030510, "cm_core_bmll5_part2"},
      {0x00030520, "cm_core_bmll5_part3"},
      {0x00030530, "cm_core_bmll5_part4"},
      {0x00030540, "cm_core_bmlh5_part1"},
      {0x00030550, "cm_core_bmlh5_part2"},
      {0x00030560, "cm_core_bmlh5_part3"},
      {0x00030570, "cm_core_bmlh5_part4"},
      {0x00030580, "cm_core_bmhl5_part1"},
      {0x00030590, "cm_core_bmhl5_part2"},
      {0x000305A0, "cm_core_bmhl5_part3"},
      {0x000305B0, "cm_core_bmhl5_part4"},
      {0x000305C0, "cm_core_bmhh5_part1"},
      {0x000305D0, "cm_core_bmhh5_part2"},
      {0x000305E0, "cm_core_bmhh5_part3"},
      {0x000305F0, "cm_core_bmhh5_part4"},
      {0x00030600, "cm_core_bmll6_part1"},
      {0x00030610, "cm_core_bmll6_part2"},
      {0x00030620, "cm_core_bmll6_part3"},
      {0x00030630, "cm_core_bmll6_part4"},
      {0x00030640, "cm_core_bmlh6_part1"},
      {0x00030650, "cm_core_bmlh6_part2"},
      {0x00030660, "cm_core_bmlh6_part3"},
      {0x00030670, "cm_core_bmlh6_part4"},
      {0x00030680, "cm_core_bmhl6_part1"},
      {0x00030690, "cm_core_bmhl6_part2"},
      {0x000306A0, "cm_core_bmhl6_part3"},
      {0x000306B0, "cm_core_bmhl6_part4"},
      {0x000306C0, "cm_core_bmhh6_part1"},
      {0x000306D0, "cm_core_bmhh6_part2"},
      {0x000306E0, "cm_core_bmhh6_part3"},
      {0x000306F0, "cm_core_bmhh6_part4"},
      {0x00030700, "cm_core_bmll7_part1"},
      {0x00030710, "cm_core_bmll7_part2"},
      {0x00030720, "cm_core_bmll7_part3"},
      {0x00030730, "cm_core_bmll7_part4"},
      {0x00030740, "cm_core_bmlh7_part1"},
      {0x00030750, "cm_core_bmlh7_part2"},
      {0x00030760, "cm_core_bmlh7_part3"},
      {0x00030770, "cm_core_bmlh7_part4"},
      {0x00030780, "cm_core_bmhl7_part1"},
      {0x00030790, "cm_core_bmhl7_part2"},
      {0x000307A0, "cm_core_bmhl7_part3"},
      {0x000307B0, "cm_core_bmhl7_part4"},
      {0x000307C0, "cm_core_bmhh7_part1"},
      {0x000307D0, "cm_core_bmhh7_part2"},
      {0x000307E0, "cm_core_bmhh7_part3"},
      {0x000307F0, "cm_core_bmhh7_part4"},
      {0x00031800, "cm_core_x0_part1"},
      {0x00031810, "cm_core_x0_part2"},
      {0x00031820, "cm_core_x0_part3"},
      {0x00031830, "cm_core_x0_part4"},
      {0x00031840, "cm_core_x1_part1"},
      {0x00031850, "cm_core_x1_part2"},
      {0x00031860, "cm_core_x1_part3"},
      {0x00031870, "cm_core_x1_part4"},
      {0x00031880, "cm_core_x2_part1"},
      {0x00031890, "cm_core_x2_part2"},
      {0x000318A0, "cm_core_x2_part3"},
      {0x000318B0, "cm_core_x2_part4"},
      {0x000318C0, "cm_core_x3_part1"},
      {0x000318D0, "cm_core_x3_part2"},
      {0x000318E0, "cm_core_x3_part3"},
      {0x000318F0, "cm_core_x3_part4"},
      {0x00031900, "cm_core_x4_part1"},
      {0x00031910, "cm_core_x4_part2"},
      {0x00031920, "cm_core_x4_part3"},
      {0x00031930, "cm_core_x4_part4"},
      {0x00031940, "cm_core_x5_part1"},
      {0x00031950, "cm_core_x5_part2"},
      {0x00031960, "cm_core_x5_part3"},
      {0x00031970, "cm_core_x5_part4"},
      {0x00031980, "cm_core_x6_part1"},
      {0x00031990, "cm_core_x6_part2"},
      {0x000319A0, "cm_core_x6_part3"},
      {0x000319B0, "cm_core_x6_part4"},
      {0x000319C0, "cm_core_x7_part1"},
      {0x000319D0, "cm_core_x7_part2"},
      {0x000319E0, "cm_core_x7_part3"},
      {0x000319F0, "cm_core_x7_part4"},
      {0x00031A00, "cm_core_x8_part1"},
      {0x00031A10, "cm_core_x8_part2"},
      {0x00031A20, "cm_core_x8_part3"},
      {0x00031A30, "cm_core_x8_part4"},
      {0x00031A40, "cm_core_x9_part1"},
      {0x00031A50, "cm_core_x9_part2"},
      {0x00031A60, "cm_core_x9_part3"},
      {0x00031A70, "cm_core_x9_part4"},
      {0x00031A80, "cm_core_x10_part1"},
      {0x00031A90, "cm_core_x10_part2"},
      {0x00031AA0, "cm_core_x10_part3"},
      {0x00031AB0, "cm_core_x10_part4"},
      {0x00031AC0, "cm_core_x11_part1"},
      {0x00031AD0, "cm_core_x11_part2"},
      {0x00031AE0, "cm_core_x11_part3"},
      {0x00031AF0, "cm_core_x11_part4"},
      {0x00032400, "cm_core_ldfifol0_part1"},
      {0x00032410, "cm_core_ldfifol0_part2"},
      {0x00032420, "cm_core_ldfifol0_part3"},
      {0x00032430, "cm_core_ldfifol0_part4"},
      {0x00032440, "cm_core_ldfifoh0_part1"},
      {0x00032450, "cm_core_ldfifoh0_part2"},
      {0x00032460, "cm_core_ldfifoh0_part3"},
      {0x00032470, "cm_core_ldfifoh0_part4"},
      {0x00032480, "cm_core_ldfifol1_part1"},
      {0x00032490, "cm_core_ldfifol1_part2"},
      {0x000324A0, "cm_core_ldfifol1_part3"},
      {0x000324B0, "cm_core_ldfifol1_part4"},
      {0x000324C0, "cm_core_ldfifoh1_part1"},
      {0x000324D0, "cm_core_ldfifoh1_part2"},
      {0x000324E0, "cm_core_ldfifoh1_part3"},
      {0x000324F0, "cm_core_ldfifoh1_part4"},
      {0x00032500, "cm_core_stfifol_part1"},
      {0x00032510, "cm_core_stfifol_part2"},
      {0x00032520, "cm_core_stfifol_part3"},
      {0x00032530, "cm_core_stfifol_part4"},
      {0x00032540, "cm_core_stfifoh_part1"},
      {0x00032550, "cm_core_stfifoh_part2"},
      {0x00032560, "cm_core_stfifoh_part3"},
      {0x00032570, "cm_core_stfifoh_part4"},
      {0x00032580, "cm_core_fifoxtra_part1"},
      {0x00032590, "cm_core_fifoxtra_part2"},
      {0x000325A0, "cm_core_fifoxtra_part3"},
      {0x000325B0, "cm_core_fifoxtra_part4"},
      {0x00032600, "cm_core_eg0"},
      {0x00032610, "cm_core_eg1"},
      {0x00032620, "cm_core_eg2"},
      {0x00032630, "cm_core_eg3"},
      {0x00032640, "cm_core_eg4"},
      {0x00032650, "cm_core_eg5"},
      {0x00032660, "cm_core_eg6"},
      {0x00032670, "cm_core_eg7"},
      {0x00032680, "cm_core_eg8"},
      {0x00032690, "cm_core_eg9"},
      {0x000326A0, "cm_core_eg10"},
      {0x000326B0, "cm_core_eg11"},
      {0x00032700, "cm_core_f0"},
      {0x00032710, "cm_core_f1"},
      {0x00032720, "cm_core_f2"},
      {0x00032730, "cm_core_f3"},
      {0x00032740, "cm_core_f4"},
      {0x00032750, "cm_core_f5"},
      {0x00032760, "cm_core_f6"},
      {0x00032770, "cm_core_f7"},
      {0x00032780, "cm_core_f8"},
      {0x00032790, "cm_core_f9"},
      {0x000327A0, "cm_core_f10"},
      {0x000327B0, "cm_core_f11"},
      {0x00032800, "cm_core_r0"},
      {0x00032810, "cm_core_r1"},
      {0x00032820, "cm_core_r2"},
      {0x00032830, "cm_core_r3"},
      {0x00032840, "cm_core_r4"},
      {0x00032850, "cm_core_r5"},
      {0x00032860, "cm_core_r6"},
      {0x00032870, "cm_core_r7"},
      {0x00032880, "cm_core_r8"},
      {0x00032890, "cm_core_r9"},
      {0x000328A0, "cm_core_r10"},
      {0x000328B0, "cm_core_r11"},
      {0x000328C0, "cm_core_r12"},
      {0x000328D0, "cm_core_r13"},
      {0x000328E0, "cm_core_r14"},
      {0x000328F0, "cm_core_r15"},
      {0x00032900, "cm_core_r16"},
      {0x00032910, "cm_core_r17"},
      {0x00032920, "cm_core_r18"},
      {0x00032930, "cm_core_r19"},
      {0x00032940, "cm_core_r20"},
      {0x00032950, "cm_core_r21"},
      {0x00032960, "cm_core_r22"},
      {0x00032970, "cm_core_r23"},
      {0x00032980, "cm_core_r24"},
      {0x00032990, "cm_core_r25"},
      {0x000329A0, "cm_core_r26"},
      {0x000329B0, "cm_core_r27"},
      {0x000329C0, "cm_core_r28"},
      {0x000329D0, "cm_core_r29"},
      {0x000329E0, "cm_core_r30"},
      {0x000329F0, "cm_core_r31"},
      {0x00032A00, "cm_core_m0"},
      {0x00032A10, "cm_core_m1"},
      {0x00032A20, "cm_core_m2"},
      {0x00032A30, "cm_core_m3"},
      {0x00032A40, "cm_core_m4"},
      {0x00032A50, "cm_core_m5"},
      {0x00032A60, "cm_core_m6"},
      {0x00032A70, "cm_core_m7"},
      {0x00032A80, "cm_core_dn0"},
      {0x00032A90, "cm_core_dn1"},
      {0x00032AA0, "cm_core_dn2"},
      {0x00032AB0, "cm_core_dn3"},
      {0x00032AC0, "cm_core_dn4"},
      {0x00032AD0, "cm_core_dn5"},
      {0x00032AE0, "cm_core_dn6"},
      {0x00032AF0, "cm_core_dn7"},
      {0x00032B00, "cm_core_dj0"},
      {0x00032B10, "cm_core_dj1"},
      {0x00032B20, "cm_core_dj2"},
      {0x00032B30, "cm_core_dj3"},
      {0x00032B40, "cm_core_dj4"},
      {0x00032B50, "cm_core_dj5"},
      {0x00032B60, "cm_core_dj6"},
      {0x00032B70, "cm_core_dj7"},
      {0x00032B80, "cm_core_dc0"},
      {0x00032B90, "cm_core_dc1"},
      {0x00032BA0, "cm_core_dc2"},
      {0x00032BB0, "cm_core_dc3"},
      {0x00032BC0, "cm_core_dc4"},
      {0x00032BD0, "cm_core_dc5"},
      {0x00032BE0, "cm_core_dc6"},
      {0x00032BF0, "cm_core_dc7"},
      {0x00032C00, "cm_core_p0"},
      {0x00032C10, "cm_core_p1"},
      {0x00032C20, "cm_core_p2"},
      {0x00032C30, "cm_core_p3"},
      {0x00032C40, "cm_core_p4"},
      {0x00032C50, "cm_core_p5"},
      {0x00032C60, "cm_core_p6"},
      {0x00032C70, "cm_core_p7"},
      {0x00032C80, "cm_core_s0"},
      {0x00032C90, "cm_core_s1"},
      {0x00032CA0, "cm_core_s2"},
      {0x00032CB0, "cm_core_s3"},
      {0x00032D00, "cm_program_counter"},
      {0x00032D10, "cm_core_fc"},
      {0x00032D20, "cm_core_sp"},
      {0x00032D30, "cm_core_lr"},
      {0x00032D40, "cm_core_ls"},
      {0x00032D50, "cm_core_le"},
      {0x00032D60, "cm_core_lc"},
      {0x00032D70, "cm_core_lci"},
      {0x00032D80, "cm_core_cr1"},
      {0x00032D90, "cm_core_cr2"},
      {0x00032DA0, "cm_core_cr3"},
      {0x00032DC0, "cm_core_sr1"},
      {0x00032DD0, "cm_core_sr2"},
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
      {0x00036040, "cm_cssd_trigger"},
      {0x00036060, "cm_accumulator_control"},
      {0x00036070, "cm_memory_control"},
      {0x00037500, "cm_performance_control0"},
      {0x00037504, "cm_performance_control1"},
      {0x00037508, "cm_performance_control2"},
      {0x00037520, "cm_performance_counter0"},
      {0x00037524, "cm_performance_counter1"},
      {0x00037528, "cm_performance_counter2"},
      {0x0003752C, "cm_performance_counter3"},
      {0x00037580, "cm_performance_counter0_event_value"},
      {0x00037584, "cm_performance_counter1_event_value"},
      {0x00037588, "cm_performance_counter2_event_value"},
      {0x0003758C, "cm_performance_counter3_event_value"},
      {0x00038000, "cm_core_control"},
      {0x00038004, "cm_core_status"},
      {0x00038008, "cm_enable_events"},
      {0x0003800C, "cm_reset_event"},
      {0x00038010, "cm_debug_control0"},
      {0x00038014, "cm_debug_control1"},
      {0x00038018, "cm_debug_control2"},
      {0x0003801C, "cm_debug_status"},
      {0x00038020, "cm_pc_event0"},
      {0x00038024, "cm_pc_event1"},
      {0x00038028, "cm_pc_event2"},
      {0x0003802C, "cm_pc_event3"},
      {0x00038030, "cm_error_halt_control"},
      {0x00038034, "cm_error_halt_event"},
      {0x00038038, "cm_core_processor_bus"},
      {0x00038100, "cm_ecc_control"},
      {0x00038110, "cm_ecc_scrubbing_event"},
      {0x00038120, "cm_ecc_failing_address"},
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
      {0x00060010, "cm_module_reset_control"},
      {0x00060020, "cm_tile_control"},
      {0x00060030, "cm_core_reset_defeature"},
      {0x00060100, "cm_spare_reg_privileged"},
      {0x00060104, "cm_spare_reg"}
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
      {0x0001100C, "mm_performance_control2"},
      {0x00011010, "mm_performance_control3"},
      {0x00011020, "mm_performance_counter0"},
      {0x00011080, "mm_performance_counter0_event_value"},
      {0x00011024, "mm_performance_counter1"},
      {0x00011084, "mm_performance_counter1_event_value"},
      {0x00011028, "mm_performance_counter2"},
      {0x0001102C, "mm_performance_counter3"},
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
      {0x000A06E0, "mem_dma_mm2s_0_constant_pad_value"},
      {0x000A0630, "mem_dma_mm2s_0_ctrl"},
      {0x000A0634, "mem_dma_mm2s_0_start_queue"},
      {0x000A06E4, "mem_dma_mm2s_1_constant_pad_value"},
      {0x000A0638, "mem_dma_mm2s_1_ctrl"},
      {0x000A063C, "mem_dma_mm2s_1_start_queue"},
      {0x000A06E8, "mem_dma_mm2s_2_constant_pad_value"},
      {0x000A0640, "mem_dma_mm2s_2_ctrl"},
      {0x000A0644, "mem_dma_mm2s_2_start_queue"},
      {0x000A06EC, "mem_dma_mm2s_3_constant_pad_value"},
      {0x000A0648, "mem_dma_mm2s_3_ctrl"},
      {0x000A064C, "mem_dma_mm2s_3_start_queue"},
      {0x000A06F0, "mem_dma_mm2s_4_constant_pad_value"},
      {0x000A0650, "mem_dma_mm2s_4_ctrl"},
      {0x000A0654, "mem_dma_mm2s_4_start_queue"},
      {0x000A06F4, "mem_dma_mm2s_5_constant_pad_value"},
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
      {0x0009100C, "mem_performance_control3"},
      {0x00091010, "mem_performance_control4"},
      {0x00091020, "mem_performance_counter0"},
      {0x00091080, "mem_performance_counter0_event_value"},
      {0x00091024, "mem_performance_counter1"},
      {0x00091084, "mem_performance_counter1_event_value"},
      {0x00091028, "mem_performance_counter2"},
      {0x00091088, "mem_performance_counter2_event_value"},
      {0x0009102C, "mem_performance_counter3"},
      {0x0009108C, "mem_performance_counter3_event_value"},
      {0x00091030, "mem_performance_counter4"},
      {0x00091034, "mem_performance_counter5"},
      {0x000FFFF4, "mem_spare_reg"},
      {0x000FFFF0, "mem_spare_reg_privileged"},
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
      {0x000FFF20, "mem_tile_control"},
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
      {0x00000000, "shim_lock0_value"},
      {0x00000010, "shim_lock1_value"},
      {0x00000020, "shim_lock2_value"},
      {0x00000030, "shim_lock3_value"},
      {0x00000040, "shim_lock4_value"},
      {0x00000050, "shim_lock5_value"},
      {0x00000060, "shim_lock6_value"},
      {0x00000070, "shim_lock7_value"},
      {0x00000080, "shim_lock8_value"},
      {0x00000090, "shim_lock9_value"},
      {0x000000A0, "shim_lock10_value"},
      {0x000000B0, "shim_lock11_value"},
      {0x000000C0, "shim_lock12_value"},
      {0x000000D0, "shim_lock13_value"},
      {0x000000E0, "shim_lock14_value"},
      {0x000000F0, "shim_lock15_value"},
      {0x00000100, "shim_locks_event_selection_0"},
      {0x00000104, "shim_locks_event_selection_1"},
      {0x00000108, "shim_locks_event_selection_2"},
      {0x0000010C, "shim_locks_event_selection_3"},
      {0x00000110, "shim_locks_event_selection_4"},
      {0x00000114, "shim_locks_event_selection_5"},
      {0x00000120, "shim_locks_overflow"},
      {0x00000128, "shim_locks_underflow"},
      {0x00001000, "shim_interrupt_controller_2nd_level_mask"},
      {0x00001004, "shim_interrupt_controller_2nd_level_enable"},
      {0x00001008, "shim_interrupt_controller_2nd_level_disable"},
      {0x0000100C, "shim_interrupt_controller_2nd_level_status"},
      {0x00001010, "shim_interrupt_controller_2nd_level_interrupt"},
      {0x00002000, "shim_spare_reg"},
      {0x00002100, "shim_me_aximm_config"},
      {0x00002104, "shim_mux_config"},
      {0x00002108, "shim_demux_config"},
      {0x00002120, "shim_axi_mm_outstanding_transactions"},
      {0x00003000, "shim_smid"},
      {0x00009000, "shim_dma_bd0_0"},
      {0x00009004, "shim_dma_bd0_1"},
      {0x00009008, "shim_dma_bd0_2"},
      {0x0000900C, "shim_dma_bd0_3"},
      {0x00009010, "shim_dma_bd0_4"},
      {0x00009014, "shim_dma_bd0_5"},
      {0x00009018, "shim_dma_bd0_6"},
      {0x0000901C, "shim_dma_bd0_7"},
      {0x00009020, "shim_dma_bd0_8"},
      {0x00009030, "shim_dma_bd1_0"},
      {0x00009034, "shim_dma_bd1_1"},
      {0x00009038, "shim_dma_bd1_2"},
      {0x0000903C, "shim_dma_bd1_3"},
      {0x00009040, "shim_dma_bd1_4"},
      {0x00009044, "shim_dma_bd1_5"},
      {0x00009048, "shim_dma_bd1_6"},
      {0x0000904C, "shim_dma_bd1_7"},
      {0x00009050, "shim_dma_bd1_8"},
      {0x00009060, "shim_dma_bd2_0"},
      {0x00009064, "shim_dma_bd2_1"},
      {0x00009068, "shim_dma_bd2_2"},
      {0x0000906C, "shim_dma_bd2_3"},
      {0x00009070, "shim_dma_bd2_4"},
      {0x00009074, "shim_dma_bd2_5"},
      {0x00009078, "shim_dma_bd2_6"},
      {0x0000907C, "shim_dma_bd2_7"},
      {0x00009080, "shim_dma_bd2_8"},
      {0x00009090, "shim_dma_bd3_0"},
      {0x00009094, "shim_dma_bd3_1"},
      {0x00009098, "shim_dma_bd3_2"},
      {0x0000909C, "shim_dma_bd3_3"},
      {0x000090A0, "shim_dma_bd3_4"},
      {0x000090A4, "shim_dma_bd3_5"},
      {0x000090A8, "shim_dma_bd3_6"},
      {0x000090AC, "shim_dma_bd3_7"},
      {0x000090B0, "shim_dma_bd3_8"},
      {0x000090C0, "shim_dma_bd4_0"},
      {0x000090C4, "shim_dma_bd4_1"},
      {0x000090C8, "shim_dma_bd4_2"},
      {0x000090CC, "shim_dma_bd4_3"},
      {0x000090D0, "shim_dma_bd4_4"},
      {0x000090D4, "shim_dma_bd4_5"},
      {0x000090D8, "shim_dma_bd4_6"},
      {0x000090DC, "shim_dma_bd4_7"},
      {0x000090E0, "shim_dma_bd4_8"},
      {0x000090F0, "shim_dma_bd5_0"},
      {0x000090F4, "shim_dma_bd5_1"},
      {0x000090F8, "shim_dma_bd5_2"},
      {0x000090FC, "shim_dma_bd5_3"},
      {0x00009100, "shim_dma_bd5_4"},
      {0x00009104, "shim_dma_bd5_5"},
      {0x00009108, "shim_dma_bd5_6"},
      {0x0000910C, "shim_dma_bd5_7"},
      {0x00009110, "shim_dma_bd5_8"},
      {0x00009120, "shim_dma_bd6_0"},
      {0x00009124, "shim_dma_bd6_1"},
      {0x00009128, "shim_dma_bd6_2"},
      {0x0000912C, "shim_dma_bd6_3"},
      {0x00009130, "shim_dma_bd6_4"},
      {0x00009134, "shim_dma_bd6_5"},
      {0x00009138, "shim_dma_bd6_6"},
      {0x0000913C, "shim_dma_bd6_7"},
      {0x00009140, "shim_dma_bd6_8"},
      {0x00009150, "shim_dma_bd7_0"},
      {0x00009154, "shim_dma_bd7_1"},
      {0x00009158, "shim_dma_bd7_2"},
      {0x0000915C, "shim_dma_bd7_3"},
      {0x00009160, "shim_dma_bd7_4"},
      {0x00009164, "shim_dma_bd7_5"},
      {0x00009168, "shim_dma_bd7_6"},
      {0x0000916C, "shim_dma_bd7_7"},
      {0x00009170, "shim_dma_bd7_8"},
      {0x00009180, "shim_dma_bd8_0"},
      {0x00009184, "shim_dma_bd8_1"},
      {0x00009188, "shim_dma_bd8_2"},
      {0x0000918C, "shim_dma_bd8_3"},
      {0x00009190, "shim_dma_bd8_4"},
      {0x00009194, "shim_dma_bd8_5"},
      {0x00009198, "shim_dma_bd8_6"},
      {0x0000919C, "shim_dma_bd8_7"},
      {0x000091A0, "shim_dma_bd8_8"},
      {0x000091B0, "shim_dma_bd9_0"},
      {0x000091B4, "shim_dma_bd9_1"},
      {0x000091B8, "shim_dma_bd9_2"},
      {0x000091BC, "shim_dma_bd9_3"},
      {0x000091C0, "shim_dma_bd9_4"},
      {0x000091C4, "shim_dma_bd9_5"},
      {0x000091C8, "shim_dma_bd9_6"},
      {0x000091CC, "shim_dma_bd9_7"},
      {0x000091D0, "shim_dma_bd9_8"},
      {0x000091E0, "shim_dma_bd10_0"},
      {0x000091E4, "shim_dma_bd10_1"},
      {0x000091E8, "shim_dma_bd10_2"},
      {0x000091EC, "shim_dma_bd10_3"},
      {0x000091F0, "shim_dma_bd10_4"},
      {0x000091F4, "shim_dma_bd10_5"},
      {0x000091F8, "shim_dma_bd10_6"},
      {0x000091FC, "shim_dma_bd10_7"},
      {0x00009200, "shim_dma_bd10_8"},
      {0x00009210, "shim_dma_bd11_0"},
      {0x00009214, "shim_dma_bd11_1"},
      {0x00009218, "shim_dma_bd11_2"},
      {0x0000921C, "shim_dma_bd11_3"},
      {0x00009220, "shim_dma_bd11_4"},
      {0x00009224, "shim_dma_bd11_5"},
      {0x00009228, "shim_dma_bd11_6"},
      {0x0000922C, "shim_dma_bd11_7"},
      {0x00009230, "shim_dma_bd11_8"},
      {0x00009240, "shim_dma_bd12_0"},
      {0x00009244, "shim_dma_bd12_1"},
      {0x00009248, "shim_dma_bd12_2"},
      {0x0000924C, "shim_dma_bd12_3"},
      {0x00009250, "shim_dma_bd12_4"},
      {0x00009254, "shim_dma_bd12_5"},
      {0x00009258, "shim_dma_bd12_6"},
      {0x0000925C, "shim_dma_bd12_7"},
      {0x00009260, "shim_dma_bd12_8"},
      {0x00009270, "shim_dma_bd13_0"},
      {0x00009274, "shim_dma_bd13_1"},
      {0x00009278, "shim_dma_bd13_2"},
      {0x0000927C, "shim_dma_bd13_3"},
      {0x00009280, "shim_dma_bd13_4"},
      {0x00009284, "shim_dma_bd13_5"},
      {0x00009288, "shim_dma_bd13_6"},
      {0x0000928C, "shim_dma_bd13_7"},
      {0x00009290, "shim_dma_bd13_8"},
      {0x000092A0, "shim_dma_bd14_0"},
      {0x000092A4, "shim_dma_bd14_1"},
      {0x000092A8, "shim_dma_bd14_2"},
      {0x000092AC, "shim_dma_bd14_3"},
      {0x000092B0, "shim_dma_bd14_4"},
      {0x000092B4, "shim_dma_bd14_5"},
      {0x000092B8, "shim_dma_bd14_6"},
      {0x000092BC, "shim_dma_bd14_7"},
      {0x000092C0, "shim_dma_bd14_8"},
      {0x000092D0, "shim_dma_bd15_0"},
      {0x000092D4, "shim_dma_bd15_1"},
      {0x000092D8, "shim_dma_bd15_2"},
      {0x000092DC, "shim_dma_bd15_3"},
      {0x000092E0, "shim_dma_bd15_4"},
      {0x000092E4, "shim_dma_bd15_5"},
      {0x000092E8, "shim_dma_bd15_6"},
      {0x000092EC, "shim_dma_bd15_7"},
      {0x000092F0, "shim_dma_bd15_8"},
      {0x00009300, "shim_dma_s2mm_0_ctrl"},
      {0x00009304, "shim_dma_s2mm_0_task_queue"},
      {0x00009308, "shim_dma_s2mm_1_ctrl"},
      {0x0000930C, "shim_dma_s2mm_1_task_queue"},
      {0x00009310, "shim_dma_mm2s_0_ctrl"},
      {0x00009314, "shim_dma_mm2s_0_task_queue"},
      {0x00009318, "shim_dma_mm2s_1_ctrl"},
      {0x0000931C, "shim_dma_mm2s_1_task_queue"},
      {0x00009320, "shim_dma_s2mm_status_0"},
      {0x00009324, "shim_dma_s2mm_status_1"},
      {0x00009328, "shim_dma_mm2s_status_0"},
      {0x0000932C, "shim_dma_mm2s_status_1"},
      {0x00009330, "shim_dma_s2mm_current_write_count_0"},
      {0x00009334, "shim_dma_s2mm_current_write_count_1"},
      {0x00009338, "shim_dma_s2mm_fot_count_fifo_pop_0"},
      {0x0000933C, "shim_dma_s2mm_fot_count_fifo_pop_1"},
      {0x00009340, "shim_dma_mm2s_0_response_fifo_parity_error_injection"},
      {0x00009344, "shim_dma_mm2s_1_response_fifo_parity_error_injection"},
      {0x00009348, "shim_dma_pause"},
      {0x0000C000, "shim_lock_request"},
      {0x00030000, "shim_pl_interface_upsizer_config"},
      {0x00030004, "shim_pl_interface_downsizer_config"},
      {0x00030008, "shim_pl_interface_downsizer_enable"},
      {0x0003000C, "shim_pl_interface_downsizer_bypass"},
      {0x00031000, "shim_performance_control0"},
      {0x00031000, "shim_performance_start_stop_0_1"},
      {0x00031008, "shim_performance_control1"},
      {0x00031008, "shim_performance_reset_0_1"},
      {0x0003100C, "shim_performance_control2"},
      {0x0003100C, "shim_performance_start_stop_2_3"},
      {0x00031010, "shim_performance_control3"},
      {0x00031010, "shim_performance_reset_2_3"},
      {0x00031014, "shim_performance_control4"},
      {0x00031014, "shim_performance_start_stop_4_5"},
      {0x00031018, "shim_performance_control5"},
      {0x00031018, "shim_performance_reset_4_5"},
      {0x00031020, "shim_performance_counter0"},
      {0x00031024, "shim_performance_counter1"},
      {0x00031028, "shim_performance_counter2"},
      {0x0003102C, "shim_performance_counter3"},
      {0x00031030, "shim_performance_counter4"},
      {0x00031034, "shim_performance_counter5"},
      {0x00031080, "shim_performance_counter0_event_value"},
      {0x00031084, "shim_performance_counter1_event_value"},
      {0x00034000, "shim_timer_control"},
      {0x00034008, "shim_event_generate"},
      {0x00034010, "shim_event_broadcast_a_0"},
      {0x00034014, "shim_event_broadcast_a_1"},
      {0x00034018, "shim_event_broadcast_a_2"},
      {0x0003401C, "shim_event_broadcast_a_3"},
      {0x00034020, "shim_event_broadcast_a_4"},
      {0x00034024, "shim_event_broadcast_a_5"},
      {0x00034028, "shim_event_broadcast_a_6"},
      {0x0003402C, "shim_event_broadcast_a_7"},
      {0x00034030, "shim_event_broadcast_a_8"},
      {0x00034034, "shim_event_broadcast_a_9"},
      {0x00034038, "shim_event_broadcast_a_10"},
      {0x0003403C, "shim_event_broadcast_a_11"},
      {0x00034040, "shim_event_broadcast_a_12"},
      {0x00034044, "shim_event_broadcast_a_13"},
      {0x00034048, "shim_event_broadcast_a_14"},
      {0x0003404C, "shim_event_broadcast_a_15"},
      {0x00034050, "shim_event_broadcast_a_block_south_set"},
      {0x00034054, "shim_event_broadcast_a_block_south_clr"},
      {0x00034058, "shim_event_broadcast_a_block_south_value"},
      {0x00034060, "shim_event_broadcast_a_block_west_set"},
      {0x00034064, "shim_event_broadcast_a_block_west_clr"},
      {0x00034068, "shim_event_broadcast_a_block_west_value"},
      {0x00034070, "shim_event_broadcast_a_block_north_set"},
      {0x00034074, "shim_event_broadcast_a_block_north_clr"},
      {0x00034078, "shim_event_broadcast_a_block_north_value"},
      {0x00034080, "shim_event_broadcast_a_block_east_set"},
      {0x00034084, "shim_event_broadcast_a_block_east_clr"},
      {0x00034088, "shim_event_broadcast_a_block_east_value"},
      {0x00034090, "shim_event_broadcast_b_block_south_set"},
      {0x00034094, "shim_event_broadcast_b_block_south_clr"},
      {0x00034098, "shim_event_broadcast_b_block_south_value"},
      {0x000340A0, "shim_event_broadcast_b_block_west_set"},
      {0x000340A4, "shim_event_broadcast_b_block_west_clr"},
      {0x000340A8, "shim_event_broadcast_b_block_west_value"},
      {0x000340B0, "shim_event_broadcast_b_block_north_set"},
      {0x000340B4, "shim_event_broadcast_b_block_north_clr"},
      {0x000340B8, "shim_event_broadcast_b_block_north_value"},
      {0x000340C0, "shim_event_broadcast_b_block_east_set"},
      {0x000340C4, "shim_event_broadcast_b_block_east_clr"},
      {0x000340C8, "shim_event_broadcast_b_block_east_value"},
      {0x000340D0, "shim_trace_control0"},
      {0x000340D4, "shim_trace_control1"},
      {0x000340D8, "shim_trace_status"},
      {0x000340E0, "shim_trace_event0"},
      {0x000340E4, "shim_trace_event1"},
      {0x000340F0, "shim_timer_trig_event_low_value"},
      {0x000340F4, "shim_timer_trig_event_high_value"},
      {0x000340F8, "shim_timer_low"},
      {0x000340FC, "shim_timer_high"},
      {0x00034200, "shim_event_status0"},
      {0x00034204, "shim_event_status1"},
      {0x00034208, "shim_event_status2"},
      {0x0003420C, "shim_event_status3"},
      {0x00034210, "shim_event_status4"},
      {0x00034214, "shim_event_status5"},
      {0x00034218, "shim_event_status6"},
      {0x0003421C, "shim_event_status7"},
      {0x00034400, "shim_combo_event_inputs"},
      {0x00034404, "shim_combo_event_control"},
      {0x00034408, "shim_edge_detection_event_control"},
      {0x00034500, "shim_event_group_0_enable"},
      {0x00034504, "shim_event_group_dma_enable"},
      {0x00034508, "shim_event_group_noc_1_dma_activity_enable"},
      {0x0003450C, "shim_event_group_noc_0_lock_enable"},
      {0x00034510, "shim_event_group_noc_1_lock_enable"},
      {0x00034514, "shim_event_group_errors_enable"},
      {0x00034518, "shim_event_group_stream_switch_enable"},
      {0x0003451C, "shim_event_group_broadcast_a_enable"},
      {0x00034520, "shim_event_group_uc_dma_activity_enable"},
      {0x00034524, "shim_event_group_uc_module_errors_enable"},
      {0x00034528, "shim_event_group_uc_core_streams_enable"},
      {0x0003452C, "shim_event_group_uc_core_program_flow_enable"},
      {0x00034600, "shim_uc_core_interrupt_event"},
      {0x00035000, "shim_interrupt_controller_1st_level_mask_a"},
      {0x00035004, "shim_interrupt_controller_1st_level_enable_a"},
      {0x00035008, "shim_interrupt_controller_1st_level_disable_a"},
      {0x0003500C, "shim_interrupt_controller_1st_level_status_a"},
      {0x00035010, "shim_interrupt_controller_1st_level_irq_no_a"},
      {0x00035014, "shim_interrupt_controller_1st_level_irq_event_a"},
      {0x00035018, "shim_interrupt_controller_1st_level_block_north_in_a_set"},
      {0x0003501C, "shim_interrupt_controller_1st_level_block_north_in_a_clear"},
      {0x00035020, "shim_interrupt_controller_1st_level_block_north_in_a_value"},
      {0x00035030, "shim_interrupt_controller_1st_level_mask_b"},
      {0x00035034, "shim_interrupt_controller_1st_level_enable_b"},
      {0x00035038, "shim_interrupt_controller_1st_level_disable_b"},
      {0x0003503C, "shim_interrupt_controller_1st_level_status_b"},
      {0x00035040, "shim_interrupt_controller_1st_level_irq_no_b"},
      {0x00035044, "shim_interrupt_controller_1st_level_irq_event_b"},
      {0x00035048, "shim_interrupt_controller_1st_level_block_north_in_b_set"},
      {0x0003504C, "shim_interrupt_controller_1st_level_block_north_in_b_clear"},
      {0x00035050, "shim_interrupt_controller_1st_level_block_north_in_b_value"},
      {0x00036000, "shim_bisr_cache_ctrl"},
      {0x00036008, "shim_bisr_cache_status"},
      {0x00036010, "shim_bisr_cache_data0"},
      {0x00036014, "shim_bisr_cache_data1"},
      {0x00036018, "shim_bisr_cache_data2"},
      {0x0003601C, "shim_bisr_cache_data3"},
      {0x00036020, "shim_bisr_cache_data4"},
      {0x00036024, "shim_bisr_cache_data5"},
      {0x00036028, "shim_bisr_cache_data6"},
      {0x0003602C, "shim_bisr_cache_data7"},
      {0x00036030, "shim_bisr_test_data0"},
      {0x00036034, "shim_bisr_test_data1"},
      {0x00036038, "shim_bisr_test_data2"},
      {0x0003603C, "shim_bisr_test_data3"},
      {0x00036040, "shim_bisr_test_data4"},
      {0x00036044, "shim_bisr_test_data5"},
      {0x00036048, "shim_bisr_test_data6"},
      {0x0003604C, "shim_bisr_test_data7"},
      {0x0003F000, "shim_stream_switch_master_config_tile_ctrl"},
      {0x0003F004, "shim_stream_switch_master_config_fifo0"},
      {0x0003F008, "shim_stream_switch_master_config_south0"},
      {0x0003F00C, "shim_stream_switch_master_config_south1"},
      {0x0003F010, "shim_stream_switch_master_config_south2"},
      {0x0003F014, "shim_stream_switch_master_config_south3"},
      {0x0003F018, "shim_stream_switch_master_config_south4"},
      {0x0003F01C, "shim_stream_switch_master_config_south5"},
      {0x0003F020, "shim_stream_switch_master_config_west0"},
      {0x0003F024, "shim_stream_switch_master_config_west1"},
      {0x0003F028, "shim_stream_switch_master_config_west2"},
      {0x0003F02C, "shim_stream_switch_master_config_west3"},
      {0x0003F030, "shim_stream_switch_master_config_north0"},
      {0x0003F034, "shim_stream_switch_master_config_north1"},
      {0x0003F038, "shim_stream_switch_master_config_north2"},
      {0x0003F03C, "shim_stream_switch_master_config_north3"},
      {0x0003F040, "shim_stream_switch_master_config_north4"},
      {0x0003F044, "shim_stream_switch_master_config_north5"},
      {0x0003F048, "shim_stream_switch_master_config_east0"},
      {0x0003F04C, "shim_stream_switch_master_config_east1"},
      {0x0003F050, "shim_stream_switch_master_config_east2"},
      {0x0003F054, "shim_stream_switch_master_config_east3"},
      {0x0003F058, "shim_stream_switch_master_config_ucontroller"},
      {0x0003F100, "shim_stream_switch_slave_config_tile_ctrl"},
      {0x0003F104, "shim_stream_switch_slave_config_fifo_0"},
      {0x0003F108, "shim_stream_switch_slave_config_south_0"},
      {0x0003F10C, "shim_stream_switch_slave_config_south_1"},
      {0x0003F110, "shim_stream_switch_slave_config_south_2"},
      {0x0003F114, "shim_stream_switch_slave_config_south_3"},
      {0x0003F118, "shim_stream_switch_slave_config_south_4"},
      {0x0003F11C, "shim_stream_switch_slave_config_south_5"},
      {0x0003F120, "shim_stream_switch_slave_config_south_6"},
      {0x0003F124, "shim_stream_switch_slave_config_south_7"},
      {0x0003F128, "shim_stream_switch_slave_config_west_0"},
      {0x0003F12C, "shim_stream_switch_slave_config_west_1"},
      {0x0003F130, "shim_stream_switch_slave_config_west_2"},
      {0x0003F134, "shim_stream_switch_slave_config_west_3"},
      {0x0003F138, "shim_stream_switch_slave_config_north_0"},
      {0x0003F13C, "shim_stream_switch_slave_config_north_1"},
      {0x0003F140, "shim_stream_switch_slave_config_north_2"},
      {0x0003F144, "shim_stream_switch_slave_config_north_3"},
      {0x0003F148, "shim_stream_switch_slave_config_east_0"},
      {0x0003F14C, "shim_stream_switch_slave_config_east_1"},
      {0x0003F150, "shim_stream_switch_slave_config_east_2"},
      {0x0003F154, "shim_stream_switch_slave_config_east_3"},
      {0x0003F158, "shim_stream_switch_slave_config_trace"},
      {0x0003F15C, "shim_stream_switch_slave_config_ucontroller"},
      {0x0003F200, "shim_stream_switch_slave_tile_ctrl_slot0"},
      {0x0003F204, "shim_stream_switch_slave_tile_ctrl_slot1"},
      {0x0003F208, "shim_stream_switch_slave_tile_ctrl_slot2"},
      {0x0003F20C, "shim_stream_switch_slave_tile_ctrl_slot3"},
      {0x0003F210, "shim_stream_switch_slave_fifo_0_slot0"},
      {0x0003F214, "shim_stream_switch_slave_fifo_0_slot1"},
      {0x0003F218, "shim_stream_switch_slave_fifo_0_slot2"},
      {0x0003F21C, "shim_stream_switch_slave_fifo_0_slot3"},
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
      {0x0003F360, "shim_stream_switch_slave_trace_slot0"},
      {0x0003F364, "shim_stream_switch_slave_trace_slot1"},
      {0x0003F368, "shim_stream_switch_slave_trace_slot2"},
      {0x0003F36C, "shim_stream_switch_slave_trace_slot3"},
      {0x0003F370, "shim_stream_switch_slave_ucontroller_slot0"},
      {0x0003F374, "shim_stream_switch_slave_ucontroller_slot1"},
      {0x0003F378, "shim_stream_switch_slave_ucontroller_slot2"},
      {0x0003F37C, "shim_stream_switch_slave_ucontroller_slot3"},
      {0x0003F800, "shim_stream_switch_deterministic_merge_arb0_slave0_1"},
      {0x0003F804, "shim_stream_switch_deterministic_merge_arb0_slave2_3"},
      {0x0003F808, "shim_stream_switch_deterministic_merge_arb0_ctrl"},
      {0x0003F810, "shim_stream_switch_deterministic_merge_arb1_slave0_1"},
      {0x0003F814, "shim_stream_switch_deterministic_merge_arb1_slave2_3"},
      {0x0003F818, "shim_stream_switch_deterministic_merge_arb1_ctrl"},
      {0x0003FF00, "shim_stream_switch_event_port_selection_0"},
      {0x0003FF04, "shim_stream_switch_event_port_selection_1"},
      {0x0003FF10, "shim_stream_switch_parity_status"},
      {0x0003FF20, "shim_stream_switch_parity_injection"},
      {0x0003FF30, "shim_control_packet_handler_status"},
      {0x0003FF34, "shim_stream_switch_adaptive_clock_gate_status"},
      {0x0003FF38, "shim_stream_switch_adaptive_clock_gate_abort_period"},
      {0x0007FF00, "shim_module_clock_control_0"},
      {0x0007FF04, "shim_module_clock_control_1"},
      {0x0007FF10, "shim_module_reset_control_0"},
      {0x0007FF14, "shim_module_reset_control_1"},
      {0x0007FF20, "shim_column_clock_control"},
      {0x0007FF28, "shim_column_reset_control"},
      {0x0007FF30, "shim_spare_reg_privileged"},
      {0x0007FF34, "shim_spare_reg"},
      {0x0007FF40, "shim_tile_control"},
      {0x0007FF44, "shim_tile_control_axi_mm"},
      {0x0007FF48, "shim_nmu_switches_config"},
      {0x0007FF4C, "shim_cssd_trigger"},
      {0x0007FF50, "shim_interrupt_controller_hw_error_mask"},
      {0x0007FF54, "shim_interrupt_controller_hw_error_status"},
      {0x0007FF58, "shim_interrupt_controller_hw_error_interrupt"},
      {0x10, "shim_lock_step_size"},
      {0x20, "shim_dma_bd_step_size"},
      {0x8, "shim_dma_s2mm_step_size"}
   };

   // uc_module registers
   ucRegValueToName = {
      {0x000C0004, "uc_core_control"},
      {0x000C0008, "uc_core_interrupt_status"},
      {0x000C0000, "uc_core_status"},
      {0x000C0108, "uc_dma_dm2mm_axi_control"},
      {0x000C0104, "uc_dma_dm2mm_control"},
      {0x000C0100, "uc_dma_dm2mm_status"},
      {0x000C0118, "uc_dma_mm2dm_axi_control"},
      {0x000C0114, "uc_dma_mm2dm_control"},
      {0x000C0110, "uc_dma_mm2dm_status"},
      {0x000C0120, "uc_dma_pause"},
      {0x000B0010, "uc_mdm_dbg_ctrl_status"},
      {0x000B0014, "uc_mdm_dbg_data"},
      {0x000B0018, "uc_mdm_dbg_lock"},
      {0x000B5480, "uc_mdm_pccmdr"},
      {0x000B5440, "uc_mdm_pcctrlr"},
      {0x000B5580, "uc_mdm_pcdrr"},
      {0x000B54C0, "uc_mdm_pcsr"},
      {0x000B55C0, "uc_mdm_pcwr"},
      {0x000C003C, "uc_memory_dm_ecc_error_generation"},
      {0x000C0038, "uc_memory_dm_ecc_scrubbing_period"},
      {0x000C0034, "uc_memory_privileged"},
      {0x000C0030, "uc_memory_zeroization"},
      {0x000C0020, "uc_module_aximm_offset"},
      {0x000C0024, "uc_module_axi_mm_outstanding_transactions"}
   };

   // npi_module registers
   npiRegValueToName = {
      {0x00000030, "npi_me_isr"},
      {0x00000034, "npi_me_itr"},
      {0x00000038, "npi_me_imr0"},
      {0x0000003C, "npi_me_ier0"},
      {0x00000040, "npi_me_idr0"},
      {0x00000044, "npi_me_imr1"},
      {0x00000048, "npi_me_ier1"},
      {0x0000004C, "npi_me_idr1"},
      {0x00000050, "npi_me_imr2"},
      {0x00000054, "npi_me_ier2"},
      {0x00000058, "npi_me_idr2"},
      {0x0000005C, "npi_me_imr3"},
      {0x00000060, "npi_me_ier3"},
      {0x00000064, "npi_me_idr3"},
      {0x0000006C, "npi_me_ior"},
      {0x0000010C, "npi_me_pll_status"},
      {0x00000208, "npi_me_secure_reg"},
      {0x00000000, "uc_base_address"}
   };
}

void AIE2psUsedRegisters::populateRegAddrToSizeMap() {
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
      {0x00031800, 128},
      {0x00031810, 128},
      {0x00031820, 128},
      {0x00031830, 128},
      {0x00031840, 128},
      {0x00031850, 128},
      {0x00031860, 128},
      {0x00031870, 128},
      {0x00031880, 128},
      {0x00031890, 128},
      {0x000318A0, 128},
      {0x000318B0, 128},
      {0x000318C0, 128},
      {0x000318D0, 128},
      {0x000318E0, 128},
      {0x000318F0, 128},
      {0x00031900, 128},
      {0x00031910, 128},
      {0x00031920, 128},
      {0x00031930, 128},
      {0x00031940, 128},
      {0x00031950, 128},
      {0x00031960, 128},
      {0x00031970, 128},
      {0x00031980, 128},
      {0x00031990, 128},
      {0x000319A0, 128},
      {0x000319B0, 128},
      {0x000319C0, 128},
      {0x000319D0, 128},
      {0x000319E0, 128},
      {0x000319F0, 128},
      {0x00031A00, 128},
      {0x00031A10, 128},
      {0x00031A20, 128},
      {0x00031A30, 128},
      {0x00031A40, 128},
      {0x00031A50, 128},
      {0x00031A60, 128},
      {0x00031A70, 128},
      {0x00031A80, 128},
      {0x00031A90, 128},
      {0x00031AA0, 128},
      {0x00031AB0, 128},
      {0x00031AC0, 128},
      {0x00031AD0, 128},
      {0x00031AE0, 128},
      {0x00031AF0, 128},
      {0x00032400, 128},
      {0x00032410, 128},
      {0x00032420, 128},
      {0x00032430, 128},
      {0x00032440, 128},
      {0x00032450, 128},
      {0x00032460, 128},
      {0x00032470, 128},
      {0x00032480, 128},
      {0x00032490, 128},
      {0x000324A0, 128},
      {0x000324B0, 128},
      {0x000324C0, 128},
      {0x000324D0, 128},
      {0x000324E0, 128},
      {0x000324F0, 128},
      {0x00032500, 128},
      {0x00032510, 128},
      {0x00032520, 128},
      {0x00032530, 128},
      {0x00032540, 128},
      {0x00032550, 128},
      {0x00032560, 128},
      {0x00032570, 128},
      {0x00032580, 128},
      {0x00032590, 128},
      {0x000325A0, 128},
      {0x000325B0, 128},
      {0x00032600, 128},
      {0x00032610, 128},
      {0x00032620, 128},
      {0x00032630, 128},
      {0x00032640, 128},
      {0x00032650, 128},
      {0x00032660, 128},
      {0x00032670, 128},
      {0x00032680, 128},
      {0x00032690, 128},
      {0x000326A0, 128},
      {0x000326B0, 128},
      {0x00032700, 128},
      {0x00032710, 128},
      {0x00032720, 128},
      {0x00032730, 128},
      {0x00032740, 128},
      {0x00032750, 128},
      {0x00032760, 128},
      {0x00032770, 128},
      {0x00032780, 128},
      {0x00032790, 128},
      {0x000327A0, 128},
      {0x000327B0, 128}
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

   // uc_module registers
   ucRegAddrToSize = {
    };

   //npi module registers
   npiRegAddrToSize ={};

}

}