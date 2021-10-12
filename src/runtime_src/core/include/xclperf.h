/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

// Common information on the profiling IP structure that is used in
//  the profiling portion of XDP, the application debug portion
//  of XDP, the emulation shims, and xbutil.
#ifndef _XCL_PERF_H_
#define _XCL_PERF_H_

// DSA version (e.g., XCL_PLATFORM=xilinx_adm-pcie-7v3_1ddr_1_1)
// Simply a default as its read from the device using lspci (see CR 870994)
#define DSA_MAJOR_VERSION 1
#define DSA_MINOR_VERSION 1

/************************ DEBUG IP LAYOUT ************************************/

#define IP_LAYOUT_SEP "-"

/************************ APM 0: Monitor MIG Ports ****************************/

#define XPAR_AIM0_HOST_SLOT                             0

#ifdef XRT_EDGE
#define XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH            32
#else
#define XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH            64
#endif

#define MAX_TRACE_NUMBER_SAMPLES                        16384

/************************ APM Profile Counters ********************************/

// Max slots = floor(max slots on trace funnel / 2) = floor(63 / 2) = 31
// NOTE: AIM max slots += 3 to support XDMA/KDMA/P2P monitors on some 2018.3 platforms
#define XAIM_MAX_NUMBER_SLOTS             34
#define XAM_MAX_NUMBER_SLOTS             31
#define XASM_MAX_NUMBER_SLOTS            31

/************************ Trace IDs ************************************/

#define MIN_TRACE_ID_AIM        0
#define MAX_TRACE_ID_AIM        61
#define MIN_TRACE_ID_AM        64
#define MAX_TRACE_ID_AM        544
#define MAX_TRACE_ID_AM_HWEM   94
#define MIN_TRACE_ID_ASM       576
#define MAX_TRACE_ID_ASM       607

/********************** Definitions: Enums, Structs ***************************/

/* Performance monitor type or location */
enum xclPerfMonType {
	XCL_PERF_MON_MEMORY = 0,
	XCL_PERF_MON_HOST   = 1,
	XCL_PERF_MON_SHELL  = 2,
	XCL_PERF_MON_ACCEL  = 3,
	XCL_PERF_MON_STALL  = 4,
	XCL_PERF_MON_STR    = 5,
	XCL_PERF_MON_FIFO   = 6,
  XCL_PERF_MON_NOC    = 7,
	XCL_PERF_MON_TOTAL_PROFILE = 8
};

/*
 * Performance monitor event types
 * NOTE: these are the same values used by Zynq
 */
enum xclPerfMonEventType {
  XCL_PERF_MON_START_EVENT = 0x4,
  XCL_PERF_MON_END_EVENT = 0x5
};

/* Performance monitor counter results */
typedef struct {
  float              SampleIntervalUsec;
  unsigned long long WriteBytes[XAIM_MAX_NUMBER_SLOTS];
  unsigned long long WriteTranx[XAIM_MAX_NUMBER_SLOTS];
  unsigned long long WriteLatency[XAIM_MAX_NUMBER_SLOTS];
  unsigned short     WriteMinLatency[XAIM_MAX_NUMBER_SLOTS];
  unsigned short     WriteMaxLatency[XAIM_MAX_NUMBER_SLOTS];
  unsigned long long ReadBytes[XAIM_MAX_NUMBER_SLOTS];
  unsigned long long ReadTranx[XAIM_MAX_NUMBER_SLOTS];
  unsigned long long ReadLatency[XAIM_MAX_NUMBER_SLOTS];
  unsigned short     ReadMinLatency[XAIM_MAX_NUMBER_SLOTS];
  unsigned short     ReadMaxLatency[XAIM_MAX_NUMBER_SLOTS];
  unsigned long long ReadBusyCycles[XAIM_MAX_NUMBER_SLOTS];
  unsigned long long WriteBusyCycles[XAIM_MAX_NUMBER_SLOTS];
  // Accelerator Monitor
  unsigned long long CuExecCount[XAM_MAX_NUMBER_SLOTS];
  unsigned long long CuExecCycles[XAM_MAX_NUMBER_SLOTS];
  unsigned long long CuBusyCycles[XAM_MAX_NUMBER_SLOTS];
  unsigned long long CuMaxParallelIter[XAM_MAX_NUMBER_SLOTS];
  unsigned long long CuStallExtCycles[XAM_MAX_NUMBER_SLOTS];
  unsigned long long CuStallIntCycles[XAM_MAX_NUMBER_SLOTS];
  unsigned long long CuStallStrCycles[XAM_MAX_NUMBER_SLOTS];
  unsigned long long CuMinExecCycles[XAM_MAX_NUMBER_SLOTS];
  unsigned long long CuMaxExecCycles[XAM_MAX_NUMBER_SLOTS];
  // AXI Stream Monitor
  unsigned long long StrNumTranx[XASM_MAX_NUMBER_SLOTS];
  unsigned long long StrDataBytes[XASM_MAX_NUMBER_SLOTS];
  unsigned long long StrBusyCycles[XASM_MAX_NUMBER_SLOTS];
  unsigned long long StrStallCycles[XASM_MAX_NUMBER_SLOTS];
  unsigned long long StrStarveCycles[XASM_MAX_NUMBER_SLOTS];
} xclCounterResults;

/* Performance monitor trace results */
typedef struct {
  enum xclPerfMonEventType EventType;
  unsigned long long Timestamp;
  unsigned char  Overflow;
  unsigned int TraceID;
  unsigned char Error;
  unsigned char Reserved;
  int isClockTrain;
  // Used in HW Emulation
  unsigned long long  HostTimestamp;
  unsigned char  EventFlags;
  unsigned char  WriteAddrLen;
  unsigned char  ReadAddrLen;
  unsigned short WriteBytes;
  unsigned short ReadBytes;
} xclTraceResults;

#define DRIVER_NAME_ROOT "/dev"
#define DEVICE_PREFIX "/dri/renderD"
#define NIFD_PREFIX "/nifd"
#define MAX_NAME_LEN 256

/**
 * \brief data structure for querying device info
 * 
 * TODO:
 * all the data for nifd won't be avaiable until nifd
 * driver is merged and scan.h is changed to recognize
 * nifd driver.
 */
typedef struct {
  unsigned int device_index;
  unsigned int user_instance;
  unsigned int nifd_instance;
  char device_name[MAX_NAME_LEN];
  char nifd_name[MAX_NAME_LEN];
} xclDebugProfileDeviceInfo;

// Used in the HAL API Interface to access hardware counters in host code
enum HalInterfaceCallbackType {
  START_DEVICE_PROFILING,
  CREATE_PROFILE_RESULTS,
  GET_PROFILE_RESULTS,
  DESTROY_PROFILE_RESULTS
} ;

#ifdef __cplusplus
#include <cstdint>
#include <cstddef>
#else
#include <stdlib.h>
#include <stdint.h>
#endif

/**
 * This is an example of the struct that callback
 * functions can take. Eventually, different API
 * callbacks are likely to take different structs.
 */
typedef struct CBPayload {
  uint64_t idcode;
  void* deviceHandle;
} CBPayload;

/**
 * More callback payload struct should be declared 
 * here for the users to include.
 */

struct ProfileResultsCBPayload
{
  struct CBPayload basePayload;
  void* results;
};

/**
 * end hal level xdp plugin types
 */


#endif
