/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef XDP_APP_DEBUG_INT_H
#define XDP_APP_DEBUG_INT_H

#include <cstdint>

#include "core/include/xdp/common.h"
#include "core/include/xdp/lapc.h"

// An additional type (xclDebugReadType) is used in a shim function that
// was incorrectly exposed to the user in the xrt.h.  The declaration
// is deprecated in 2022.2 (but not the shim function) and the type will
// be re-implemented here in a future release when the the declaration is
// removed.

namespace xdp {

// The structs declared in this file are used in xbutil and application
// debug in order to read all the debug counter values from the
// debug/profile IP.

struct AIMCounterResults {
  uint64_t WriteBytes   [MAX_NUM_AIMS];
  uint64_t WriteTranx   [MAX_NUM_AIMS];
  uint64_t ReadBytes    [MAX_NUM_AIMS];
  uint64_t ReadTranx    [MAX_NUM_AIMS];

  uint64_t OutStandCnts [MAX_NUM_AIMS];
  uint64_t LastWriteAddr[MAX_NUM_AIMS];
  uint64_t LastWriteData[MAX_NUM_AIMS];
  uint64_t LastReadAddr [MAX_NUM_AIMS];
  uint64_t LastReadData [MAX_NUM_AIMS];
  uint32_t NumSlots;
  char     DevUserName[256];
};

struct ASMCounterResults {
  uint32_t NumSlots;
  char DevUserName[256];

  uint64_t StrNumTranx    [MAX_NUM_ASMS];
  uint64_t StrDataBytes   [MAX_NUM_ASMS];
  uint64_t StrBusyCycles  [MAX_NUM_ASMS];
  uint64_t StrStallCycles [MAX_NUM_ASMS];
  uint64_t StrStarveCycles[MAX_NUM_ASMS];
};

struct AMCounterResults {
  uint32_t NumSlots ;
  char DevUserName[256] ;

  uint64_t CuExecCount      [MAX_NUM_AMS];
  uint64_t CuExecCycles     [MAX_NUM_AMS];
  uint64_t CuBusyCycles     [MAX_NUM_AMS];
  uint64_t CuMaxParallelIter[MAX_NUM_AMS];
  uint64_t CuStallExtCycles [MAX_NUM_AMS];
  uint64_t CuStallIntCycles [MAX_NUM_AMS];
  uint64_t CuStallStrCycles [MAX_NUM_AMS];
  uint64_t CuMinExecCycles  [MAX_NUM_AMS];
  uint64_t CuMaxExecCycles  [MAX_NUM_AMS];
  uint64_t CuStartCount     [MAX_NUM_AMS];
};

struct ADDCounterResults {
  uint32_t Num;
  uint32_t DeadlockStatus;
};

struct LAPCCounterResults {
  uint32_t OverallStatus[MAX_NUM_LAPCS];
  uint32_t CumulativeStatus[MAX_NUM_LAPCS][IP::LAPC::NUM_STATUS];
  uint32_t SnapshotStatus[MAX_NUM_LAPCS][IP::LAPC::NUM_STATUS];
  uint32_t NumSlots;
  char DevUserName[256];
};

struct SPCCounterResults {
  uint32_t PCAsserted[MAX_NUM_SPCS];
  uint32_t CurrentPC [MAX_NUM_SPCS];
  uint32_t SnapshotPC[MAX_NUM_SPCS];
  uint32_t NumSlots;
  char DevUserName[256];
};

} // end namespace xdp

#endif
