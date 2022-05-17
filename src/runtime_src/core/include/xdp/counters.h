/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc
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

#include "core/include/xdp/common.h"

// The definitions in this file are used in the shim interface, which are
// dynamically linked via dlsym().  Because of that, the structures are
// defined in C using C constructs.
#ifdef __cplusplus
extern "C" {
#endif

// xclCounterResults is passed to the shim to be filled in with all
// of the current values of the profiling IP registers.  The struct is
// the maximal set of all possible profiling IP combinations.

typedef struct {
  float SampleIntervalUsec;

  // Counter values for all of the AIMs in the design
  unsigned long long int WriteBytes[xdp::MAX_NUM_AIMS];
  unsigned long long int WriteTranx[xdp::MAX_NUM_AIMS];
  unsigned long long int WriteLatency[xdp::MAX_NUM_AIMS];
  unsigned short int     WriteMinLatency[xdp::MAX_NUM_AIMS];
  unsigned short int     WriteMaxLatency[xdp::MAX_NUM_AIMS];
  unsigned long long int ReadBytes[xdp::MAX_NUM_AIMS];
  unsigned long long int ReadTranx[xdp::MAX_NUM_AIMS];
  unsigned long long int ReadLatency[xdp::MAX_NUM_AIMS];
  unsigned short int     ReadMinLatency[xdp::MAX_NUM_AIMS];
  unsigned short int     ReadMaxLatency[xdp::MAX_NUM_AIMS];
  unsigned long long int ReadBusyCycles[xdp::MAX_NUM_AIMS];
  unsigned long long int WriteBusyCycles[xdp::MAX_NUM_AIMS];

  // Counter values for all the AMs in the design
  unsigned long long int CuExecCount[xdp::MAX_NUM_AMS];
  unsigned long long int CuExecCycles[xdp::MAX_NUM_AMS];
  unsigned long long int CuBusyCycles[xdp::MAX_NUM_AMS];
  unsigned long long int CuMaxParallelIter[xdp::MAX_NUM_AMS];
  unsigned long long int CuStallExtCycles[xdp::MAX_NUM_AMS];
  unsigned long long int CuStallIntCycles[xdp::MAX_NUM_AMS];
  unsigned long long int CuStallStrCycles[xdp::MAX_NUM_AMS];
  unsigned long long int CuMinExecCycles[xdp::MAX_NUM_AMS];
  unsigned long long int CuMaxExecCycles[xdp::MAX_NUM_AMS];

  // Counter values for all the ASMs in the design
  unsigned long long int StrNumTranx[xdp::MAX_NUM_ASMS];
  unsigned long long int StrDataBytes[xdp::MAX_NUM_ASMS];
  unsigned long long int StrBusyCycles[xdp::MAX_NUM_ASMS];
  unsigned long long int StrStallCycles[xdp::MAX_NUM_ASMS];
  unsigned long long int StrStarveCycles[xdp::MAX_NUM_ASMS];
} xclCounterResults;

#ifdef __cplusplus
}
#endif

#endif
