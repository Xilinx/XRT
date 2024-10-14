/**
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_PLUGIN_SOURCE

#include "xdp/profile/plugin/aie_profile/util/aie_profile_util.h"
#include "xdp/profile/database/static_info/aie_util.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <set>

#include "core/common/message.h"

// ***************************************************************
// Anonymous namespace for helper functions local to this file
// ***************************************************************
namespace xdp::aie::profile {
  using severity_level = xrt_core::message::severity_level;

  /****************************************************************************
   * Get metric sets for core modules
   ***************************************************************************/
  std::map<std::string, std::vector<XAie_Events>> getCoreEventSets(const int hwGen)
  {
    std::map<std::string, std::vector<XAie_Events>> eventSets;
    eventSets = {
      {"heat_map",                {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_GROUP_CORE_STALL_CORE,
                                   XAIE_EVENT_INSTR_VECTOR_CORE,         XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE}},
      {"stalls",                  {XAIE_EVENT_MEMORY_STALL_CORE,         XAIE_EVENT_STREAM_STALL_CORE,
                                   XAIE_EVENT_LOCK_STALL_CORE,           XAIE_EVENT_CASCADE_STALL_CORE}},
      {"execution",               {XAIE_EVENT_INSTR_VECTOR_CORE,         XAIE_EVENT_INSTR_LOAD_CORE,
                                   XAIE_EVENT_INSTR_STORE_CORE,          XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE}},
      {"stream_put_get",          {XAIE_EVENT_INSTR_CASCADE_GET_CORE,    XAIE_EVENT_INSTR_CASCADE_PUT_CORE,
                                   XAIE_EVENT_INSTR_STREAM_GET_CORE,     XAIE_EVENT_INSTR_STREAM_PUT_CORE}},
      {"write_throughputs",       {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_INSTR_STREAM_PUT_CORE,
                                   XAIE_EVENT_INSTR_CASCADE_PUT_CORE,    XAIE_EVENT_GROUP_CORE_STALL_CORE}},
      {"read_throughputs",        {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_INSTR_STREAM_GET_CORE,
                                   XAIE_EVENT_INSTR_CASCADE_GET_CORE,    XAIE_EVENT_GROUP_CORE_STALL_CORE}},
      {"s2mm_throughputs",        {XAIE_EVENT_PORT_RUNNING_0_CORE,       XAIE_EVENT_PORT_STALLED_0_CORE}},
      {"mm2s_throughputs",        {XAIE_EVENT_PORT_RUNNING_0_CORE,       XAIE_EVENT_PORT_STALLED_0_CORE}},
      {"aie_trace",               {XAIE_EVENT_PORT_RUNNING_0_CORE,       XAIE_EVENT_PORT_STALLED_0_CORE,
                                   XAIE_EVENT_PORT_RUNNING_1_CORE,       XAIE_EVENT_PORT_STALLED_1_CORE}},
      {"events",                  {XAIE_EVENT_INSTR_EVENT_0_CORE,        XAIE_EVENT_INSTR_EVENT_1_CORE,
                                   XAIE_EVENT_USER_EVENT_0_CORE,         XAIE_EVENT_USER_EVENT_1_CORE}}
    };

    if (hwGen == 1) {
      eventSets["floating_point"]   = {XAIE_EVENT_FP_OVERFLOW_CORE,    XAIE_EVENT_FP_UNDERFLOW_CORE,
                                       XAIE_EVENT_FP_INVALID_CORE,     XAIE_EVENT_FP_DIV_BY_ZERO_CORE};
    } else {
      eventSets["floating_point"]   = {XAIE_EVENT_FP_HUGE_CORE,        XAIE_EVENT_INT_FP_0_CORE, 
                                       XAIE_EVENT_FP_INVALID_CORE,     XAIE_EVENT_FP_INF_CORE};
    }

    return eventSets;
  }

  /****************************************************************************
   * Get metric sets for memory modules
   * 
   * NOTE: Set names common with core module will be auto-specified when parsing
   ***************************************************************************/
  std::map<std::string, std::vector<XAie_Events>> getMemoryEventSets(const int hwGen)
  {
    std::map<std::string, std::vector<XAie_Events>> eventSets;

    eventSets = {
      {"conflicts",               {XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM, XAIE_EVENT_GROUP_ERRORS_MEM}},
      {"dma_locks",               {XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM,    XAIE_EVENT_GROUP_LOCK_MEM}}
    };

    if (hwGen == 1) {
      eventSets["dma_stalls_s2mm"]  = {XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_ACQUIRE_MEM,
                                       XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_ACQUIRE_MEM};
      eventSets["dma_stalls_mm2s"]  = {XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_ACQUIRE_MEM,
                                       XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_ACQUIRE_MEM};
      eventSets["s2mm_throughputs"] = {XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM,
                                       XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM};
      eventSets["mm2s_throughputs"] = {XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM,
                                       XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM};
    } else {
      eventSets["dma_stalls_s2mm"]  = {XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_MEM,
                                       XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_MEM};
      eventSets["dma_stalls_mm2s"]  = {XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_MEM,
                                       XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_MEM};
      eventSets["s2mm_throughputs"] = {XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_MEM,
                                       XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_MEM};
      eventSets["mm2s_throughputs"] = {XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_MEM,
                                       XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_MEM};
    }

    return eventSets;
  }

  /****************************************************************************
   * Get metric sets for interface tiles
   ***************************************************************************/
  std::map<std::string, std::vector<XAie_Events>> getInterfaceTileEventSets(const int hwGen)
  {
    std::map<std::string, std::vector<XAie_Events>> eventSets;
    eventSets = {
      {"packets",                   {XAIE_EVENT_PORT_TLAST_0_PL,       XAIE_EVENT_PORT_TLAST_1_PL}},
      {"input_throughputs",         {XAIE_EVENT_GROUP_DMA_ACTIVITY_PL, XAIE_EVENT_PORT_RUNNING_0_PL}},
      {"output_throughputs",        {XAIE_EVENT_GROUP_DMA_ACTIVITY_PL, XAIE_EVENT_PORT_RUNNING_0_PL}},
      {METRIC_BYTE_COUNT,           {XAIE_EVENT_PORT_RUNNING_0_PL,     XAIE_EVENT_PORT_RUNNING_0_PL}},
      {METRIC_LATENCY,              {XAIE_EVENT_PORT_RUNNING_0_PL,     XAIE_EVENT_PORT_RUNNING_0_PL}},
    };

    if (hwGen == 1) {
      eventSets["input_stalls"]   = {XAIE_EVENT_PORT_STALLED_0_PL, 
                                     XAIE_EVENT_PORT_IDLE_0_PL};
      eventSets["output_stalls"]  = {XAIE_EVENT_PORT_STALLED_0_PL, 
                                     XAIE_EVENT_PORT_IDLE_0_PL};
    } else {
      eventSets["input_stalls"]   = {XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_PL, 
                                     XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_PL};
      eventSets["output_stalls"]  = {XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL, 
                                     XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_PL};
    }
    eventSets["mm2s_throughputs"] = eventSets["input_throughputs"];
    eventSets["s2mm_throughputs"] = eventSets["output_throughputs"];
    eventSets["mm2s_stalls"]      = eventSets["input_stalls"];
    eventSets["s2mm_stalls"]      = eventSets["output_stalls"];
    return eventSets;
  }

  /****************************************************************************
   * Get metric sets for memory tiles
   ***************************************************************************/
  std::map<std::string, std::vector<XAie_Events>> getMemoryTileEventSets()
  {
    std::map<std::string, std::vector<XAie_Events>> eventSets;
    eventSets = {
      {"input_channels",          {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, 
                                   XAIE_EVENT_PORT_STALLED_0_MEM_TILE,
                                   XAIE_EVENT_PORT_TLAST_0_MEM_TILE,   
                                   XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_BD_MEM_TILE}},
      {"input_channels_details",  {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE,
                                   XAIE_EVENT_DMA_S2MM_SEL0_MEMORY_BACKPRESSURE_MEM_TILE,  
                                   XAIE_EVENT_DMA_S2MM_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE,
                                   XAIE_EVENT_DMA_S2MM_SEL0_STREAM_STARVATION_MEM_TILE}},
      {"output_channels",         {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, 
                                   XAIE_EVENT_PORT_STALLED_0_MEM_TILE,
                                   XAIE_EVENT_PORT_TLAST_0_MEM_TILE,   
                                   XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_MEM_TILE}},
      {"output_channels_details", {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE,
                                   XAIE_EVENT_DMA_MM2S_SEL0_STREAM_BACKPRESSURE_MEM_TILE,
                                   XAIE_EVENT_DMA_MM2S_SEL0_MEMORY_STARVATION_MEM_TILE,
                                   XAIE_EVENT_DMA_MM2S_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE}},
      {"memory_stats",            {XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM_TILE,
                                   XAIE_EVENT_GROUP_ERRORS_MEM_TILE,
                                   XAIE_EVENT_GROUP_LOCK_MEM_TILE,
                                   XAIE_EVENT_GROUP_WATCHPOINT_MEM_TILE}},
      {"mem_trace",               {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, 
                                   XAIE_EVENT_PORT_STALLED_0_MEM_TILE,
                                   XAIE_EVENT_PORT_IDLE_0_MEM_TILE,
                                   XAIE_EVENT_PORT_TLAST_0_MEM_TILE}},
      {"input_throughputs",       {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE,
                                   XAIE_EVENT_DMA_S2MM_SEL0_STREAM_STARVATION_MEM_TILE,
                                   XAIE_EVENT_DMA_S2MM_SEL0_MEMORY_BACKPRESSURE_MEM_TILE,
                                   XAIE_EVENT_DMA_S2MM_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE}},
      {"output_throughputs",      {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, 
                                   XAIE_EVENT_DMA_MM2S_SEL0_STREAM_BACKPRESSURE_MEM_TILE,
                                   XAIE_EVENT_DMA_MM2S_SEL0_MEMORY_STARVATION_MEM_TILE,
                                   XAIE_EVENT_DMA_MM2S_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE}},
      {"conflict_stats1",         {XAIE_EVENT_CONFLICT_DM_BANK_0_MEM_TILE,
                                   XAIE_EVENT_CONFLICT_DM_BANK_1_MEM_TILE,
                                   XAIE_EVENT_CONFLICT_DM_BANK_2_MEM_TILE,
                                   XAIE_EVENT_CONFLICT_DM_BANK_3_MEM_TILE}},
      {"conflict_stats2",         {XAIE_EVENT_CONFLICT_DM_BANK_4_MEM_TILE,
                                   XAIE_EVENT_CONFLICT_DM_BANK_5_MEM_TILE,
                                   XAIE_EVENT_CONFLICT_DM_BANK_6_MEM_TILE,
                                   XAIE_EVENT_CONFLICT_DM_BANK_7_MEM_TILE}},
      {"conflict_stats3",         {XAIE_EVENT_CONFLICT_DM_BANK_8_MEM_TILE,
                                   XAIE_EVENT_CONFLICT_DM_BANK_9_MEM_TILE,
                                   XAIE_EVENT_CONFLICT_DM_BANK_10_MEM_TILE,
                                   XAIE_EVENT_CONFLICT_DM_BANK_11_MEM_TILE}}, 
      {"conflict_stats4",         {XAIE_EVENT_CONFLICT_DM_BANK_12_MEM_TILE,
                                   XAIE_EVENT_CONFLICT_DM_BANK_13_MEM_TILE,
                                   XAIE_EVENT_CONFLICT_DM_BANK_14_MEM_TILE,
                                   XAIE_EVENT_CONFLICT_DM_BANK_15_MEM_TILE}}
    };
    eventSets["s2mm_channels"]         = eventSets["input_channels"];
    eventSets["s2mm_channels_details"] = eventSets["input_channels_details"];
    eventSets["s2mm_throughputs"]      = eventSets["input_throughputs"];
    eventSets["mm2s_channels"]         = eventSets["output_channels"];
    eventSets["mm2s_channels_details"] = eventSets["output_channels_details"];
    eventSets["mm2s_throughputs"]      = eventSets["output_throughputs"];
    return eventSets;
  }

  /****************************************************************************
  * Modify configured events based on the channel and hardware generation
  ***************************************************************************/
  void modifyEvents(const module_type type, const io_type subtype, const uint8_t channel,
                                        std::vector<XAie_Events>& events, const int hwGen)
  {
    if ((type != module_type::dma) && (type != module_type::shim))
      return;
    
    // Memory modules
    if (type == module_type::dma) {
      // Modify events based on channel number
      if (channel > 0) {
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_MEM,        XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_MEM);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_MEM, XAIE_EVENT_DMA_S2MM_1_MEMORY_BACKPRESSURE_MEM);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_MEM, XAIE_EVENT_DMA_MM2S_1_STREAM_BACKPRESSURE_MEM);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_MEM,   XAIE_EVENT_DMA_MM2S_1_MEMORY_STARVATION_MEM);
      }
    }

    // Interface tiles

    // Calculate throughput differently for PLIO or AIE1 devices
    // since DMA-related events are not defined in those cases
    if ((subtype == io_type::PLIO) || (hwGen == 1)) {
      std::replace(events.begin(), events.end(), 
        XAIE_EVENT_GROUP_DMA_ACTIVITY_PL,              XAIE_EVENT_PORT_STALLED_0_PL);
      std::replace(events.begin(), events.end(), 
        XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_PL,  XAIE_EVENT_PORT_STALLED_0_PL);
      std::replace(events.begin(), events.end(), 
        XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_PL,    XAIE_EVENT_PORT_IDLE_0_PL);
      std::replace(events.begin(), events.end(), 
        XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL,  XAIE_EVENT_PORT_STALLED_0_PL);
      std::replace(events.begin(), events.end(), 
        XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_PL,         XAIE_EVENT_PORT_IDLE_0_PL);
    }

    // Modify events based on channel number
    if (channel > 0) {
      // Interface tiles
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL,  XAIE_EVENT_DMA_S2MM_1_MEMORY_BACKPRESSURE_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_PL,         XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_PL,  XAIE_EVENT_DMA_MM2S_1_STREAM_BACKPRESSURE_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_PL,    XAIE_EVENT_DMA_MM2S_1_MEMORY_STARVATION_PL);
    }
  }

  /****************************************************************************
   * Check if stream switch port event
   ***************************************************************************/
  bool isStreamSwitchPortEvent(const XAie_Events event)
  {
    if (isPortRunningEvent(event) || isPortStalledEvent(event) ||
        isPortIdleEvent(event) || isPortTlastEvent(event))
      return true;

    return false;
  }

  /****************************************************************************
   * Check if port running event
   ***************************************************************************/
  bool isPortRunningEvent(const XAie_Events event)
  {
    std::set<XAie_Events> runningEvents = {
      XAIE_EVENT_PORT_RUNNING_0_CORE,     XAIE_EVENT_PORT_RUNNING_1_CORE,
      XAIE_EVENT_PORT_RUNNING_2_CORE,     XAIE_EVENT_PORT_RUNNING_3_CORE,
      XAIE_EVENT_PORT_RUNNING_4_CORE,     XAIE_EVENT_PORT_RUNNING_5_CORE,
      XAIE_EVENT_PORT_RUNNING_6_CORE,     XAIE_EVENT_PORT_RUNNING_7_CORE,
      XAIE_EVENT_PORT_RUNNING_0_PL,       XAIE_EVENT_PORT_RUNNING_1_PL,
      XAIE_EVENT_PORT_RUNNING_2_PL,       XAIE_EVENT_PORT_RUNNING_3_PL,
      XAIE_EVENT_PORT_RUNNING_4_PL,       XAIE_EVENT_PORT_RUNNING_5_PL,
      XAIE_EVENT_PORT_RUNNING_6_PL,       XAIE_EVENT_PORT_RUNNING_7_PL,
      XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, XAIE_EVENT_PORT_RUNNING_1_MEM_TILE,
      XAIE_EVENT_PORT_RUNNING_2_MEM_TILE, XAIE_EVENT_PORT_RUNNING_3_MEM_TILE,
      XAIE_EVENT_PORT_RUNNING_4_MEM_TILE, XAIE_EVENT_PORT_RUNNING_5_MEM_TILE,
      XAIE_EVENT_PORT_RUNNING_6_MEM_TILE, XAIE_EVENT_PORT_RUNNING_7_MEM_TILE
    };

    return (runningEvents.find(event) != runningEvents.end());
  }

  /****************************************************************************
   * Check if port stalled event
   ***************************************************************************/
  bool isPortStalledEvent(const XAie_Events event)
  {
    std::set<XAie_Events> stalledEvents = {
      XAIE_EVENT_PORT_STALLED_0_CORE,     XAIE_EVENT_PORT_STALLED_1_CORE,
      XAIE_EVENT_PORT_STALLED_2_CORE,     XAIE_EVENT_PORT_STALLED_3_CORE,
      XAIE_EVENT_PORT_STALLED_4_CORE,     XAIE_EVENT_PORT_STALLED_5_CORE,
      XAIE_EVENT_PORT_STALLED_6_CORE,     XAIE_EVENT_PORT_STALLED_7_CORE,
      XAIE_EVENT_PORT_STALLED_0_PL,       XAIE_EVENT_PORT_STALLED_1_PL,
      XAIE_EVENT_PORT_STALLED_2_PL,       XAIE_EVENT_PORT_STALLED_3_PL,
      XAIE_EVENT_PORT_STALLED_4_PL,       XAIE_EVENT_PORT_STALLED_5_PL,
      XAIE_EVENT_PORT_STALLED_6_PL,       XAIE_EVENT_PORT_STALLED_7_PL,
      XAIE_EVENT_PORT_STALLED_0_MEM_TILE, XAIE_EVENT_PORT_STALLED_1_MEM_TILE,
      XAIE_EVENT_PORT_STALLED_2_MEM_TILE, XAIE_EVENT_PORT_STALLED_3_MEM_TILE,
      XAIE_EVENT_PORT_STALLED_4_MEM_TILE, XAIE_EVENT_PORT_STALLED_5_MEM_TILE,
      XAIE_EVENT_PORT_STALLED_6_MEM_TILE, XAIE_EVENT_PORT_STALLED_7_MEM_TILE
    };

    return (stalledEvents.find(event) != stalledEvents.end());
  }

  /****************************************************************************
   * Check if port idle event
   ***************************************************************************/
  bool isPortIdleEvent(const XAie_Events event)
  {
    std::set<XAie_Events> idleEvents = {
      XAIE_EVENT_PORT_IDLE_0_CORE,     XAIE_EVENT_PORT_IDLE_1_CORE,
      XAIE_EVENT_PORT_IDLE_2_CORE,     XAIE_EVENT_PORT_IDLE_3_CORE,
      XAIE_EVENT_PORT_IDLE_4_CORE,     XAIE_EVENT_PORT_IDLE_5_CORE,
      XAIE_EVENT_PORT_IDLE_6_CORE,     XAIE_EVENT_PORT_IDLE_7_CORE,
      XAIE_EVENT_PORT_IDLE_0_PL,       XAIE_EVENT_PORT_IDLE_1_PL,
      XAIE_EVENT_PORT_IDLE_2_PL,       XAIE_EVENT_PORT_IDLE_3_PL,
      XAIE_EVENT_PORT_IDLE_4_PL,       XAIE_EVENT_PORT_IDLE_5_PL,
      XAIE_EVENT_PORT_IDLE_6_PL,       XAIE_EVENT_PORT_IDLE_7_PL,
      XAIE_EVENT_PORT_IDLE_0_MEM_TILE, XAIE_EVENT_PORT_IDLE_1_MEM_TILE,
      XAIE_EVENT_PORT_IDLE_2_MEM_TILE, XAIE_EVENT_PORT_IDLE_3_MEM_TILE,
      XAIE_EVENT_PORT_IDLE_4_MEM_TILE, XAIE_EVENT_PORT_IDLE_5_MEM_TILE,
      XAIE_EVENT_PORT_IDLE_6_MEM_TILE, XAIE_EVENT_PORT_IDLE_7_MEM_TILE
    };

    return (idleEvents.find(event) != idleEvents.end());
  }

  /****************************************************************************
   * Check if port Tlast event
   ***************************************************************************/
  bool isPortTlastEvent(const XAie_Events event)
  {
    std::set<XAie_Events> tlastEvents = {
      XAIE_EVENT_PORT_TLAST_0_CORE,     XAIE_EVENT_PORT_TLAST_1_CORE,
      XAIE_EVENT_PORT_TLAST_2_CORE,     XAIE_EVENT_PORT_TLAST_3_CORE,
      XAIE_EVENT_PORT_TLAST_4_CORE,     XAIE_EVENT_PORT_TLAST_5_CORE,
      XAIE_EVENT_PORT_TLAST_6_CORE,     XAIE_EVENT_PORT_TLAST_7_CORE,
      XAIE_EVENT_PORT_TLAST_0_PL,       XAIE_EVENT_PORT_TLAST_1_PL,
      XAIE_EVENT_PORT_TLAST_2_PL,       XAIE_EVENT_PORT_TLAST_3_PL,
      XAIE_EVENT_PORT_TLAST_4_PL,       XAIE_EVENT_PORT_TLAST_5_PL,
      XAIE_EVENT_PORT_TLAST_6_PL,       XAIE_EVENT_PORT_TLAST_7_PL,
      XAIE_EVENT_PORT_TLAST_0_MEM_TILE, XAIE_EVENT_PORT_TLAST_1_MEM_TILE,
      XAIE_EVENT_PORT_TLAST_2_MEM_TILE, XAIE_EVENT_PORT_TLAST_3_MEM_TILE,
      XAIE_EVENT_PORT_TLAST_4_MEM_TILE, XAIE_EVENT_PORT_TLAST_5_MEM_TILE,
      XAIE_EVENT_PORT_TLAST_6_MEM_TILE, XAIE_EVENT_PORT_TLAST_7_MEM_TILE
    };

    return (tlastEvents.find(event) != tlastEvents.end());
  }

  uint8_t getPortNumberFromEvent(const XAie_Events event)
  {
    switch (event) {
    case XAIE_EVENT_PORT_RUNNING_7_CORE:
    case XAIE_EVENT_PORT_STALLED_7_CORE:
    case XAIE_EVENT_PORT_IDLE_7_CORE:
    case XAIE_EVENT_PORT_RUNNING_7_PL:
    case XAIE_EVENT_PORT_STALLED_7_PL:
    case XAIE_EVENT_PORT_IDLE_7_PL:
      return 7;
    case XAIE_EVENT_PORT_RUNNING_6_CORE:
    case XAIE_EVENT_PORT_STALLED_6_CORE:
    case XAIE_EVENT_PORT_IDLE_6_CORE:
    case XAIE_EVENT_PORT_RUNNING_6_PL:
    case XAIE_EVENT_PORT_STALLED_6_PL:
    case XAIE_EVENT_PORT_IDLE_6_PL:
      return 6;
    case XAIE_EVENT_PORT_RUNNING_5_CORE:
    case XAIE_EVENT_PORT_STALLED_5_CORE:
    case XAIE_EVENT_PORT_IDLE_5_CORE:
    case XAIE_EVENT_PORT_RUNNING_5_PL:
    case XAIE_EVENT_PORT_STALLED_5_PL:
    case XAIE_EVENT_PORT_IDLE_5_PL:
      return 5;
    case XAIE_EVENT_PORT_RUNNING_4_CORE:
    case XAIE_EVENT_PORT_STALLED_4_CORE:
    case XAIE_EVENT_PORT_IDLE_4_CORE:
    case XAIE_EVENT_PORT_RUNNING_4_PL:
    case XAIE_EVENT_PORT_STALLED_4_PL:
    case XAIE_EVENT_PORT_IDLE_4_PL:
      return 4;
    case XAIE_EVENT_PORT_RUNNING_3_CORE:
    case XAIE_EVENT_PORT_STALLED_3_CORE:
    case XAIE_EVENT_PORT_IDLE_3_CORE:
    case XAIE_EVENT_PORT_RUNNING_3_PL:
    case XAIE_EVENT_PORT_STALLED_3_PL:
    case XAIE_EVENT_PORT_IDLE_3_PL:
      return 3;
    case XAIE_EVENT_PORT_RUNNING_2_CORE:
    case XAIE_EVENT_PORT_STALLED_2_CORE:
    case XAIE_EVENT_PORT_IDLE_2_CORE:
    case XAIE_EVENT_PORT_RUNNING_2_PL:
    case XAIE_EVENT_PORT_STALLED_2_PL:
    case XAIE_EVENT_PORT_IDLE_2_PL:
      return 2;
    case XAIE_EVENT_PORT_RUNNING_1_CORE:
    case XAIE_EVENT_PORT_STALLED_1_CORE:
    case XAIE_EVENT_PORT_IDLE_1_CORE:
    case XAIE_EVENT_PORT_RUNNING_1_PL:
    case XAIE_EVENT_PORT_STALLED_1_PL:
    case XAIE_EVENT_PORT_IDLE_1_PL:
      return 1;
    default:
      return 0;
    }
  }


  /****************************************************************************
   * Get XAie module enum at the module index 
   ***************************************************************************/

  XAie_ModuleType getFalModuleType(const int moduleIndex)
  {
    return falModuleTypes[moduleIndex];
  }

  /****************************************************************************
   * Get base event number for a module
   ***************************************************************************/

  uint16_t getCounterBase(const xdp::module_type type)
  {
    return counterBases.at(type);
  }

  /****************************************************************************
   *  Check the match of the XAie enum module type with our xdp::module_type
   ***************************************************************************/
  bool isValidType(const module_type type, const XAie_ModuleType mod)
  {
    if ((mod == XAIE_CORE_MOD) && ((type == module_type::core) 
        || (type == module_type::dma)))
      return true;
    if ((mod == XAIE_MEM_MOD) && ((type == module_type::dma) 
        || (type == module_type::mem_tile)))
      return true;
    if ((mod == XAIE_PL_MOD) && (type == module_type::shim)) 
      return true;
    return false;
  }

  /****************************************************************************
   * Check if metric set is from Prof APIs Support
   ***************************************************************************/
  bool metricSupportsGraphIterator(std::string metricSet)
  {
    std::set<std::string> graphIterMetricSets = {
      "input_throughputs", "output_throughputs",
      METRIC_BYTE_COUNT
    };

    return graphIterMetricSets.find(metricSet) != graphIterMetricSets.end();
  }

  bool profileAPIMetricSet(const std::string metricSet)
  {
    // input_throughputs/output_throughputs is already supported, hence excluded here
    return adfApiMetricSetMap.find(metricSet) != adfApiMetricSetMap.end();
  }

  uint16_t getAdfApiReservedEventId(const std::string metricSet)
  {
    return adfApiMetricSetMap.at(metricSet);
  }

   /****************************************************************************
   * Get Interface tile broadcast channel and event 
   * This is in pre-defined order of using last broadcast event first to avoid
   * re-usage of same broadcast channel again in other plugin flows.
   * TODO: All plugin broadcast usage should only query to FAL
   ***************************************************************************/
  std::pair<int, XAie_Events> getPreferredPLBroadcastChannel()
  {
    static std::vector<XAie_Events> broadcastEvents = {
      XAIE_EVENT_BROADCAST_A_0_PL, XAIE_EVENT_BROADCAST_A_1_PL,
      XAIE_EVENT_BROADCAST_A_2_PL, XAIE_EVENT_BROADCAST_A_3_PL,
      XAIE_EVENT_BROADCAST_A_4_PL, XAIE_EVENT_BROADCAST_A_5_PL,
      XAIE_EVENT_BROADCAST_A_6_PL, XAIE_EVENT_BROADCAST_A_7_PL,
      XAIE_EVENT_BROADCAST_A_8_PL, XAIE_EVENT_BROADCAST_A_9_PL,
      XAIE_EVENT_BROADCAST_A_10_PL, XAIE_EVENT_BROADCAST_A_11_PL,
      XAIE_EVENT_BROADCAST_A_12_PL, XAIE_EVENT_BROADCAST_A_13_PL,
      XAIE_EVENT_BROADCAST_A_14_PL, XAIE_EVENT_BROADCAST_A_15_PL,
    };
  
    static int bcChannel = static_cast<int>(broadcastEvents.size()-1);
    
    if (bcChannel < 0)
      return {-1, XAIE_EVENT_NONE_CORE};
  
    std::pair<int, XAie_Events> bcPair = std::make_pair(bcChannel, broadcastEvents[bcChannel]);
    bcChannel--;
    return bcPair;
  }



  /****************************************************************************
   * Convert user specified bytes to beats for provided metric set
   ***************************************************************************/
  uint32_t convertToBeats(const std::string& metricSet, uint32_t bytes, uint8_t hw_gen)
  {
    if (metricSet != METRIC_BYTE_COUNT)
      return bytes;

    uint32_t streamWidth = aie::getStreamWidth(hw_gen);
    uint32_t total_beats = static_cast<uint32_t>(std::ceil(1.0 * bytes / streamWidth));

    // Note: As per run experiments on board, combo 3 FSM checks stop event in
    // every 4 clock cycles. We make sure stop event is in sync with FSM oddity
    // by rounding up total beats to nearest multiple of 4.
    uint32_t max_beats = 4 * ((total_beats + 3) / 4);
    return max_beats; 
  }

} // namespace xdp::aie
