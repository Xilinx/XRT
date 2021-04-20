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

#ifndef _XCL_COMMON_GRAPH_H_
#define _XCL_COMMON_GRAPH_H_

#include "experimental/xrt_graph.h"

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

#endif
