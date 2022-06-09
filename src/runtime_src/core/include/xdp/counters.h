/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc - All rights reserved
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

#ifndef XDP_COUNTERS_DOT_H
#define XDP_COUNTERS_DOT_H

#include <cstdint>
#include "core/include/xdp/common.h"

namespace xdp {

// Orignally called xclCounterResults, the xdp::CounterResults struct
// is passed to the shim to be filled in with all of the current values
// of the profiling IP registers.  The struct is the maximal set of all
// possible profiling IP combinations.

struct CounterResults {
  float SampleIntervalUsec;

  // Counter values for all of the AIMs in the design
  uint64_t WriteBytes     [MAX_NUM_AIMS];
  uint64_t WriteTranx     [MAX_NUM_AIMS];
  uint64_t WriteLatency   [MAX_NUM_AIMS];
  uint16_t WriteMinLatency[MAX_NUM_AIMS];
  uint16_t WriteMaxLatency[MAX_NUM_AIMS];
  uint64_t ReadBytes      [MAX_NUM_AIMS];
  uint64_t ReadTranx      [MAX_NUM_AIMS];
  uint64_t ReadLatency    [MAX_NUM_AIMS];
  uint16_t ReadMinLatency [MAX_NUM_AIMS];
  uint16_t ReadMaxLatency [MAX_NUM_AIMS];
  uint64_t ReadBusyCycles [MAX_NUM_AIMS];
  uint64_t WriteBusyCycles[MAX_NUM_AIMS];

  // Counter values for all the AMs in the design
  uint64_t CuExecCount      [MAX_NUM_AMS];
  uint64_t CuExecCycles     [MAX_NUM_AMS];
  uint64_t CuBusyCycles     [MAX_NUM_AMS];
  uint64_t CuMaxParallelIter[MAX_NUM_AMS];
  uint64_t CuStallExtCycles [MAX_NUM_AMS];
  uint64_t CuStallIntCycles [MAX_NUM_AMS];
  uint64_t CuStallStrCycles [MAX_NUM_AMS];
  uint64_t CuMinExecCycles  [MAX_NUM_AMS];
  uint64_t CuMaxExecCycles  [MAX_NUM_AMS];

  // Counter values for all the ASMs in the design
  uint64_t StrNumTranx    [MAX_NUM_ASMS];
  uint64_t StrDataBytes   [MAX_NUM_ASMS];
  uint64_t StrBusyCycles  [MAX_NUM_ASMS];
  uint64_t StrStallCycles [MAX_NUM_ASMS];
  uint64_t StrStarveCycles[MAX_NUM_ASMS];
};

} // end namespace xdp

#endif
