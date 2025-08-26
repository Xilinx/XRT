// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved

#define XDP_PLUGIN_SOURCE

#include "xdp/profile/plugin/aie_trace/util/aie_trace_util.h"
#include "xdp/profile/plugin/aie_base/aie_base_util.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/device/pl_device_intf.h"
#include "xdp/profile/device/tracedefs.h"

#include "core/common/message.h"
#include "core/include/xrt/xrt_kernel.h"

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <regex>
#include <set>

// ***************************************************************
// Anonymous namespace for helper functions local to this file
// ***************************************************************
namespace xdp::aie::trace {
  using severity_level = xrt_core::message::severity_level;

  /****************************************************************************
   * Get metric sets for core modules
   * 
   * NOTE: These sets are supplemented with counter events as those are 
   *       dependent on counter number (AIE1 only).
   ***************************************************************************/
  std::map<std::string, std::vector<XAie_Events>> 
  getCoreEventSets(int hwGen)
  {
    std::map<std::string, std::vector<XAie_Events>> eventSets;
    eventSets = {
      {"functions",  {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE}}
    };

    eventSets["partial_stalls"]           = eventSets["functions"];
    eventSets["all_stalls"]               = eventSets["functions"];
    eventSets["all_dma"]                  = eventSets["functions"];
    eventSets["all_stalls_dma"]           = eventSets["functions"];
    eventSets["s2mm_channels"]            = eventSets["functions"];
    eventSets["mm2s_channels"]            = eventSets["functions"];
    eventSets["all_stalls_s2mm"]          = eventSets["functions"];
    eventSets["all_stalls_mm2s"]          = eventSets["functions"];

    if (!xdp::aie::isAIE1(hwGen)) {
      eventSets["s2mm_channels_stalls"]   = eventSets["functions"];
      eventSets["mm2s_channels_stalls"]   = eventSets["functions"];
    }

    return eventSets;
  }

  /****************************************************************************
   * Get metric sets for memory modules
   * 
   * NOTE 1: Core events listed here are broadcast by the resource manager.
   * NOTE 2: These sets are supplemented with counter events as those are 
   *         dependent on counter number (AIE1 only).
   ***************************************************************************/
  std::map<std::string, std::vector<XAie_Events>> 
  getMemoryEventSets(int hwGen)
  {
    std::map<std::string, std::vector<XAie_Events>> eventSets;
    eventSets = {
        {"functions", 
         {XAIE_EVENT_INSTR_CALL_CORE,                      XAIE_EVENT_INSTR_RETURN_CORE}},
        {"partial_stalls",
         {XAIE_EVENT_INSTR_CALL_CORE,                      XAIE_EVENT_INSTR_RETURN_CORE, 
          XAIE_EVENT_STREAM_STALL_CORE,                    XAIE_EVENT_CASCADE_STALL_CORE, 
          XAIE_EVENT_LOCK_STALL_CORE}},
        {"all_stalls",
         {XAIE_EVENT_INSTR_CALL_CORE,                      XAIE_EVENT_INSTR_RETURN_CORE, 
          XAIE_EVENT_MEMORY_STALL_CORE,                    XAIE_EVENT_STREAM_STALL_CORE, 
          XAIE_EVENT_CASCADE_STALL_CORE,                   XAIE_EVENT_LOCK_STALL_CORE}},
        {"all_dma",
         {XAIE_EVENT_INSTR_CALL_CORE,                      XAIE_EVENT_INSTR_RETURN_CORE,
          XAIE_EVENT_PORT_RUNNING_0_CORE,                  XAIE_EVENT_PORT_RUNNING_1_CORE,
          XAIE_EVENT_PORT_RUNNING_2_CORE,                  XAIE_EVENT_PORT_RUNNING_3_CORE}},
        {"all_stalls_s2mm",
         {XAIE_EVENT_INSTR_CALL_CORE,                      XAIE_EVENT_INSTR_RETURN_CORE,
          XAIE_EVENT_MEMORY_STALL_CORE,                    XAIE_EVENT_STREAM_STALL_CORE, 
          XAIE_EVENT_LOCK_STALL_CORE,                      XAIE_EVENT_PORT_RUNNING_0_CORE,
          XAIE_EVENT_PORT_RUNNING_1_CORE}},
        {"all_stalls_dma",
         {XAIE_EVENT_INSTR_CALL_CORE,                      XAIE_EVENT_INSTR_RETURN_CORE,
          XAIE_EVENT_GROUP_CORE_STALL_CORE,                XAIE_EVENT_PORT_RUNNING_0_CORE,
          XAIE_EVENT_PORT_RUNNING_1_CORE,                  XAIE_EVENT_PORT_RUNNING_2_CORE,
          XAIE_EVENT_PORT_RUNNING_3_CORE}},
        {"s2mm_channels",
         {XAIE_EVENT_INSTR_CALL_CORE,                      XAIE_EVENT_INSTR_RETURN_CORE,
          XAIE_EVENT_PORT_RUNNING_0_CORE,                  XAIE_EVENT_PORT_STALLED_0_CORE,
          XAIE_EVENT_PORT_RUNNING_1_CORE,                  XAIE_EVENT_PORT_STALLED_1_CORE}}
    };

    // Generation-specific sets
    //   * AIE2+ supports all eight trace events (AIE1 requires one for counter)
    //   * Sets w/ DMA stall/backpressure events not supported on AIE1
    if (xdp::aie::isAIE1(hwGen)) {
      eventSets["s2mm_channels_stalls"] = {};
      eventSets["mm2s_channels_stalls"] = {};
    }
    else if (xdp::aie::isAIE2(hwGen) || xdp::aie::isAIE2ps(hwGen)) {
      eventSets["all_stalls_s2mm"].push_back(XAIE_EVENT_CASCADE_STALL_CORE);

      eventSets["s2mm_channels_stalls"] =
         {XAIE_EVENT_DMA_S2MM_0_START_TASK_MEM,            XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM,
          XAIE_EVENT_DMA_S2MM_0_FINISHED_TASK_MEM,         XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_MEM,
          XAIE_EVENT_EDGE_DETECTION_EVENT_0_MEM,           XAIE_EVENT_EDGE_DETECTION_EVENT_1_MEM, 
          XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_MEM};
      eventSets["mm2s_channels_stalls"] =
         {XAIE_EVENT_DMA_MM2S_0_START_TASK_MEM,            XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM,
          XAIE_EVENT_DMA_MM2S_0_FINISHED_TASK_MEM,         XAIE_EVENT_EDGE_DETECTION_EVENT_0_MEM, 
          XAIE_EVENT_EDGE_DETECTION_EVENT_1_MEM,           XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_MEM,
          XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_MEM};
    }
    else if (xdp::aie::isNPU3(hwGen)) {
      eventSets["all_stalls_s2mm"].push_back(XAIE_EVENT_CASCADE_STALL_CORE);

      eventSets["s2mm_channels_stalls"] =
         {XAIE_EVENT_DMA_S2MM_0_START_BD_MEM,              XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM,
          XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_MEM,          XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_MEM,
          XAIE_EVENT_DMA_S2MM_1_START_BD_MEM,              XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM,
          XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_MEM,          XAIE_EVENT_DMA_S2MM_1_MEMORY_BACKPRESSURE_MEM};
      eventSets["mm2s_channels_stalls"] =
         {XAIE_EVENT_DMA_MM2S_0_START_BD_MEM,              XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM,
          XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_MEM,   XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_MEM};
    }

    eventSets["mm2s_channels"]   = eventSets["s2mm_channels"];
    eventSets["all_stalls_mm2s"] = eventSets["all_stalls_s2mm"];

    return eventSets;
  }

  /****************************************************************************
   * Get metric sets for memory tiles
   ***************************************************************************/
  std::map<std::string, std::vector<XAie_Events>> 
  getMemoryTileEventSets(int hwGen)
  {
    if (aie::isAIE1(hwGen))
      return {};
      
    std::map<std::string, std::vector<XAie_Events>> eventSets;
    eventSets = {
        {"input_channels",
         {XAIE_EVENT_DMA_S2MM_SEL0_START_TASK_MEM_TILE,    XAIE_EVENT_DMA_S2MM_SEL1_START_TASK_MEM_TILE,
          XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_BD_MEM_TILE,   XAIE_EVENT_DMA_S2MM_SEL1_FINISHED_BD_MEM_TILE,
          XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_TASK_MEM_TILE, XAIE_EVENT_DMA_S2MM_SEL1_FINISHED_TASK_MEM_TILE}},
        {"input_channels_stalls",
         {XAIE_EVENT_DMA_S2MM_SEL0_START_TASK_MEM_TILE,    XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_BD_MEM_TILE,
          XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_TASK_MEM_TILE, XAIE_EVENT_DMA_S2MM_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE,
          XAIE_EVENT_EDGE_DETECTION_EVENT_0_MEM_TILE,      XAIE_EVENT_EDGE_DETECTION_EVENT_1_MEM_TILE, 
          XAIE_EVENT_DMA_S2MM_SEL0_MEMORY_BACKPRESSURE_MEM_TILE}},
        {"output_channels",
         {XAIE_EVENT_DMA_MM2S_SEL0_START_TASK_MEM_TILE,    XAIE_EVENT_DMA_MM2S_SEL1_START_TASK_MEM_TILE,
          XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_MEM_TILE,   XAIE_EVENT_DMA_MM2S_SEL1_FINISHED_BD_MEM_TILE,
          XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_TASK_MEM_TILE, XAIE_EVENT_DMA_MM2S_SEL1_FINISHED_TASK_MEM_TILE}},
        {"output_channels_stalls",
         {XAIE_EVENT_DMA_MM2S_SEL0_START_TASK_MEM_TILE,    XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_MEM_TILE,
          XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_TASK_MEM_TILE, XAIE_EVENT_EDGE_DETECTION_EVENT_0_MEM_TILE, 
          XAIE_EVENT_EDGE_DETECTION_EVENT_1_MEM_TILE,      XAIE_EVENT_DMA_MM2S_SEL0_STREAM_BACKPRESSURE_MEM_TILE, 
          XAIE_EVENT_DMA_MM2S_SEL0_MEMORY_STARVATION_MEM_TILE}},
        {"memory_conflicts1",         
         {XAIE_EVENT_CONFLICT_DM_BANK_0_MEM_TILE,          XAIE_EVENT_CONFLICT_DM_BANK_1_MEM_TILE,
          XAIE_EVENT_CONFLICT_DM_BANK_2_MEM_TILE,          XAIE_EVENT_CONFLICT_DM_BANK_3_MEM_TILE,
          XAIE_EVENT_CONFLICT_DM_BANK_4_MEM_TILE,          XAIE_EVENT_CONFLICT_DM_BANK_5_MEM_TILE,
          XAIE_EVENT_CONFLICT_DM_BANK_6_MEM_TILE,          XAIE_EVENT_CONFLICT_DM_BANK_7_MEM_TILE}},
        {"memory_conflicts2",         
         {XAIE_EVENT_CONFLICT_DM_BANK_8_MEM_TILE,          XAIE_EVENT_CONFLICT_DM_BANK_9_MEM_TILE,
          XAIE_EVENT_CONFLICT_DM_BANK_10_MEM_TILE,         XAIE_EVENT_CONFLICT_DM_BANK_11_MEM_TILE,
          XAIE_EVENT_CONFLICT_DM_BANK_12_MEM_TILE,         XAIE_EVENT_CONFLICT_DM_BANK_13_MEM_TILE,
          XAIE_EVENT_CONFLICT_DM_BANK_14_MEM_TILE,         XAIE_EVENT_CONFLICT_DM_BANK_15_MEM_TILE}}
    };

    // Banks 16-23 are not defined for all generations
    if (aie::isNPU3(hwGen)) {
#ifdef XDP_NPU3_BUILD
      eventSets["memory_conflicts3"] = {
          XAIE_EVENT_CONFLICT_DM_BANK_16_MEM_TILE,         XAIE_EVENT_CONFLICT_DM_BANK_17_MEM_TILE,
          XAIE_EVENT_CONFLICT_DM_BANK_18_MEM_TILE,         XAIE_EVENT_CONFLICT_DM_BANK_19_MEM_TILE,
          XAIE_EVENT_CONFLICT_DM_BANK_20_MEM_TILE,         XAIE_EVENT_CONFLICT_DM_BANK_21_MEM_TILE,
          XAIE_EVENT_CONFLICT_DM_BANK_22_MEM_TILE,         XAIE_EVENT_CONFLICT_DM_BANK_23_MEM_TILE};
#endif
    }
    else {
      eventSets["memory_conflicts3"] = {};
    }

    eventSets["s2mm_channels"]        = eventSets["input_channels"];
    eventSets["s2mm_channels_stalls"] = eventSets["input_channels_stalls"];
    eventSets["mm2s_channels"]        = eventSets["output_channels"];
    eventSets["mm2s_channels_stalls"] = eventSets["output_channels_stalls"];
    return eventSets;
  }

  /****************************************************************************
   * Get metric sets for interface tiles
   ***************************************************************************/
  std::map<std::string, std::vector<XAie_Events>> 
  getInterfaceTileEventSets(int hwGen)
  {
    std::map<std::string, std::vector<XAie_Events>> eventSets;
    eventSets = {
        {"input_ports",
         {XAIE_EVENT_PORT_RUNNING_0_PL,                      XAIE_EVENT_PORT_RUNNING_1_PL,
          XAIE_EVENT_PORT_RUNNING_2_PL,                      XAIE_EVENT_PORT_RUNNING_3_PL}},
        {"output_ports",
         {XAIE_EVENT_PORT_RUNNING_0_PL,                      XAIE_EVENT_PORT_RUNNING_1_PL,
          XAIE_EVENT_PORT_RUNNING_2_PL,                      XAIE_EVENT_PORT_RUNNING_3_PL}},
        {"input_output_ports",
         {XAIE_EVENT_PORT_RUNNING_0_PL,                      XAIE_EVENT_PORT_RUNNING_1_PL,
          XAIE_EVENT_PORT_RUNNING_2_PL,                      XAIE_EVENT_PORT_RUNNING_3_PL}},
        {"input_ports_stalls",
         {XAIE_EVENT_PORT_RUNNING_0_PL,                      XAIE_EVENT_PORT_STALLED_0_PL,
          XAIE_EVENT_PORT_RUNNING_1_PL,                      XAIE_EVENT_PORT_STALLED_1_PL}},
        {"output_ports_stalls",
        {XAIE_EVENT_PORT_RUNNING_0_PL,                       XAIE_EVENT_PORT_STALLED_0_PL,
         XAIE_EVENT_PORT_RUNNING_1_PL,                       XAIE_EVENT_PORT_STALLED_1_PL}},
        {"input_output_ports_stalls",
         {XAIE_EVENT_PORT_RUNNING_0_PL,                      XAIE_EVENT_PORT_STALLED_0_PL,
          XAIE_EVENT_PORT_RUNNING_1_PL,                      XAIE_EVENT_PORT_STALLED_1_PL,
          XAIE_EVENT_PORT_RUNNING_2_PL,                      XAIE_EVENT_PORT_STALLED_2_PL,
          XAIE_EVENT_PORT_RUNNING_3_PL,                      XAIE_EVENT_PORT_STALLED_3_PL}}
    };

    if (aie::isAIE1(hwGen)) {
      eventSets["input_ports_details"] = {
          XAIE_EVENT_DMA_MM2S_0_START_BD_PL,                 XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_PL,
          XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_ACQUIRE_PL,
          XAIE_EVENT_DMA_MM2S_1_START_BD_PL,                 XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_PL,
          XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_ACQUIRE_PL};
      eventSets["output_ports_details"] = {
          XAIE_EVENT_DMA_S2MM_0_START_BD_PL,                 XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_PL,
          XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_ACQUIRE_PL,
          XAIE_EVENT_DMA_S2MM_1_START_BD_PL,                 XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_PL,
          XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_ACQUIRE_PL};
    }
    else if (aie::isAIE2(hwGen)) {
      eventSets["input_ports_details"] = {
          XAIE_EVENT_DMA_MM2S_0_START_TASK_PL,               XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_PL,
          XAIE_EVENT_DMA_MM2S_0_FINISHED_TASK_PL,            XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_PL,
          XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_PL,      XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_PL};
      eventSets["output_ports_details"] = {
          XAIE_EVENT_DMA_S2MM_0_START_TASK_PL,               XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_PL,
          XAIE_EVENT_DMA_S2MM_0_FINISHED_TASK_PL,            XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_PL,
          XAIE_EVENT_DMA_S2MM_0_STREAM_STARVATION_PL,        XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL};
    }
    else if (aie::isAIE2ps(hwGen)) {
#ifndef XDP_CLIENT_BUILD
      eventSets["input_ports_details"] = {
          XAIE_EVENT_NOC0_DMA_MM2S_0_START_TASK_PL,          XAIE_EVENT_NOC0_DMA_MM2S_0_FINISHED_BD_PL,
          XAIE_EVENT_NOC0_DMA_MM2S_0_FINISHED_TASK_PL,       XAIE_EVENT_NOC0_DMA_MM2S_0_STALLED_LOCK_PL,
          XAIE_EVENT_NOC0_DMA_MM2S_0_STREAM_BACKPRESSURE_PL, XAIE_EVENT_NOC0_DMA_MM2S_0_MEMORY_STARVATION_PL};
      eventSets["output_ports_details"] = {
          XAIE_EVENT_NOC0_DMA_S2MM_0_START_TASK_PL,          XAIE_EVENT_NOC0_DMA_S2MM_0_FINISHED_BD_PL,
          XAIE_EVENT_NOC0_DMA_S2MM_0_FINISHED_TASK_PL,       XAIE_EVENT_NOC0_DMA_S2MM_0_STALLED_LOCK_PL,
          XAIE_EVENT_NOC0_DMA_S2MM_0_STREAM_STARVATION_PL,   XAIE_EVENT_NOC0_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL};
#endif
    }
    else if (aie::isNPU3(hwGen)) {
#ifdef XDP_NPU3_BUILD
      eventSets["input_ports_details"] = {
          XAIE_EVENT_DMA_MM2S_SEL0_START_BD_PL,              XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_PL,
          XAIE_EVENT_DMA_MM2S_SEL0_STREAM_BACKPRESSURE_PL,   XAIE_EVENT_DMA_MM2S_SEL0_MEMORY_STARVATION_PL,
          XAIE_EVENT_DMA_MM2S_SEL1_START_BD_PL,              XAIE_EVENT_DMA_MM2S_SEL1_FINISHED_BD_PL,
          XAIE_EVENT_DMA_MM2S_SEL1_STREAM_BACKPRESSURE_PL,   XAIE_EVENT_DMA_MM2S_SEL1_MEMORY_STARVATION_PL};
      eventSets["output_ports_details"] = {
          XAIE_EVENT_DMA_S2MM_0_START_BD_PL,                 XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_PL,
          XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_PL,             XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL,
          XAIE_EVENT_DMA_S2MM_1_START_BD_PL,                 XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_PL,
          XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_PL,             XAIE_EVENT_DMA_S2MM_1_MEMORY_BACKPRESSURE_PL};
#endif
    }

    // Microcontroller sets
    if (aie::isMicroSupported(hwGen)) {
#ifndef XDP_CLIENT_BUILD
    if (aie::isAIE2ps(hwGen)) {
      eventSets["uc_axis"] = {
          XAIE_EVENT_CORE_AXIS_MASTER_RUNNING_UC,            XAIE_EVENT_CORE_AXIS_MASTER_STALLED_UC,
	        XAIE_EVENT_CORE_AXIS_SLAVE_RUNNING_UC,             XAIE_EVENT_CORE_AXIS_SLAVE_STALLED_UC};
      eventSets["uc_program_flow"] = {
          XAIE_EVENT_CORE_REG_WRITE_UC,                      XAIE_EVENT_CORE_EXCEPTION_TAKEN_UC,
	        XAIE_EVENT_CORE_JUMP_TAKEN_UC,                     XAIE_EVENT_CORE_DATA_READ_UC,
	        XAIE_EVENT_CORE_DATA_WRITE_UC,                     XAIE_EVENT_CORE_STREAM_GET_UC,
	        XAIE_EVENT_CORE_STREAM_PUT_UC};
      eventSets["uc_dma"] = {
          XAIE_EVENT_DMA_DM2MM_START_TASK_UC,                XAIE_EVENT_DMA_DM2MM_FINISHED_BD_UC,
          XAIE_EVENT_DMA_DM2MM_FINISHED_TASK_UC,             XAIE_EVENT_DMA_MM2DM_START_TASK_UC,
          XAIE_EVENT_DMA_MM2DM_FINISHED_BD_UC,               XAIE_EVENT_DMA_MM2DM_FINISHED_TASK_UC};
      eventSets["uc_dma_dm2mm"] = {
          XAIE_EVENT_DMA_DM2MM_START_TASK_UC,                XAIE_EVENT_DMA_DM2MM_FINISHED_BD_UC,
          XAIE_EVENT_DMA_DM2MM_FINISHED_TASK_UC,             XAIE_EVENT_DMA_DM2MM_LOCAL_MEMORY_STARVATION_UC,
	        XAIE_EVENT_DMA_DM2MM_REMOTE_MEMORY_BACKPRESSURE_UC};
      eventSets["uc_dma_mm2dm"] = {
          XAIE_EVENT_DMA_MM2DM_START_TASK_UC,                XAIE_EVENT_DMA_MM2DM_FINISHED_BD_UC,
	        XAIE_EVENT_DMA_MM2DM_FINISHED_TASK_UC,             XAIE_EVENT_DMA_MM2DM_LOCAL_MEMORY_STARVATION_UC,
	        XAIE_EVENT_DMA_MM2DM_REMOTE_MEMORY_BACKPRESSURE_UC};
    }
#elif XDP_NPU3_BUILD
      eventSets["uc_axis"] = {
          XAIE_EVENT_CORE_AXIS_MASTER_RUNNING_UC,            XAIE_EVENT_CORE_AXIS_MASTER_STALLED_UC,
	        XAIE_EVENT_CORE_AXIS_SLAVE_RUNNING_UC,             XAIE_EVENT_CORE_AXIS_SLAVE_STALLED_UC};
      eventSets["uc_program_flow"] = {
          XAIE_EVENT_CORE_REG_WRITE_UC,                      XAIE_EVENT_CORE_EXCEPTION_TAKEN_UC,
	        XAIE_EVENT_CORE_JUMP_TAKEN_UC,                     XAIE_EVENT_CORE_DATA_READ_UC,
	        XAIE_EVENT_CORE_DATA_WRITE_UC,                     XAIE_EVENT_CORE_STREAM_GET_UC,
	        XAIE_EVENT_CORE_STREAM_PUT_UC};
      eventSets["uc_dma"] = {
          XAIE_EVENT_DMA_DM2MM_A_START_BD_UC,                XAIE_EVENT_DMA_DM2MM_A_FINISHED_BD_UC,
          XAIE_EVENT_DMA_DM2MM_A_LOCAL_MEMORY_STARVATION_UC, XAIE_EVENT_DMA_DM2MM_A_REMOTE_MEMORY_BACKPRESSURE_UC,
          XAIE_EVENT_DMA_MM2DM_A_START_BD_UC,                XAIE_EVENT_DMA_MM2DM_A_FINISHED_BD_UC,
	        XAIE_EVENT_DMA_MM2DM_A_REMOTE_MEMORY_STARVATION_UC,XAIE_EVENT_DMA_MM2DM_A_LOCAL_MEMORY_BACKPRESSURE_UC};
      eventSets["uc_dma_dm2mm"] = {};
      eventSets["uc_dma_mm2dm"] = {};
#endif
    }
    else {
      eventSets["uc_dma_dm2mm"] = {};
      eventSets["uc_dma_mm2dm"] = {};
      eventSets["uc_axis"] = {};
      eventSets["uc_program_flow"] = {};
    }

    eventSets["mm2s_ports"]             = eventSets["input_ports"];
    eventSets["s2mm_ports"]             = eventSets["output_ports"];
    eventSets["mm2s_s2mm_ports"]        = eventSets["input_output_ports"];
    eventSets["mm2s_ports_stalls"]      = eventSets["input_ports_stalls"];
    eventSets["s2mm_ports_stalls"]      = eventSets["output_ports_stalls"];
    eventSets["mm2s_s2mm_ports_stalls"] = eventSets["input_output_ports_stalls"];
    eventSets["mm2s_ports_details"]     = eventSets["input_ports_details"];
    eventSets["s2mm_ports_details"]     = eventSets["output_ports_details"];
    return eventSets;
  }

  /****************************************************************************
   * Get start events for core module counters
   ***************************************************************************/
  std::vector<XAie_Events> getCoreCounterStartEvents(int hwGen, std::string scheme)
  {
    if (!aie::isAIE1(hwGen))
      return {};

    std::vector<XAie_Events> startEvents;
    if (scheme == "es1")
      startEvents = {XAIE_EVENT_ACTIVE_CORE, XAIE_EVENT_ACTIVE_CORE};
    else if (scheme == "es2")
      startEvents = {XAIE_EVENT_ACTIVE_CORE};
    return startEvents;
  }
  
  /****************************************************************************
   * Get end events for core module counters
   ***************************************************************************/
  std::vector<XAie_Events> getCoreCounterEndEvents(int hwGen, std::string scheme)
  {
    if (!aie::isAIE1(hwGen))
      return {};

    std::vector<XAie_Events> endEvents;
    if (scheme == "es1")
      endEvents = {XAIE_EVENT_DISABLED_CORE, XAIE_EVENT_DISABLED_CORE};
    else if (scheme == "es2")
      endEvents = {XAIE_EVENT_DISABLED_CORE};
    return endEvents;
  }
  
  /****************************************************************************
   * Get event values for core module counters
   * 
   * NOTE: These counters are required HW workarounds with thresholds chosen
   *       to produce events before hitting the bug. For example, sync packets
   *       occur after 1024 cycles and with no events, is incorrectly repeated.
   ***************************************************************************/
  std::vector<uint32_t> getCoreCounterEventValues(int hwGen, std::string scheme)
  {
    if (!aie::isAIE1(hwGen))
      return {};

    std::vector<uint32_t> eventValues;
    if (scheme == "es1")
      eventValues = {ES1_TRACE_COUNTER, ES1_TRACE_COUNTER * ES1_TRACE_COUNTER};
    else if (scheme == "es2")
      eventValues = {ES2_TRACE_COUNTER};
    return eventValues;
  }

  /****************************************************************************
   * Get start events for memory module counters
   ***************************************************************************/
  std::vector<XAie_Events> getMemoryCounterStartEvents(int hwGen, std::string scheme)
  {
    if (!aie::isAIE1(hwGen))
      return {};

    std::vector<XAie_Events> startEvents;
    if (scheme == "es1")
      startEvents = {XAIE_EVENT_TRUE_MEM, XAIE_EVENT_TRUE_MEM};
    else if (scheme == "es2")
      startEvents = {XAIE_EVENT_TRUE_MEM};
    return startEvents;
  }
  
  /****************************************************************************
   * Get end events for memory module counters
   ***************************************************************************/
  std::vector<XAie_Events> getMemoryCounterEndEvents(int hwGen, std::string scheme)
  {
    if (!aie::isAIE1(hwGen))
      return {};

    std::vector<XAie_Events> endEvents;
    if (scheme == "es1") 
      endEvents = {XAIE_EVENT_NONE_MEM, XAIE_EVENT_NONE_MEM};
    else if (scheme == "es2")
      endEvents = {XAIE_EVENT_NONE_MEM};
    return endEvents;
  }

  /****************************************************************************
   * Get event values for memory module counters
   * 
   * NOTE: These counters are required HW workarounds with thresholds chosen
   *       to produce events before hitting the bug. For example, sync packets
   *       occur after 1024 cycles and with no events, is incorrectly repeated.
   ***************************************************************************/
  std::vector<uint32_t> getMemoryCounterEventValues(int hwGen, std::string scheme)
  {
    if (!aie::isAIE1(hwGen))
      return {};

    std::vector<uint32_t> eventValues;
    if (scheme == "es1")
      eventValues = {ES1_TRACE_COUNTER, ES1_TRACE_COUNTER * ES1_TRACE_COUNTER};
    else if (scheme == "es2")
      eventValues = {ES2_TRACE_COUNTER};
    return eventValues;
  }

  /****************************************************************************
   * Print out reserved trace events
   ***************************************************************************/
  void printTraceEventStats(int m, int numTiles[])
  {
    if (!aie::isInfoVerbosity())
      return;

    auto modName = getModuleName(static_cast<module_type>(m));

    std::stringstream msg;
    msg << "AIE trace events reserved in " << modName << " - ";
    for (uint32_t n = 0; n <= NUM_TRACE_EVENTS; ++n) {
      if (numTiles[n] == 0)
        continue;
      msg << n << ": " << numTiles[n] << " tiles, ";
    }

    xrt_core::message::send(severity_level::info, "XRT", msg.str());
  }

  /****************************************************************************
   * Modify events in metric set based on type and channel
   ***************************************************************************/
  void modifyEvents(module_type type, io_type subtype, const std::string metricSet,
                    uint8_t channel, std::vector<XAie_Events>& events, const int hwGen)
  {
    // Only needed for GMIO DMA channel 1
    if ((type != module_type::shim) || (subtype == io_type::PLIO) || (channel == 0))
      return;

    // Check type to minimize replacements
    if (aie::isInputSet(type, metricSet)) {
      // Input or MM2S
      if (aie::isAIE2ps(hwGen)) {
#ifndef XDP_CLIENT_BUILD
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_NOC0_DMA_MM2S_0_START_TASK_PL,          XAIE_EVENT_NOC0_DMA_MM2S_1_START_TASK_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_NOC0_DMA_MM2S_0_FINISHED_BD_PL,         XAIE_EVENT_NOC0_DMA_MM2S_1_FINISHED_BD_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_NOC0_DMA_MM2S_0_FINISHED_TASK_PL,       XAIE_EVENT_NOC0_DMA_MM2S_1_FINISHED_TASK_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_NOC0_DMA_MM2S_0_STALLED_LOCK_PL,        XAIE_EVENT_NOC0_DMA_MM2S_1_STALLED_LOCK_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_NOC0_DMA_MM2S_0_STREAM_BACKPRESSURE_PL, XAIE_EVENT_NOC0_DMA_MM2S_1_STREAM_BACKPRESSURE_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_NOC0_DMA_MM2S_0_MEMORY_STARVATION_PL,   XAIE_EVENT_NOC0_DMA_MM2S_1_MEMORY_STARVATION_PL);
#endif
      } else {
        // Applicable for Edge Versal and client builds
        // NOTE: NPU3 build need to be handled separately if required
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_DMA_MM2S_0_START_TASK_PL,               XAIE_EVENT_DMA_MM2S_1_START_TASK_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_PL,              XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_DMA_MM2S_0_FINISHED_TASK_PL,            XAIE_EVENT_DMA_MM2S_1_FINISHED_TASK_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_PL,             XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_PL,      XAIE_EVENT_DMA_MM2S_1_STREAM_BACKPRESSURE_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_PL,        XAIE_EVENT_DMA_MM2S_1_MEMORY_STARVATION_PL);
      }
    }
    else {
      // Output or S2MM
      if (aie::isAIE2ps(hwGen)) {
#ifndef XDP_CLIENT_BUILD
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_NOC0_DMA_S2MM_0_START_TASK_PL,          XAIE_EVENT_NOC0_DMA_S2MM_1_START_TASK_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_NOC0_DMA_S2MM_0_FINISHED_BD_PL,         XAIE_EVENT_NOC0_DMA_S2MM_1_FINISHED_BD_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_NOC0_DMA_S2MM_0_FINISHED_TASK_PL,       XAIE_EVENT_NOC0_DMA_S2MM_1_FINISHED_TASK_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_NOC0_DMA_S2MM_0_STALLED_LOCK_PL,        XAIE_EVENT_NOC0_DMA_S2MM_1_STALLED_LOCK_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_NOC0_DMA_S2MM_0_STREAM_STARVATION_PL,   XAIE_EVENT_NOC0_DMA_S2MM_1_STREAM_STARVATION_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_NOC0_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL, XAIE_EVENT_NOC0_DMA_S2MM_1_MEMORY_BACKPRESSURE_PL);
#endif
      } else {
        // Applicable for Edge Versal and client builds
        // NOTE: NPU3 build need to be handled separately if required
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_DMA_S2MM_0_START_TASK_PL,               XAIE_EVENT_DMA_S2MM_1_START_TASK_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_PL,              XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_DMA_S2MM_0_FINISHED_TASK_PL,            XAIE_EVENT_DMA_S2MM_1_FINISHED_TASK_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_PL,             XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_DMA_S2MM_0_STREAM_STARVATION_PL,        XAIE_EVENT_DMA_S2MM_1_STREAM_STARVATION_PL);
        std::replace(events.begin(), events.end(), 
            XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL,      XAIE_EVENT_DMA_S2MM_1_MEMORY_BACKPRESSURE_PL);
      }
    }
  }

  /****************************************************************************
   * Configure group events (core modules only)
   ***************************************************************************/
  void configGroupEvents(XAie_DevInst* aieDevInst, const XAie_LocType loc,
                         const XAie_ModuleType mod, const module_type type, 
                         const std::string metricSet)
  {
    // Only needed for core module and metric sets that include DMA events
    if (!aie::isDmaSet(metricSet) || (type != module_type::core))
      return;

    // Set masks for group events
    XAie_EventGroupControl(aieDevInst, loc, mod, XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE, 
                           GROUP_CORE_FUNCTIONS_MASK);
    XAie_EventGroupControl(aieDevInst, loc, mod, XAIE_EVENT_GROUP_CORE_STALL_CORE, 
                           GROUP_CORE_STALL_MASK);
    XAie_EventGroupControl(aieDevInst, loc, mod, XAIE_EVENT_GROUP_STREAM_SWITCH_CORE, 
                           GROUP_STREAM_SWITCH_RUNNING_MASK);
  }

  /****************************************************************************
   * Configure event selection (memory tiles only)
   ***************************************************************************/
  void configEventSelections(XAie_DevInst* aieDevInst, const tile_type& tile,
                             const XAie_LocType loc, const module_type type, 
                             const std::string metricSet, const uint8_t channel0, 
                             const uint8_t channel1, aie_cfg_base& config)
  {
    if (type != module_type::mem_tile)
      return;

    XAie_DmaDirection dmaDir = aie::isInputSet(type, metricSet) ? DMA_S2MM : DMA_MM2S;

    if (aie::isDebugVerbosity()) {
      std::string typeName = (dmaDir == DMA_S2MM) ? "S2MM" : "MM2S";
      std::string msg = "Configuring event selections for DMA " + typeName + " channels "
                      + std::to_string(channel0) + " and " + std::to_string(channel1);
      xrt_core::message::send(severity_level::debug, "XRT", msg);
    }

    XAie_EventSelectDmaChannel(aieDevInst, loc, 0, dmaDir, channel0);
    XAie_EventSelectDmaChannel(aieDevInst, loc, 1, dmaDir, channel1);

    // Record for runtime config file
    config.port_trace_ids[0] = channel0;
    config.port_trace_ids[1] = channel1;
    if (aie::isInputSet(type, metricSet)) {
      config.port_trace_is_master[0] = true;
      config.port_trace_is_master[1] = true;
      config.s2mm_channels[0] = channel0;
      if (channel0 < tile.s2mm_names.size())
        config.s2mm_names[0] = tile.s2mm_names[channel0];
      if (channel0 != channel1) {
        config.s2mm_channels[1] = channel1;
      if (channel1 < tile.s2mm_names.size())
        config.s2mm_names[1] = tile.s2mm_names[channel1];
      }
    } 
    else {
      config.port_trace_is_master[0] = false;
      config.port_trace_is_master[1] = false;
      config.mm2s_channels[0] = channel0;
      if (channel0 < tile.mm2s_names.size())
        config.mm2s_names[0] = tile.mm2s_names[channel0];
      if (channel0 != channel1) {
        config.mm2s_channels[1] = channel1;
        if (channel1 < tile.mm2s_names.size())
          config.mm2s_names[1] = tile.mm2s_names[channel1];
        }
      }
  }

  /****************************************************************************
   * Configure edge detection events
   ***************************************************************************/
  void configEdgeEvents(XAie_DevInst* aieDevInst, const tile_type& tile,
                        const module_type type, const std::string metricSet, 
                        const XAie_Events event, const uint8_t channel)
  {
    if ((event != XAIE_EVENT_EDGE_DETECTION_EVENT_0_MEM_TILE)
        && (event != XAIE_EVENT_EDGE_DETECTION_EVENT_1_MEM_TILE)
        && (event != XAIE_EVENT_EDGE_DETECTION_EVENT_0_MEM)
        && (event != XAIE_EVENT_EDGE_DETECTION_EVENT_1_MEM))
      return;

    // Catch memory tiles
    if (type == module_type::mem_tile) {
      // Event is DMA_S2MM_Sel0_stream_starvation or DMA_MM2S_Sel0_stalled_lock
      uint16_t eventNum = aie::isInputSet(type, metricSet)
          ? EVENT_MEM_TILE_DMA_S2MM_SEL0_STREAM_STARVATION
          : EVENT_MEM_TILE_DMA_MM2S_SEL0_STALLED_LOCK;

      // Register Edge_Detection_event_control
      // 26    Event 1 triggered on falling edge
      // 25    Event 1 triggered on rising edge
      // 23:16 Input event for edge event 1
      // 10    Event 0 triggered on falling edge
      //  9    Event 0 triggered on rising edge
      //  7:0  Input event for edge event 0
      uint32_t edgeEventsValue = (1 << 26) + (eventNum << 16) + (1 << 9) + eventNum;

      xrt_core::message::send(severity_level::debug, "XRT",
          "Configuring memory tile edge events to detect rise and fall of event " 
          + std::to_string(eventNum));

      auto tileOffset = XAie_GetTileAddr(aieDevInst, tile.row, tile.col);
      XAie_Write32(aieDevInst, tileOffset + AIE_OFFSET_EDGE_CONTROL_MEM_TILE, 
                   edgeEventsValue);
      return;
    }

    // Below is AIE tile support
    
    // Event is DMA_MM2S_stalled_lock or DMA_S2MM_stream_starvation
    uint16_t eventNum = aie::isInputSet(type, metricSet)
        ? ((channel == 0) ? EVENT_MEM_DMA_MM2S_0_STALLED_LOCK
                          : EVENT_MEM_DMA_MM2S_1_STALLED_LOCK)
        : ((channel == 0) ? EVENT_MEM_DMA_S2MM_0_STREAM_STARVATION
                          : EVENT_MEM_DMA_S2MM_1_STREAM_STARVATION);

    // Register Edge_Detection_event_control
    // 26    Event 1 triggered on falling edge
    // 25    Event 1 triggered on rising edge
    // 23:16 Input event for edge event 1
    // 10    Event 0 triggered on falling edge
    //  9    Event 0 triggered on rising edge
    //  7:0  Input event for edge event 0
    uint32_t edgeEventsValue = (1 << 26) + (eventNum << 16) + (1 << 9) + eventNum;

    xrt_core::message::send(severity_level::debug, "XRT", 
        "Configuring AIE tile edge events to detect rise and fall of event " 
        + std::to_string(eventNum));

    auto tileOffset = XAie_GetTileAddr(aieDevInst, tile.row, tile.col);
    XAie_Write32(aieDevInst, tileOffset + AIE_OFFSET_EDGE_CONTROL_MEM, 
                 edgeEventsValue);
  }

  /****************************************************************************
   * Set up broadcast network 
   ***************************************************************************/
  void build2ChannelBroadcastNetwork(XAie_DevInst* aieDevInst, std::shared_ptr<AieTraceMetadata> metadata,
                                    uint8_t broadcastId1, uint8_t broadcastId2, XAie_Events event,
                                    uint8_t startCol, uint8_t numCols, uint8_t numRows)
  {
    XAie_Events bcastEvent2_PL =  (XAie_Events) (XAIE_EVENT_BROADCAST_A_0_PL + broadcastId2);
    XAie_EventBroadcast(aieDevInst, XAie_TileLoc(startCol, 0), XAIE_PL_MOD, broadcastId2, event);

    for(uint8_t col = startCol; col < startCol + numCols; col++) {
      for(uint8_t row = 0; row < numRows; row++) {
        module_type tileType = aie::getModuleType(row, metadata->getRowOffset());
        auto loc = XAie_TileLoc(col, row);

        if(tileType == module_type::shim) {
          // first channel is only used to send north
          if(col == startCol) {
            XAie_EventBroadcast(aieDevInst, loc, XAIE_PL_MOD, broadcastId1, event);
          }
          else {
            XAie_EventBroadcast(aieDevInst, loc, XAIE_PL_MOD, broadcastId1, bcastEvent2_PL);
          }
          if(row != numRows-1) {
            XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST);
          }
          else {
            XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST | XAIE_EVENT_BROADCAST_NORTH);
          }

          // second channel is only used to send east
          XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId2, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH);

          if(col != startCol + numCols - 1) {
            XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, broadcastId2, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH);
          }
          else {
            XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, broadcastId2, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_EAST);
          }
        }
        else if(tileType == module_type::mem_tile) {
          if(row != numRows-1) {
            XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_MEM_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST);
          }
          else {
            XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_MEM_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST | XAIE_EVENT_BROADCAST_NORTH);
          }
        }
        else { //core tile
          if(row != numRows-1) {
            XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_CORE_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST);
          }
          else {
            XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_CORE_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH);
          }
          XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_MEM_MOD,  XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST | XAIE_EVENT_BROADCAST_NORTH);
        }
      }
    }
  }

  /****************************************************************************
   * Reset broadcast network
   ***************************************************************************/
  void reset2ChannelBroadcastNetwork(XAie_DevInst* aieDevInst, std::shared_ptr<AieTraceMetadata> metadata,
                                    uint8_t broadcastId1, uint8_t broadcastId2, uint8_t startCol,
                                    uint8_t numCols, uint8_t numRows) {

    XAie_EventBroadcastReset(aieDevInst, XAie_TileLoc(startCol, 0), XAIE_PL_MOD, broadcastId2);

    for(uint8_t col = startCol; col < startCol + numCols; col++) {
      for(uint8_t row = 0; row < numRows; row++) {
        module_type tileType = aie::getModuleType(row, metadata->getRowOffset());
        auto loc = XAie_TileLoc(col, row);

        if(tileType == module_type::shim) {
          XAie_EventBroadcastReset(aieDevInst, loc, XAIE_PL_MOD, broadcastId1);
          XAie_EventBroadcastUnblockDir(aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_ALL);
          XAie_EventBroadcastUnblockDir(aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId2, XAIE_EVENT_BROADCAST_ALL);
          XAie_EventBroadcastUnblockDir(aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, broadcastId2, XAIE_EVENT_BROADCAST_ALL);
        }
        else if(tileType == module_type::mem_tile) {
          XAie_EventBroadcastUnblockDir(aieDevInst, loc, XAIE_MEM_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_ALL);
        }
        else { //core tile
          XAie_EventBroadcastUnblockDir(aieDevInst, loc, XAIE_CORE_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_ALL);
          XAie_EventBroadcastUnblockDir(aieDevInst, loc, XAIE_MEM_MOD,  XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_ALL);
        }
      }
    }
  }

} // namespace xdp::aie::trace
