/* Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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
#include "xdp/profile/plugin/aie_trace/x86/aie_trace_kernel_config.h"

constexpr uint32_t ES1_TRACE_COUNTER = 1020;
constexpr uint32_t ES2_TRACE_COUNTER = 0x3FF00;

// This struct encapsulates all of the internal configuration information
// for a single AIE tile
struct EventConfiguration {
  XAie_Events coreTraceStartEvent = XAIE_EVENT_ACTIVE_CORE;
  XAie_Events coreTraceEndEvent = XAIE_EVENT_DISABLED_CORE;

  /*
   * This is needed because the cores are started/stopped during execution
   * to get around some hw bugs. We cannot restart tracemodules when that happens.
   * At the end, we need to use event generate register to create this event
   * to gracefully shut down trace modules.
   */
  XAie_Events traceFlushEndEvent = XAIE_EVENT_INSTR_EVENT_1_CORE;

  std::map<xdp::built_in::MetricSet, std::vector<XAie_Events>> coreEventsBase;
  std::map<xdp::built_in::MetricSet, std::vector<XAie_Events>> memoryCrossEventsBase;
  std::map<xdp::built_in::MemTileMetricSet, std::vector<XAie_Events>> memTileEventSets;

  std::vector<XAie_Events> coreCounterStartEvents;
  std::vector<XAie_Events> coreCounterEndEvents;
  std::vector<uint32_t> coreCounterEventValues;
  std::vector<XAie_Events> memoryCounterStartEvents;
  std::vector<XAie_Events> memoryCounterEndEvents;
  std::vector<uint32_t> memoryCounterEventValues;

  std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> mCoreCounters;
  std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> mMemoryCounters;

  void initialize(const xdp::built_in::TraceInputConfiguration* params)
  {
    coreEventsBase = {
        {xdp::built_in::MetricSet::FUNCTIONS, {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE}},
        {xdp::built_in::MetricSet::PARTIAL_STALLS, {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE}},
        {xdp::built_in::MetricSet::ALL_STALLS, {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE}},
        {xdp::built_in::MetricSet::ALL, {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE}}};

    // **** Memory Module Trace ****
    // NOTE 1: Core events listed here are broadcast by the resource manager
    // NOTE 2: These are supplemented with counter events as those are dependent on counter #
    // NOTE 3: For now, 'all' is the same as 'functions_all_stalls'. Combo events (required
    //         for all) have limited support in the resource manager.
    memoryCrossEventsBase = {
        {xdp::built_in::MetricSet::FUNCTIONS, {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE}},
        {xdp::built_in::MetricSet::PARTIAL_STALLS,
         {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE, XAIE_EVENT_STREAM_STALL_CORE,
          XAIE_EVENT_CASCADE_STALL_CORE, XAIE_EVENT_LOCK_STALL_CORE}},
        {xdp::built_in::MetricSet::ALL_STALLS,
         {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE, XAIE_EVENT_MEMORY_STALL_CORE,
          XAIE_EVENT_STREAM_STALL_CORE, XAIE_EVENT_CASCADE_STALL_CORE, XAIE_EVENT_LOCK_STALL_CORE}},
        {xdp::built_in::MetricSet::ALL,
         {XAIE_EVENT_INSTR_CALL_CORE, XAIE_EVENT_INSTR_RETURN_CORE, XAIE_EVENT_MEMORY_STALL_CORE,
          XAIE_EVENT_STREAM_STALL_CORE, XAIE_EVENT_CASCADE_STALL_CORE, XAIE_EVENT_LOCK_STALL_CORE}}};

    if (params->hwGen == 1) {
      if (params->counterScheme == static_cast<uint8_t>(xdp::built_in::CounterScheme::ES1)) {
        // ES1 requires 2 performance counters to get around hardware bugs

        coreCounterStartEvents = {XAIE_EVENT_ACTIVE_CORE, XAIE_EVENT_ACTIVE_CORE};
        coreCounterEndEvents = {XAIE_EVENT_DISABLED_CORE, XAIE_EVENT_DISABLED_CORE};
        coreCounterEventValues = {ES1_TRACE_COUNTER, ES1_TRACE_COUNTER * ES1_TRACE_COUNTER};

        memoryCounterStartEvents = {XAIE_EVENT_TRUE_MEM, XAIE_EVENT_TRUE_MEM};
        memoryCounterEndEvents = {XAIE_EVENT_NONE_MEM, XAIE_EVENT_NONE_MEM};
        memoryCounterEventValues = {ES1_TRACE_COUNTER, ES1_TRACE_COUNTER * ES1_TRACE_COUNTER};

      } else if (params->counterScheme == static_cast<uint8_t>(xdp::built_in::CounterScheme::ES2)) {
        // ES2 requires only 1 performance counter
        coreCounterStartEvents = {XAIE_EVENT_ACTIVE_CORE};
        coreCounterEndEvents = {XAIE_EVENT_DISABLED_CORE};
        coreCounterEventValues = {ES2_TRACE_COUNTER};

        memoryCounterStartEvents = {XAIE_EVENT_TRUE_MEM};
        memoryCounterEndEvents = {XAIE_EVENT_NONE_MEM};
        memoryCounterEventValues = {ES2_TRACE_COUNTER};
      }
    }

    // **** Memory Tile Trace ****
    memTileEventSets = {
        {xdp::built_in::MemTileMetricSet::INPUT_CHANNELS,
         {XAIE_EVENT_DMA_S2MM_SEL0_START_TASK_MEM_TILE, XAIE_EVENT_DMA_S2MM_SEL1_START_TASK_MEM_TILE,
          XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_BD_MEM_TILE, XAIE_EVENT_DMA_S2MM_SEL1_FINISHED_BD_MEM_TILE,
          XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_TASK_MEM_TILE, XAIE_EVENT_DMA_S2MM_SEL1_FINISHED_TASK_MEM_TILE}},
        {xdp::built_in::MemTileMetricSet::INPUT_CHANNELS_STALLS,
         {XAIE_EVENT_DMA_S2MM_SEL0_START_TASK_MEM_TILE, XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_BD_MEM_TILE,
          XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_TASK_MEM_TILE, XAIE_EVENT_DMA_S2MM_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE,
          XAIE_EVENT_DMA_S2MM_SEL0_STREAM_STARVATION_MEM_TILE, XAIE_EVENT_DMA_S2MM_SEL0_MEMORY_BACKPRESSURE_MEM_TILE}},
        {xdp::built_in::MemTileMetricSet::OUTPUT_CHANNELS,
         {XAIE_EVENT_DMA_MM2S_SEL0_START_TASK_MEM_TILE, XAIE_EVENT_DMA_MM2S_SEL1_START_TASK_MEM_TILE,
          XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_MEM_TILE, XAIE_EVENT_DMA_MM2S_SEL1_FINISHED_BD_MEM_TILE,
          XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_TASK_MEM_TILE, XAIE_EVENT_DMA_MM2S_SEL1_FINISHED_TASK_MEM_TILE}},
        {xdp::built_in::MemTileMetricSet::OUTPUT_CHANNELS_STALLS,
         {XAIE_EVENT_DMA_MM2S_SEL0_START_TASK_MEM_TILE, XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_MEM_TILE,
          XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_TASK_MEM_TILE, XAIE_EVENT_DMA_MM2S_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE,
          XAIE_EVENT_DMA_MM2S_SEL0_STREAM_BACKPRESSURE_MEM_TILE, XAIE_EVENT_DMA_MM2S_SEL0_MEMORY_STARVATION_MEM_TILE}}};

    // MEM tile trace is always on
    XAie_Events memTileTraceStartEvent = XAIE_EVENT_TRUE_MEM_TILE;
    XAie_Events memTileTraceEndEvent = XAIE_EVENT_NONE_MEM_TILE;
  }
};

#endif
