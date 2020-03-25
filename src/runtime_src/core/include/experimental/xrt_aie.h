/**
 * Copyright (C) 2020 Xilinx, Inc
 * Author(s): Larry Liu
 * ZNYQ XRT Library layered on top of ZYNQ zocl kernel driver
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

#ifndef _XRT_AIE_H_
#define _XRT_AIE_H_

#include "xrt.h"

typedef void *xrtGraphHandle;

/**
 * xrtGraphOpen() - Open a graph and obtain its handle.
 *
 * @deviceHandle: Handle to the device with the graph.
 * @xclbUUID:     UUID of the xclbin with the specified graph.
 * @graphNmae:    The name of graph to be open.
 * Return:        Handle to representing the graph. NULL for error.
 *
 * An xclbin with the specified graph must have been loaded prior
 * to calling this function.
 */
xrtGraphHandle
xrtGraphOpen(xclDeviceHandle handle, uuid_t xclbinUUID, const char *graphName);

/**
 * xrtGraphClose() - Close an open graph.
 *
 * @gh:            Handle to graph previously opened with xrtGraphOpen.
 *
 */
void
xrtGraphClose(xrtGraphHandle gh);

/**
 * xrtGraphReset() - Reset a graph.
 *
 * @gh:            Handle to graph previously opened with xrtGraphOpen.
 * Return:         0 on success, -1 on error
 *
 * Note: Reset by disable tiles and enable tile reset
 */
int
xrtGraphReset(xrtGraphHandle gh);

/**
 * xrtGraphTimeStamp() - Get timestamp of a graph. The unit of timestamp is
 *                       AIE Cycle.
 *
 * @gh:             Handle to graph previously opened with xrtGraphOpen.
 * Return:          Timestamp in AIE cycle.
 */
uint64_t
xrtGraphTimeStamp(xrtGraphHandle gh);

/**
 * xrtGraphUpdateIter() - Update graph run iteration.
 *
 * @gh:             Handle to graph previously opened with xrtGraphOpen.
 * @iterations:     The run iteration to update to graph. -1 for infinite.
 * Return:          0 on success, -1 on error
 */
int
xrtGraphUpdateIter(xrtGraphHandle gh, int iterations);

/**
 * xrtGraphRun() - Start a graph execution
 *
 * @gh:             Handle to graph previously opened with xrtGraphOpen.
 * Return:          0 on success, -1 on error
 *
 * Note: Run by enable tiles and disable tile reset
 */
int
xrtGraphRun(xrtGraphHandle gh);

/**
 * xrtGraphWaitDone() - Wait for graph to stop.
 *
 * @gh:              Handle to graph previously opened with xrtGraphOpen.
 * @timeoutMilliSec: Timeout value to wait for graph done.
 *
 * Return:          0 on success, -1 on timeout.
 *
 * Note: Wait for done status of ALL the tiles
 */
int
xrtGraphWaitDone(xrtGraphHandle gh, int timeoutMilliSec);

/**
 * xrtGraphSuspend() - Suspend a running graph.
 *
 * @gh:             Handle to graph previously opened with xrtGraphOpen.
 * Return:          0 on success, -1 on error.
 */
int
xrtGraphSuspend(xrtGraphHandle gh);

/**
 * xrtGraphResume() - Resume a suspended graph.
 *
 * @gh:             Handle to graph previously opened with xrtGraphOpen.
 * Return:          0 on success, -1 on error.
 */
int
xrtGraphResume(xrtGraphHandle gh);

/**
 * xrtGraphStop() - Stop a running graph if not done within a given time
 *                  interval.
 *
 * @gh:              Handle to graph previously opened with xrtGraphOpen.
 * @timeoutMilliSec: Timeout value before force to stop graph.
 *
 * Return:          0 on success, -1 on timeout.
 *
 * Note: Wait for done status of ALL the tiles
 */
int
xrtGraphStop(xrtGraphHandle gh, int timeoutMilliSec);

/**
 * xrtGraphUpdateRTP() - Update RTP value of port with hierarchical name
 *
 * @gh:              Handle to graph previously opened with xrtGraphOpen.
 * @hierPathPort:    hierarchial name of RTP port.
 * @buffer:          pointer to the RTP value.
 * @size:            size in bytes of the RTP value.
 *
 * Return:          0 on success, -1 on timeout.
 *
 * Note: This is for sychcronous RTP only.
 */
int
xrtGraphUpdateRTP(xrtGraphHandle gh, const char *hierPathPort, const char *buffer, size_t size);

#endif
