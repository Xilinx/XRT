// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2021, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

// This file defines shim level XRT Graph APIs.

#ifndef XCL_COMMON_GRAPH_H_
#define XCL_COMMON_GRAPH_H_

#include "xrt/xrt_aie.h"
#include "xrt/xrt_graph.h"

typedef void * xclGraphHandle;

xclGraphHandle
xclGraphOpen(xclDeviceHandle handle, const xuid_t xclbinUUID, const char *graphName, xrt::graph::access_mode am);

void
xclGraphClose(xclGraphHandle gh);

int
xclGraphReset(xclGraphHandle gh);

uint64_t
xclGraphTimeStamp(xclGraphHandle gh);

int
xclGraphRun(xclGraphHandle gh, int iterations);

int
xclGraphWaitDone(xclGraphHandle gh, int timeoutMilliSec);

int
xclGraphWait(xclGraphHandle gh, uint64_t cycle);

int
xclGraphSuspend(xclGraphHandle gh);

int
xclGraphResume(xclGraphHandle gh);

int
xclGraphEnd(xclGraphHandle gh, uint64_t cycle);

int
xclGraphUpdateRTP(xclGraphHandle ghdl, const char* port, const char* buffer, size_t size);

int
xclGraphReadRTP(xclGraphHandle ghdl, const char *port, char *buffer, size_t size);

int
xclAIEOpenContext(xclDeviceHandle handle, xrt::aie::access_mode am);

int
xclSyncBOAIE(xclDeviceHandle handle, xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset);

int
xclResetAIEArray(xclDeviceHandle handle);

int
xclSyncBOAIENB(xclDeviceHandle handle, xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset);

int
xclGMIOWait(xclDeviceHandle handle, const char *gmioName);

int
xclStartProfiling(xclDeviceHandle handle, int option, const char* port1Name, const char* port2Nmae, uint32_t value);

uint64_t
xclReadProfiling(xclDeviceHandle handle, int phdl);

int
xclStopProfiling(xclDeviceHandle handle, int phdl);

int
xclLoadXclBinMeta(xclDeviceHandle handle, const xclBin *buffer);

int
xclConfigureBD(xclDeviceHandle handle,
    int tileType, uint8_t column, uint8_t row, uint8_t bdId,
    uint64_t address,
    uint32_t length,
    const std::vector<uint32_t>& stepsize,
    const std::vector<uint32_t>& wrap,
    const std::vector<std::pair<uint32_t, uint32_t>>& padding,
    bool enable_packet,
    uint8_t packet_id,
    uint8_t out_of_order_bd_id,
    bool tlast_suppress,
    uint32_t iteration_stepsize,
    uint16_t iteration_wrap,
    uint8_t iteration_current,
    bool enable_compression,
    bool lock_acq_enable,
    int8_t lock_acq_value,
    uint8_t lock_acq_id,
    int8_t lock_rel_value,
    uint8_t lock_rel_id,
    bool use_next_bd,
    uint8_t next_bd,
    uint8_t burst_length
);

int
xclEnqueueTask(xclDeviceHandle handle, int tileType, uint8_t column, uint8_t row, int dir, uint8_t channel, uint32_t repeatCount, bool enableTaskCompleteToken, uint8_t startBdId);

int
xclWaitDMAChannelTaskQueue(xclDeviceHandle handle, int tileType, uint8_t column, uint8_t row, int dir, uint8_t channel);

int
xclWaitDMAChannelDone(xclDeviceHandle handle, int tileType, uint8_t column, uint8_t row, int dir, uint8_t channel);

int
xclInitializeLock(xclDeviceHandle handle, int tileType, uint8_t column, uint8_t row, unsigned short lockId, int8_t initVal);

int
xclAcquireLock(xclDeviceHandle handle, int tileType, uint8_t column, uint8_t row, unsigned short lockId, int8_t acqVal);

int
xclReleaseLock(xclDeviceHandle handle, int tileType, uint8_t column, uint8_t row, unsigned short lockId, int8_t relVal);

#endif
