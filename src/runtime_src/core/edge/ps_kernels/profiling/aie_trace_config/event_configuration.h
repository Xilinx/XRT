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

  std::vector<XAie_Events> coreEventsBase;
  std::vector<XAie_Events> memoryCrossEventsBase;
  XAie_Events coreTraceStartEvent = XAIE_EVENT_ACTIVE_CORE;
  XAie_Events coreTraceEndEvent = XAIE_EVENT_DISABLED_CORE;
  std::vector<XAie_Events> coreCounterStartEvents;
  std::vector<XAie_Events> coreCounterEndEvents;
  std::vector<uint32_t> coreCounterEventValues;
  std::vector<XAie_Events> memoryCounterStartEvents;
  std::vector<XAie_Events> memoryCounterEndEvents;
  std::vector<uint32_t> memoryCounterEventValues;

  std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> mCoreCounters;
  std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> mMemoryCounters;
  
  void initialize(const xdp::built_in::InputConfiguration* params) {
    if (params->counterScheme == static_cast<uint8_t>(xdp::built_in::CounterScheme::ES1)) {
    // ES1 requires 2 performance counters to get around hardware bugs
      coreCounterStartEvents.push_back(XAIE_EVENT_ACTIVE_CORE);
      coreCounterStartEvents.push_back(XAIE_EVENT_ACTIVE_CORE);

      coreCounterEndEvents.push_back(XAIE_EVENT_DISABLED_CORE);
      coreCounterEndEvents.push_back(XAIE_EVENT_DISABLED_CORE);

      coreCounterEventValues.push_back(1020);
      coreCounterEventValues.push_back(1020*1020);

      memoryCounterStartEvents.push_back(XAIE_EVENT_TRUE_MEM);
      memoryCounterStartEvents.push_back(XAIE_EVENT_TRUE_MEM);

      memoryCounterEndEvents.push_back(XAIE_EVENT_NONE_MEM);
      memoryCounterEndEvents.push_back(XAIE_EVENT_NONE_MEM);

      memoryCounterEventValues.push_back(1020);
      memoryCounterEventValues.push_back(1020*1020);
    }
    else if (params->counterScheme == static_cast<uint8_t>(xdp::built_in::CounterScheme::ES2)) {
      // ES2 requires only 1 performance counter
      coreCounterStartEvents.push_back(XAIE_EVENT_ACTIVE_CORE);
      coreCounterEndEvents.push_back(XAIE_EVENT_DISABLED_CORE);
      coreCounterEventValues.push_back(0x3ff00);

      memoryCounterStartEvents = {XAIE_EVENT_TRUE_MEM};
      memoryCounterEndEvents   = {XAIE_EVENT_NONE_MEM};
      memoryCounterEventValues = {0x3FF00};
    }

    // All configurations have these first events in common
    coreEventsBase.push_back(XAIE_EVENT_INSTR_CALL_CORE);
    coreEventsBase.push_back(XAIE_EVENT_INSTR_RETURN_CORE);
    memoryCrossEventsBase.push_back(XAIE_EVENT_INSTR_CALL_CORE);
    memoryCrossEventsBase.push_back(XAIE_EVENT_INSTR_RETURN_CORE);

    switch (params->metricSet) {
    case static_cast<uint8_t>(xdp::built_in::MetricSet::FUNCTIONS):
      // No additional events 
      break ;
    case static_cast<uint8_t>(xdp::built_in::MetricSet::PARTIAL_STALLS):
      memoryCrossEventsBase.push_back(XAIE_EVENT_STREAM_STALL_CORE);
      memoryCrossEventsBase.push_back(XAIE_EVENT_CASCADE_STALL_CORE);
      memoryCrossEventsBase.push_back(XAIE_EVENT_LOCK_STALL_CORE);
      break;
    case static_cast<uint8_t>(xdp::built_in::MetricSet::ALL_STALLS): 
      [[fallthrough]];
    case static_cast<uint8_t>(xdp::built_in::MetricSet::ALL):
      memoryCrossEventsBase.push_back(XAIE_EVENT_MEMORY_STALL_CORE);
      memoryCrossEventsBase.push_back(XAIE_EVENT_STREAM_STALL_CORE);
      memoryCrossEventsBase.push_back(XAIE_EVENT_CASCADE_STALL_CORE);
      memoryCrossEventsBase.push_back(XAIE_EVENT_LOCK_STALL_CORE);
      break;
    default:
      break;
    }
  }
};

#endif

