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
    }
#ifdef XDP_VE2_BUILD
    else if (hwGen == 5) {
      eventSets["input_stalls"]   = {XAIE_EVENT_NOC0_DMA_MM2S_0_STREAM_BACKPRESSURE_PL, 
                                     XAIE_EVENT_NOC0_DMA_MM2S_0_MEMORY_STARVATION_PL};
      eventSets["output_stalls"]  = {XAIE_EVENT_NOC0_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL, 
                                     XAIE_EVENT_NOC0_DMA_S2MM_0_STALLED_LOCK_PL};
      eventSets["input_throughputs"] = {XAIE_EVENT_NOC0_GROUP_DMA_ACTIVITY_PL, XAIE_EVENT_PORT_RUNNING_0_PL};
      eventSets["output_throughputs"] = {XAIE_EVENT_NOC0_GROUP_DMA_ACTIVITY_PL, XAIE_EVENT_PORT_RUNNING_0_PL};
    }
#endif
    else {
      eventSets["input_stalls"]   = {XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_PL, 
                                     XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_PL};
      eventSets["output_stalls"]  = {XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL, 
                                     XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_PL};
    }

    // Microcontroller sets
    if (hwGen >= 5) {
#ifdef XDP_CLIENT_BUILD
      // eventSets["uc_dma_activity"] = {
      //     XAIE_EVENT_UC_DMA_DM2MM_A_FINISHED_BD,           XAIE_EVENT_UC_DMA_DM2MM_A_LOCAL_MEMORY_STARVATION,
	    //     XAIE_EVENT_UC_DMA_DM2MM_A_REMOTE_MEMORY_BACKPRESSURE,
      //     XAIE_EVENT_UC_DMA_MM2DM_A_FINISHED_BD,           XAIE_EVENT_UC_DMA_MM2DM_A_LOCAL_MEMORY_STARVATION,
	    //     XAIE_EVENT_UC_DMA_MM2DM_A_REMOTE_MEMORY_BACKPRESSURE};
      // eventSets["uc_axis_throughputs"] = {
      //     XAIE_EVENT_UC_CORE_AXIS_MASTER_RUNNING,          XAIE_EVENT_UC_CORE_AXIS_MASTER_STALLED,
      //     XAIE_EVENT_UC_CORE_AXIS_MASTER_TLAST,
	    //     XAIE_EVENT_UC_CORE_AXIS_SLAVE_RUNNING,           XAIE_EVENT_UC_CORE_AXIS_SLAVE_STALLED,
      //     XAIE_EVENT_UC_CORE_AXIS_SLAVE_TLAST};
      // eventSets["uc_core"] = {
      //     XAIE_EVENT_UC_CORE_REG_WRITE,                    XAIE_EVENT_UC_CORE_JUMP_TAKEN,
      //     XAIE_EVENT_UC_CORE_DATA_READ,                    XAIE_EVENT_UC_CORE_DATA_WRITE,
      //     XAIE_EVENT_UC_CORE_STREAM_GET,                   XAIE_EVENT_UC_CORE_STREAM_PUT};
      eventSets["uc_dma_activity"] = {};
      eventSets["uc_axis_throughputs"] = {};
      eventSets["uc_core"] = {};
#else
      eventSets["uc_dma_activity"] = {
          XAIE_EVENT_DMA_DM2MM_FINISHED_BD_UC,             XAIE_EVENT_DMA_DM2MM_LOCAL_MEMORY_STARVATION_UC,
	        XAIE_EVENT_DMA_DM2MM_REMOTE_MEMORY_BACKPRESSURE_UC,
          XAIE_EVENT_DMA_MM2DM_FINISHED_BD_UC,             XAIE_EVENT_DMA_MM2DM_LOCAL_MEMORY_STARVATION_UC,
	        XAIE_EVENT_DMA_MM2DM_REMOTE_MEMORY_BACKPRESSURE_UC};
      eventSets["uc_axis_throughputs"] = {
          XAIE_EVENT_CORE_AXIS_MASTER_RUNNING_UC,          XAIE_EVENT_CORE_AXIS_MASTER_STALLED_UC,
          XAIE_EVENT_CORE_AXIS_MASTER_TLAST_UC,
	        XAIE_EVENT_CORE_AXIS_SLAVE_RUNNING_UC,           XAIE_EVENT_CORE_AXIS_SLAVE_STALLED_UC,
          XAIE_EVENT_CORE_AXIS_SLAVE_TLAST_UC};
      eventSets["uc_core"] = {
          XAIE_EVENT_CORE_REG_WRITE_UC,                    XAIE_EVENT_CORE_JUMP_TAKEN_UC,
          XAIE_EVENT_CORE_DATA_READ_UC,                    XAIE_EVENT_CORE_DATA_WRITE_UC,
          XAIE_EVENT_CORE_STREAM_GET_UC,                   XAIE_EVENT_CORE_STREAM_PUT_UC};
#endif
    }
    else {
      eventSets["uc_dma_activity"] = {};
      eventSets["uc_axis_throughputs"] = {};
      eventSets["uc_core"] = {};
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
  std::map<std::string, std::vector<XAie_Events>> getMemoryTileEventSets(const int hwGen)
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
                                   XAIE_EVENT_DMA_MM2S_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE}}
    };

    if (hwGen < 40) {
      eventSets["conflict_stats1"] = {
        XAIE_EVENT_CONFLICT_DM_BANK_0_MEM_TILE,            XAIE_EVENT_CONFLICT_DM_BANK_1_MEM_TILE,
        XAIE_EVENT_CONFLICT_DM_BANK_2_MEM_TILE,            XAIE_EVENT_CONFLICT_DM_BANK_3_MEM_TILE};
      eventSets["conflict_stats2"] = {
        XAIE_EVENT_CONFLICT_DM_BANK_4_MEM_TILE,            XAIE_EVENT_CONFLICT_DM_BANK_5_MEM_TILE,
        XAIE_EVENT_CONFLICT_DM_BANK_6_MEM_TILE,            XAIE_EVENT_CONFLICT_DM_BANK_7_MEM_TILE};
      eventSets["conflict_stats3"] = {
        XAIE_EVENT_CONFLICT_DM_BANK_8_MEM_TILE,            XAIE_EVENT_CONFLICT_DM_BANK_9_MEM_TILE,
        XAIE_EVENT_CONFLICT_DM_BANK_10_MEM_TILE,           XAIE_EVENT_CONFLICT_DM_BANK_11_MEM_TILE};
      eventSets["conflict_stats4"] = {
        XAIE_EVENT_CONFLICT_DM_BANK_12_MEM_TILE,           XAIE_EVENT_CONFLICT_DM_BANK_13_MEM_TILE,
        XAIE_EVENT_CONFLICT_DM_BANK_14_MEM_TILE,           XAIE_EVENT_CONFLICT_DM_BANK_15_MEM_TILE};
    } else {
      eventSets["conflict_stats1"] = {
        XAIE_EVENT_CONFLICT_DM_BANK_0_MEM_TILE,            XAIE_EVENT_CONFLICT_DM_BANK_1_MEM_TILE,
        XAIE_EVENT_CONFLICT_DM_BANK_2_MEM_TILE,            XAIE_EVENT_CONFLICT_DM_BANK_3_MEM_TILE,
        XAIE_EVENT_CONFLICT_DM_BANK_4_MEM_TILE,            XAIE_EVENT_CONFLICT_DM_BANK_5_MEM_TILE,
        XAIE_EVENT_CONFLICT_DM_BANK_6_MEM_TILE,            XAIE_EVENT_CONFLICT_DM_BANK_7_MEM_TILE,
        XAIE_EVENT_CONFLICT_DM_BANK_8_MEM_TILE,            XAIE_EVENT_CONFLICT_DM_BANK_9_MEM_TILE,
        XAIE_EVENT_CONFLICT_DM_BANK_10_MEM_TILE,           XAIE_EVENT_CONFLICT_DM_BANK_11_MEM_TILE};
#ifdef XDP_CLIENT_BUILD
      // Banks 16-23 are not defined for all generations
      // eventSets["conflict_stats2"] = {
      //   XAIE_EVENT_CONFLICT_DM_BANK_12_MEM_TILE,           XAIE_EVENT_CONFLICT_DM_BANK_13_MEM_TILE,
      //   XAIE_EVENT_CONFLICT_DM_BANK_14_MEM_TILE,           XAIE_EVENT_CONFLICT_DM_BANK_15_MEM_TILE,
      //   XAIE_EVENT_CONFLICT_DM_BANK_16_MEM_TILE,           XAIE_EVENT_CONFLICT_DM_BANK_17_MEM_TILE,
      //   XAIE_EVENT_CONFLICT_DM_BANK_18_MEM_TILE,           XAIE_EVENT_CONFLICT_DM_BANK_19_MEM_TILE,
      //   XAIE_EVENT_CONFLICT_DM_BANK_20_MEM_TILE,           XAIE_EVENT_CONFLICT_DM_BANK_21_MEM_TILE,
      //   XAIE_EVENT_CONFLICT_DM_BANK_22_MEM_TILE,           XAIE_EVENT_CONFLICT_DM_BANK_23_MEM_TILE};
#else
      eventSets["conflict_stats2"] = {
        XAIE_EVENT_CONFLICT_DM_BANK_12_MEM_TILE,           XAIE_EVENT_CONFLICT_DM_BANK_13_MEM_TILE,
        XAIE_EVENT_CONFLICT_DM_BANK_14_MEM_TILE,           XAIE_EVENT_CONFLICT_DM_BANK_15_MEM_TILE};
#endif
      eventSets["conflict_stats3"] = {};
      eventSets["conflict_stats4"] = {};
    }

    eventSets["s2mm_channels"]         = eventSets["input_channels"];
    eventSets["s2mm_channels_details"] = eventSets["input_channels_details"];
    eventSets["s2mm_throughputs"]      = eventSets["input_throughputs"];
    eventSets["mm2s_channels"]         = eventSets["output_channels"];
    eventSets["mm2s_channels_details"] = eventSets["output_channels_details"];
    eventSets["mm2s_throughputs"]      = eventSets["output_throughputs"];
    return eventSets;
  }

  /****************************************************************************
   * Get metric sets for microcontrollers
   * TODO: convert to XAie_Events once support is available from driver
   ***************************************************************************/
  //std::map<std::string, std::vector<XAie_Events>> getMicrocontrollerEventSets(const int hwGen)
  std::map<std::string, std::vector<uint32_t>> getMicrocontrollerEventSets(const int hwGen)
  {
    //std::map<std::string, std::vector<XAie_Events>> eventSets;
    std::map<std::string, std::vector<uint32_t>> eventSets;
    if (hwGen < 5)
      return eventSets;

    // TODO: replace with enums once driver supports the MDM
    eventSets = {
      {"execution",               {16, 17, 18, 19, 20, 62}},
      {"interrupt_stalls",        {23, 24, 25, 26, 27, 57}},
      {"mmu_activity",            {43, 48, 49, 50, 53, 61}},
      {"test",                    { 0,  0,  0,  0,  0,  0}}
    };

    return eventSets;
  }

  /****************************************************************************
   * Configure the individual AIE events for metric sets that use group events
   ***************************************************************************/
  void configGroupEvents(XAie_DevInst* aieDevInst, const XAie_LocType loc,
                        const XAie_ModuleType mod, const module_type type,
                        const std::string metricSet, const XAie_Events event,
                        const uint8_t channel) 
  {
    // Set masks for group events
    // NOTE: Group error enable register is blocked, so ignoring
    if (event == XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_DMA_MASK);
    else if (event == XAIE_EVENT_GROUP_LOCK_MEM)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_LOCK_MASK);
    else if (event == XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_CONFLICT_MASK);
    else if (event == XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_CORE_PROGRAM_FLOW_MASK);
    else if (event == XAIE_EVENT_GROUP_CORE_STALL_CORE)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_CORE_STALL_MASK);
    else if (event == XAIE_EVENT_GROUP_DMA_ACTIVITY_PL) {
      uint32_t bitMask = aie::isInputSet(type, metricSet) 
          ? ((channel == 0) ? GROUP_SHIM_S2MM0_STALL_MASK : GROUP_SHIM_S2MM1_STALL_MASK)
          : ((channel == 0) ? GROUP_SHIM_MM2S0_STALL_MASK : GROUP_SHIM_MM2S1_STALL_MASK);
      XAie_EventGroupControl(aieDevInst, loc, mod, event, bitMask);
    }                                    
  }

  /****************************************************************************
  * Configure the selection index to monitor channel number in memory tiles
  ***************************************************************************/
  void configEventSelections(XAie_DevInst* aieDevInst, const XAie_LocType loc,
                            const module_type type, const std::string metricSet,
                            const uint8_t channel)
  {
    if (type != module_type::mem_tile)
      return;

    XAie_DmaDirection dmaDir = aie::isInputSet(type, metricSet) ? DMA_S2MM : DMA_MM2S;
    XAie_EventSelectDmaChannel(aieDevInst, loc, 0, dmaDir, channel);

    std::stringstream msg;
    msg << "Configured mem tile " << (aie::isInputSet(type,metricSet) ? "S2MM" : "MM2S") 
    << "DMA  for metricset " << metricSet << ", channel " << (int)channel << ".";
    xrt_core::message::send(severity_level::debug, "XRT", msg.str());
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
#ifdef XDP_VE2_BUILD
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_NOC0_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL,  XAIE_EVENT_NOC0_DMA_S2MM_1_MEMORY_BACKPRESSURE_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_NOC0_DMA_S2MM_0_STALLED_LOCK_PL,         XAIE_EVENT_NOC0_DMA_S2MM_1_STALLED_LOCK_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_NOC0_DMA_MM2S_0_STREAM_BACKPRESSURE_PL,  XAIE_EVENT_NOC0_DMA_MM2S_1_STREAM_BACKPRESSURE_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_NOC0_DMA_MM2S_0_MEMORY_STARVATION_PL,    XAIE_EVENT_NOC0_DMA_MM2S_1_MEMORY_STARVATION_PL);
#else
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL,       XAIE_EVENT_DMA_S2MM_1_MEMORY_BACKPRESSURE_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_PL,              XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_PL,       XAIE_EVENT_DMA_MM2S_1_STREAM_BACKPRESSURE_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_PL,         XAIE_EVENT_DMA_MM2S_1_MEMORY_STARVATION_PL);
#endif
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

  /****************************************************************************
   * Check if profile API metric set
   ***************************************************************************/
  bool profileAPIMetricSet(const std::string metricSet)
  {
    // input_throughputs/output_throughputs is already supported, hence excluded here
    return adfApiMetricSetMap.find(metricSet) != adfApiMetricSetMap.end();
  }

  /****************************************************************************
   * Get event ID associated with metric set
   ***************************************************************************/
  uint16_t getAdfApiReservedEventId(const std::string metricSet)
  {
    return adfApiMetricSetMap.at(metricSet);
  }

  /****************************************************************************
   * Get physical event IDs for metric set
   ***************************************************************************/
  std::pair<uint16_t, uint16_t>
  getEventPhysicalId(XAie_DevInst* aieDevInst, XAie_LocType& tileLoc,
                     XAie_ModuleType& xaieModType, module_type xdpModType,
                     const std::string& metricSet, XAie_Events startEvent, 
                     XAie_Events endEvent)
  {
    if (profileAPIMetricSet(metricSet)) {
      uint16_t eventId = getAdfApiReservedEventId(metricSet);
      return std::make_pair(eventId, eventId);
    }
    
    uint16_t tmpStart;
    uint16_t tmpEnd;
    XAie_EventLogicalToPhysicalConv(aieDevInst, tileLoc, xaieModType, startEvent, &tmpStart);
    XAie_EventLogicalToPhysicalConv(aieDevInst, tileLoc, xaieModType,   endEvent, &tmpEnd);
    
    uint16_t phyStartEvent = tmpStart + getCounterBase(xdpModType);
    uint16_t phyEndEvent   = tmpEnd   + getCounterBase(xdpModType);
    return std::make_pair(phyStartEvent, phyEndEvent);
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
  uint32_t convertToBeats(const std::string& metricSet, uint32_t bytes, uint8_t hwGen)
  {
    if (metricSet != METRIC_BYTE_COUNT)
      return bytes;

    uint32_t streamWidth = aie::getStreamWidth(hwGen);
    uint32_t total_beats = static_cast<uint32_t>(std::ceil(1.0 * bytes / streamWidth));

    return total_beats;
  }

  /****************************************************************************
   * Configure counters in Microblaze Debug Module (MDM)
   * TODO: convert to using XAie_Events once support is available from driver
   ***************************************************************************/
  void configMDMCounters(XAie_DevInst* aieDevInst, int hwGen, uint8_t col, uint8_t row, 
                         const std::vector<uint32_t> events)
  {
    // Ensure supported generation and not privileged
    if (hwGen < 5) 
      return;

    uint32_t val;
    static bool showWarning = true;
    auto tileOffset = XAie_GetTileAddr(aieDevInst, row, col);
    XAie_Read32(aieDevInst, tileOffset + UC_MEMORY_PRIVILEGED, &val);

    if (val) {
      if (showWarning) {
        xrt_core::message::send(severity_level::warning, "XRT", 
          "Unable to configure MDM counters since debug port is privileged.");
        showWarning = false;
      }
      return;
    }
    
    //
    // Use MDM protocol to configure counters
    //
    // 1. Reset to first counter
    XAie_Write32(aieDevInst, tileOffset + UC_MDM_PCCMDR, 1 << UC_MDM_PCCMDR_RESET_BIT);
    
    // 2. Write events for all counters
    for (auto event : events)
      XAie_Write32(aieDevInst, tileOffset + UC_MDM_PCCTRLR, event);

    // 3. Clear and start counters
    XAie_Write32(aieDevInst, tileOffset + UC_MDM_PCCMDR, 1 << UC_MDM_PCCMDR_CLEAR_BIT);
    XAie_Write32(aieDevInst, tileOffset + UC_MDM_PCCMDR, 1 << UC_MDM_PCCMDR_START_BIT);
  }

  /****************************************************************************
   * Read counters in Microblaze Debug Module (MDM)
   ***************************************************************************/
  void readMDMCounters(XAie_DevInst* aieDevInst, int hwGen, uint8_t col, uint8_t row, 
                       std::vector<uint64_t>& values)
  {
    // Ensure supported generation and not privileged
    if (hwGen < 5) 
      return;

    uint32_t val;
    static bool showWarning = true;
    auto tileOffset = XAie_GetTileAddr(aieDevInst, row, col);
    XAie_Read32(aieDevInst, tileOffset + UC_MEMORY_PRIVILEGED, &val);

    if (val) {
      if (showWarning) {
        xrt_core::message::send(severity_level::warning, "XRT", 
          "Unable to read MDM counters since debug port is privileged.");
        showWarning = false;
      }
      return;
    }

    //
    // Use MDM protocol to read counters
    //
    // 1. Sample and stop counters
    XAie_Write32(aieDevInst, tileOffset + UC_MDM_PCCMDR, 1 << UC_MDM_PCCMDR_SAMPLE_BIT);
    // NOTE: Do counters need to be stopped before reading? If so, uncomment line below.
    //XAie_Write32(aieDevInst, tileOffset + UC_MDM_PCCMDR, 1 << UC_MDM_PCCMDR_STOP_BIT);

    // 2. Reset to first counter
    XAie_Write32(aieDevInst, tileOffset + UC_MDM_PCCMDR, 1 << UC_MDM_PCCMDR_RESET_BIT);

    // 3. Read status of all counters
    std::vector<bool> overflows;
    uint32_t numCounters = UC_NUM_EVENT_COUNTERS + UC_NUM_LATENCY_COUNTERS;
    for (uint32_t c=0; c < numCounters; ++c) {
      uint32_t val1;
      XAie_Read32(aieDevInst, tileOffset + UC_MDM_PCSR, &val1);
      bool overflow = (((val1 >> UC_MDM_PCSR_OVERFLOW_BIT) & 0x1) == 1);
      overflows.push_back(overflow);

      if ((val1 >> UC_MDM_PCSR_FULL_BIT) & 0x1) {
        std::cout << "Full bit of tile " << col << "," << row << " microcontroller counter " 
                  << c << " is high" << std::endl;
      }
    }

    // 4. Reset to first counter
    XAie_Write32(aieDevInst, tileOffset + UC_MDM_PCCMDR, 1 << UC_MDM_PCCMDR_RESET_BIT);

    // 5. Read values of event counters
    for (uint32_t c=0; c < UC_NUM_EVENT_COUNTERS; ++c) {
      uint32_t val1;
      XAie_Read32(aieDevInst, tileOffset + UC_MDM_PCDRR, &val1);
      uint64_t val2 = (overflows.at(c)) ? (val1 + OVERFLOW_32BIT) : val1;
      values.push_back(val2);
    }

    // 6. Read four values from latency counter
    //    Read 1 - The number of times the event occurred
    //    Read 2 - The sum of each event latency
    //    Read 3 - The sum of each event latency squared
    //    Read 4 - 31:16 Minimum measured latency, 16 bits
    //             15:0  Maximum measured latency, 16 bits
    std::vector<uint64_t> latencyValues;
    for (uint32_t c=0; c < UC_MDM_PCDRR_LATENCY_READS; ++c) {
      uint32_t val1;
      XAie_Read32(aieDevInst, tileOffset + UC_MDM_PCDRR, &val1);
      uint64_t val2 = (overflows.at(UC_NUM_EVENT_COUNTERS)) ? (val1 + OVERFLOW_32BIT) : val1;
      latencyValues.push_back(val2);
    }

    // 7. Calculate average latency
    // NOTE: for now, only report average (we also have min and max; see above)
    auto numValues = latencyValues.at(0);
    auto totalLatency = latencyValues.at(1);
    uint64_t avgLatency = (numValues == 0) ? 0 : (totalLatency / numValues);
    values.push_back(avgLatency);
  }

} // namespace xdp::aie
