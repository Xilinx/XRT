/* Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

// This file contains helper structures used by the AIE event trace config
// PS kernel.

#ifndef EVENT_CONFIGURATION_DOT_H
#define EVENT_CONFIGURATION_DOT_H

#include <memory>
#include <vector>
#include "xaiefal/xaiefal.hpp"
#include "xaiengine.h"
#include "xdp/profile/plugin/aie_trace_new/x86/aie_trace_kernel_config.h"

// This struct encapsulates all of the internal configuration information
// for a single AIE tile
struct EventConfiguration {
  std::map<std::string, std::vector<XAie_Events>> mCoreStartEvents;
  std::map<std::string, std::vector<XAie_Events>> mCoreEndEvents;
  std::map<std::string, std::vector<XAie_Events>> mMemoryStartEvents;
  std::map<std::string, std::vector<XAie_Events>> mMemoryEndEvents;
  std::map<std::string, std::vector<XAie_Events>> mShimStartEvents;
  std::map<std::string, std::vector<XAie_Events>> mShimEndEvents;
  // std::vector<XAie_Events> coreEventsBase;
  // std::vector<XAie_Events> memoryCrossEventsBase;
  // XAie_Events coreTraceStartEvent = XAIE_EVENT_ACTIVE_CORE;
  // XAie_Events coreTraceEndEvent = XAIE_EVENT_DISABLED_CORE;
  // std::vector<XAie_Events> coreCounterStartEvents;
  // std::vector<XAie_Events> coreCounterEndEvents;
  // std::vector<uint32_t> coreCounterEventValues;
  // std::vector<XAie_Events> memoryCounterStartEvents;
  // std::vector<XAie_Events> memoryCounterEndEvents;
  // std::vector<uint32_t> memoryCounterEventValues;

  // std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> mCoreCounters;
  // std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> mMemoryCounters;
  
  void initialize(const xdp::built_in::InputConfiguration* params) {
    mCoreStartEvents = {
      {"heat_map",              {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_GROUP_CORE_STALL_CORE,
                                 XAIE_EVENT_INSTR_VECTOR_CORE,         XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE}},
      {"stalls",                {XAIE_EVENT_MEMORY_STALL_CORE,         XAIE_EVENT_STREAM_STALL_CORE,
                                 XAIE_EVENT_LOCK_STALL_CORE,           XAIE_EVENT_CASCADE_STALL_CORE}},
      {"execution",             {XAIE_EVENT_INSTR_VECTOR_CORE,         XAIE_EVENT_INSTR_LOAD_CORE,
                                 XAIE_EVENT_INSTR_STORE_CORE,          XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE}},
      {"floating_point",        {XAIE_EVENT_FP_OVERFLOW_CORE,          XAIE_EVENT_FP_UNDERFLOW_CORE,
                                 XAIE_EVENT_FP_INVALID_CORE,           XAIE_EVENT_FP_DIV_BY_ZERO_CORE}},
      {"stream_put_get",        {XAIE_EVENT_INSTR_CASCADE_GET_CORE,    XAIE_EVENT_INSTR_CASCADE_PUT_CORE,
                                 XAIE_EVENT_INSTR_STREAM_GET_CORE,     XAIE_EVENT_INSTR_STREAM_PUT_CORE}},
      {"write_bandwidths",      {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_INSTR_STREAM_PUT_CORE,
                                 XAIE_EVENT_INSTR_CASCADE_PUT_CORE,    XAIE_EVENT_GROUP_CORE_STALL_CORE}},
      {"read_bandwidths",       {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_INSTR_STREAM_GET_CORE,
                                 XAIE_EVENT_INSTR_CASCADE_GET_CORE,    XAIE_EVENT_GROUP_CORE_STALL_CORE}},
      {"aie_trace",             {XAIE_EVENT_PORT_RUNNING_1_CORE,       XAIE_EVENT_PORT_STALLED_1_CORE,
                                 XAIE_EVENT_PORT_RUNNING_0_CORE,       XAIE_EVENT_PORT_STALLED_0_CORE}},
      {"events",                {XAIE_EVENT_INSTR_EVENT_0_CORE,        XAIE_EVENT_INSTR_EVENT_1_CORE,
                                 XAIE_EVENT_USER_EVENT_0_CORE,         XAIE_EVENT_USER_EVENT_1_CORE}}
    };
    mCoreEndEvents = {
      {"heat_map",              {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_GROUP_CORE_STALL_CORE,
                                 XAIE_EVENT_INSTR_VECTOR_CORE,         XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE}},
      {"stalls",                {XAIE_EVENT_MEMORY_STALL_CORE,         XAIE_EVENT_STREAM_STALL_CORE,
                                 XAIE_EVENT_LOCK_STALL_CORE,           XAIE_EVENT_CASCADE_STALL_CORE}},
      {"execution",             {XAIE_EVENT_INSTR_VECTOR_CORE,         XAIE_EVENT_INSTR_LOAD_CORE,
                                 XAIE_EVENT_INSTR_STORE_CORE,          XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE}},
      {"floating_point",        {XAIE_EVENT_FP_OVERFLOW_CORE,          XAIE_EVENT_FP_UNDERFLOW_CORE,
                                 XAIE_EVENT_FP_INVALID_CORE,           XAIE_EVENT_FP_DIV_BY_ZERO_CORE}},
      {"stream_put_get",        {XAIE_EVENT_INSTR_CASCADE_GET_CORE,    XAIE_EVENT_INSTR_CASCADE_PUT_CORE,
                                 XAIE_EVENT_INSTR_STREAM_GET_CORE,     XAIE_EVENT_INSTR_STREAM_PUT_CORE}},
      {"write_bandwidths",      {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_INSTR_STREAM_PUT_CORE,
                                 XAIE_EVENT_INSTR_CASCADE_PUT_CORE,    XAIE_EVENT_GROUP_CORE_STALL_CORE}},
      {"read_bandwidths",       {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_INSTR_STREAM_GET_CORE,
                                 XAIE_EVENT_INSTR_CASCADE_GET_CORE,    XAIE_EVENT_GROUP_CORE_STALL_CORE}},
      {"aie_trace",             {XAIE_EVENT_PORT_RUNNING_1_CORE,       XAIE_EVENT_PORT_STALLED_1_CORE,
                                 XAIE_EVENT_PORT_RUNNING_0_CORE,       XAIE_EVENT_PORT_STALLED_0_CORE}},
      {"events",                {XAIE_EVENT_INSTR_EVENT_0_CORE,        XAIE_EVENT_INSTR_EVENT_1_CORE,
                                 XAIE_EVENT_USER_EVENT_0_CORE,         XAIE_EVENT_USER_EVENT_1_CORE}}
    };

    // **** Memory Module Counters ****
    mMemoryStartEvents = {
      {"conflicts",             {XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM, XAIE_EVENT_GROUP_ERRORS_MEM}},
      {"dma_locks",             {XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM,    XAIE_EVENT_GROUP_LOCK_MEM}},
      {"dma_stalls_s2mm",       {XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_ACQUIRE_MEM,
                                 XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_ACQUIRE_MEM}},
      {"dma_stalls_mm2s",       {XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_ACQUIRE_MEM,
                                 XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_ACQUIRE_MEM}},
      {"write_bandwidths",      {XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM,
                                 XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM}},
      {"read_bandwidths",       {XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM,
                                 XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM}}
    };
    mMemoryEndEvents = {
      {"conflicts",             {XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM, XAIE_EVENT_GROUP_ERRORS_MEM}},
      {"dma_locks",             {XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM,    XAIE_EVENT_GROUP_LOCK_MEM}}, 
      {"dma_stalls_s2mm",       {XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_ACQUIRE_MEM,
                                 XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_ACQUIRE_MEM}},
      {"dma_stalls_mm2s",       {XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_ACQUIRE_MEM,
                                 XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_ACQUIRE_MEM}},
      {"write_bandwidths",      {XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM,
                                 XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM}},
      {"read_bandwidths",       {XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM,
                                 XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM}}
    };

    // **** PL/Shim Counters ****
    mShimStartEvents = {
      {"input_bandwidths",      {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_STALLED_0_PL}},
      {"output_bandwidths",     {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_STALLED_0_PL}},
      {"packets",               {XAIE_EVENT_PORT_TLAST_0_PL,   XAIE_EVENT_PORT_TLAST_1_PL}}
    };
    mShimEndEvents = {
      {"input_bandwidths",      {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_STALLED_0_PL}},
      {"output_bandwidths",     {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_STALLED_0_PL}},
      {"packets",               {XAIE_EVENT_PORT_TLAST_0_PL,   XAIE_EVENT_PORT_TLAST_1_PL}}
    };

    // String event values for guidance and output
    mCoreEventStrings = {
      {"heat_map",              {"ACTIVE_CORE",               "GROUP_CORE_STALL_CORE",
                                 "INSTR_VECTOR_CORE",         "GROUP_CORE_PROGRAM_FLOW"}},
      {"stalls",                {"MEMORY_STALL_CORE",         "STREAM_STALL_CORE",
                                 "LOCK_STALL_CORE",           "CASCADE_STALL_CORE"}},
      {"execution",             {"INSTR_VECTOR_CORE",         "INSTR_LOAD_CORE",
                                 "INSTR_STORE_CORE",          "GROUP_CORE_PROGRAM_FLOW"}},
      {"floating_point",        {"FP_OVERFLOW_CORE",          "FP_UNDERFLOW_CORE",
                                 "FP_INVALID_CORE",           "FP_DIV_BY_ZERO_CORE"}},
      {"stream_put_get",        {"INSTR_CASCADE_GET_CORE",    "INSTR_CASCADE_PUT_CORE",
                                 "INSTR_STREAM_GET_CORE",     "INSTR_STREAM_PUT_CORE"}},
      {"write_bandwidths",      {"ACTIVE_CORE",               "INSTR_STREAM_PUT_CORE",
                                 "INSTR_CASCADE_PUT_CORE",    "EVENT_TRUE_CORE"}},
      {"read_bandwidths",       {"ACTIVE_CORE",               "INSTR_STREAM_GET_CORE",
                                 "INSTR_CASCADE_GET_CORE",    "EVENT_TRUE_CORE"}},
      {"aie_trace",             {"CORE_TRACE_RUNNING",        "CORE_TRACE_STALLED",
                                 "MEMORY_TRACE_RUNNING",      "MEMORY_TRACE_STALLED"}},
      {"events",                {"INSTR_EVENT_0_CORE",        "INSTR_EVENT_1_CORE",
                                 "USER_EVENT_0_CORE",         "USER_EVENT_1_CORE"}}
    };
    mMemoryEventStrings = {
      {"conflicts",             {"GROUP_MEMORY_CONFLICT_MEM", "GROUP_ERRORS_MEM"}},
      {"dma_locks",             {"GROUP_DMA_ACTIVITY_MEM",    "GROUP_LOCK_MEM"}},
      {"dma_stalls_s2mm",       {"DMA_S2MM_0_STALLED_LOCK_ACQUIRE_MEM",
                                 "DMA_S2MM_1_STALLED_LOCK_ACQUIRE_MEM"}},
      {"dma_stalls_mm2s",       {"DMA_MM2S_0_STALLED_LOCK_ACQUIRE_MEM",
                                 "DMA_MM2S_1_STALLED_LOCK_ACQUIRE_MEM"}},
      {"write_bandwidths",      {"DMA_MM2S_0_FINISHED_BD_MEM",
                                 "DMA_MM2S_1_FINISHED_BD_MEM"}},
      {"read_bandwidths",       {"DMA_S2MM_0_FINISHED_BD_MEM",
                                 "DMA_S2MM_1_FINISHED_BD_MEM"}}
    };
    mShimEventStrings = {
      {"input_bandwidths",      {"PORT_RUNNING_0_PL", "PORT_STALLED_0_PL"}},
      {"output_bandwidths",     {"PORT_RUNNING_0_PL", "PORT_STALLED_0_PL"}},
      {"packets",               {"PORT_TLAST_0_PL",   "PORT_TLAST_1_PL"}}
    };
  }
};

#endif

