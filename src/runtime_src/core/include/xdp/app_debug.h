/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
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

#include "core/include/xdp/common.h"

// The order in which we display registers is different from the
// offset in the actual IP (retrieved via ioctls).
namespace xdp {
  namespace DebugIPRegisters {

    // Offsets of the debug registers inside the AXI Interface Monitor
    namespace AIM {
      constexpr int NUM_COUNTERS           = 13;
      constexpr int NUM_COUNTERS_DISPLAYED = 9;

      namespace DisplayIndex {
        constexpr int WRITE_BYTES        = 0;
        constexpr int WRITE_TRANX        = 1;
        constexpr int READ_BYTES         = 2;
        constexpr int READ_TRANX         = 3;
        constexpr int OUTSTANDING_COUNT  = 4;
        constexpr int WRITE_LAST_ADDRESS = 5;
        constexpr int WRITE_LAST_DATA    = 6;
        constexpr int READ_LAST_ADDRESS  = 7;
        constexpr int READ_LAST_DATA     = 8;
      } // end namespace DisplayIndex

      namespace IoctlIndex {
        constexpr int WRITE_BYTES        = 0;
        constexpr int WRITE_TRANX        = 1;
        constexpr int WRITE_LATENCY      = 2;
        constexpr int WRITE_BUSY_CYCLES  = 3;
        constexpr int READ_BYTES         = 4;
        constexpr int READ_TRANX         = 5;
        constexpr int READ_LATENCY       = 6;
        constexpr int READ_BUSY_CYCLES   = 7;
        constexpr int OUTSTANDING_COUNT  = 8;
        constexpr int WRITE_LAST_ADDRESS = 9;
        constexpr int WRITE_LAST_DATA    = 10;
        constexpr int READ_LAST_ADDRESS  = 11;
        constexpr int READ_LAST_DATA     = 12;
      } // end namespace IoctlIndex
    } // end namespace AIM

    // Offsets of the debug registers inside the Accelerator Monitor
    namespace AM {
      constexpr int NUM_COUNTERS           = 10;
      constexpr int NUM_COUNTERS_DISPLAYED = 8;

      namespace DisplayIndex {
        constexpr int EXECUTION_COUNT      = 0;
        constexpr int EXECUTION_CYCLES     = 1;
        constexpr int STALL_INT            = 2;
        constexpr int STALL_STR            = 3;
        constexpr int STALL_EXT            = 4;
        constexpr int MIN_EXECUTION_CYCLES = 5;
        constexpr int MAX_EXECUTION_CYCLES = 6;
        constexpr int TOTAL_CU_START       = 7;
      } // end namespace Index

      namespace IoctlIndex {
        constexpr int EXECUTION_COUNT      = 0;
        constexpr int START_COUNT          = 1;
        constexpr int EXECUTION_CYCLES     = 2;
        constexpr int STALL_INT            = 3;
        constexpr int STALL_STR            = 4;
        constexpr int STALL_EXT            = 5;
        constexpr int BUSY_CYCLES          = 6;
        constexpr int MAX_PARALLEL_ITR     = 7;
        constexpr int MAX_EXECUTION_CYCLES = 8;
        constexpr int MIN_EXECUTION_CYCLES = 9;
      } // end namespace IoctlIndex
    } // end namespace AM

    namespace ASM {
      constexpr int NUM_COUNTERS = 5;

      // For ASMs, the display index and Ioctl index are the same
      namespace Index {
        constexpr int NUM_TRANX     = 0;
        constexpr int DATA_BYTES    = 1;
        constexpr int BUSY_CYCLES   = 2;
        constexpr int STALL_CYCLES  = 3;
        constexpr int STARVE_CYCLES = 4;
      } // end namespace Index
    } // end namespace ASM

    namespace LAPC {
      constexpr int NUM_COUNTERS = 31;
      constexpr int NUM_STATUS = 9;
      constexpr int STATUS_REG_NUM = 4;

      // For LAPCs, the DisplayIndex and IoctlIndex are the same
      namespace Index {
        constexpr int STATUS              = 0;
        constexpr int CUMULATIVE_STATUS_0 = 1;
        constexpr int CUMULATIVE_STATUS_1 = 2;
        constexpr int CUMULATIVE_STATUS_2 = 3;
        constexpr int CUMULATIVE_STATUS_3 = 4;
        constexpr int SNAPSHOT_STATUS_0   = 5;
        constexpr int SNAPSHOT_STATUS_1   = 6;
        constexpr int SNAPSHOT_STATUS_2   = 7;
        constexpr int SNAPSHOT_STATUS_3   = 8;
      } /// end namespace Index
    } // end namespace LAPC

    namespace SPC {
      constexpr int NUM_COUNTERS      = 31;
      constexpr int NUM_STATUS_PER_IP = 3;
      namespace Index {
        constexpr int PC_ASSERTED = 0;
        constexpr int CURRENT_PC  = 1;
        constexpr int SNAPSHOT_PC = 2;
      } // end namespace Index
    } // end namespace SPC

  } // end namespace DebugIPRegisters
} // end namespace xdp

#ifdef __cplusplus
extern "C" {
#endif

/********************** Definitions: Enums, Structs ***************************/
enum xclDebugReadType {
  XCL_DEBUG_READ_TYPE_APM  = 0,
  XCL_DEBUG_READ_TYPE_LAPC = 1,
  XCL_DEBUG_READ_TYPE_AIM  = 2,
  XCL_DEBUG_READ_TYPE_ASM  = 3,
  XCL_DEBUG_READ_TYPE_AM   = 4,
  XCL_DEBUG_READ_TYPE_SPC  = 5,
  XCL_DEBUG_READ_TYPE_ADD  = 6
};

/* Debug counter results */
typedef struct {
  unsigned long long int WriteBytes     [xdp::MAX_NUM_AIMS];
  unsigned long long int WriteTranx     [xdp::MAX_NUM_AIMS];
  unsigned long long int ReadBytes      [xdp::MAX_NUM_AIMS];
  unsigned long long int ReadTranx      [xdp::MAX_NUM_AIMS];

  unsigned long long int OutStandCnts   [xdp::MAX_NUM_AIMS];
  unsigned long long int LastWriteAddr  [xdp::MAX_NUM_AIMS];
  unsigned long long int LastWriteData  [xdp::MAX_NUM_AIMS];
  unsigned long long int LastReadAddr   [xdp::MAX_NUM_AIMS];
  unsigned long long int LastReadData   [xdp::MAX_NUM_AIMS];
  unsigned int           NumSlots;
  char                   DevUserName    [256];
} xclDebugCountersResults;

typedef struct {
  unsigned int           NumSlots;
  char                   DevUserName    [256];

  unsigned long long int StrNumTranx    [xdp::MAX_NUM_ASMS];
  unsigned long long int StrDataBytes   [xdp::MAX_NUM_ASMS];
  unsigned long long int StrBusyCycles  [xdp::MAX_NUM_ASMS];
  unsigned long long int StrStallCycles [xdp::MAX_NUM_ASMS];
  unsigned long long int StrStarveCycles[xdp::MAX_NUM_ASMS];
} xclStreamingDebugCountersResults ;

typedef struct {
  unsigned int           NumSlots ;
  char                   DevUserName    [256] ;

  unsigned long long CuExecCount        [xdp::MAX_NUM_AMS];
  unsigned long long CuExecCycles       [xdp::MAX_NUM_AMS];
  unsigned long long CuBusyCycles       [xdp::MAX_NUM_AMS];
  unsigned long long CuMaxParallelIter  [xdp::MAX_NUM_AMS];
  unsigned long long CuStallExtCycles   [xdp::MAX_NUM_AMS];
  unsigned long long CuStallIntCycles   [xdp::MAX_NUM_AMS];
  unsigned long long CuStallStrCycles   [xdp::MAX_NUM_AMS];
  unsigned long long CuMinExecCycles    [xdp::MAX_NUM_AMS];
  unsigned long long CuMaxExecCycles    [xdp::MAX_NUM_AMS];
  unsigned long long CuStartCount       [xdp::MAX_NUM_AMS];
} xclAccelMonitorCounterResults;

typedef struct {
  unsigned int           Num;
  unsigned int           DeadlockStatus;
} xclAccelDeadlockDetectorResults;

enum xclCheckerType {
XCL_CHECKER_MEMORY = 0,
XCL_CHECKER_STREAM = 1
};

/* Debug checker results */
typedef struct {
  unsigned int   OverallStatus[xdp::DebugIPRegisters::LAPC::NUM_COUNTERS];
  unsigned int   CumulativeStatus[xdp::DebugIPRegisters::LAPC::NUM_COUNTERS][xdp::DebugIPRegisters::LAPC::STATUS_REG_NUM];
  unsigned int   SnapshotStatus[xdp::DebugIPRegisters::LAPC::NUM_COUNTERS][xdp::DebugIPRegisters::LAPC::STATUS_REG_NUM];
  unsigned int   NumSlots;
  char DevUserName[256];
} xclDebugCheckersResults;

typedef struct {
  unsigned int PCAsserted[xdp::DebugIPRegisters::SPC::NUM_COUNTERS];
  unsigned int CurrentPC [xdp::DebugIPRegisters::SPC::NUM_COUNTERS];
  unsigned int SnapshotPC[xdp::DebugIPRegisters::SPC::NUM_COUNTERS];
  unsigned int NumSlots;
  char DevUserName[256];
} xclDebugStreamingCheckersResults;

#ifdef __cplusplus
}
#endif
#endif
