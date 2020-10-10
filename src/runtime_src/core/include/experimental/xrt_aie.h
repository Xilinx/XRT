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
#include "experimental/xrt_uuid.h"
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"

typedef void *xrtGraphHandle;

/**
 * xrtGraphOpen() - Open a graph and obtain its handle.
 *
 * @handle:       Handle to the device with the graph.
 * @xclbinUUID:   UUID of the xclbin with the specified graph.
 * @graphNmae:    The name of graph to be open.
 * Return:        Handle to representing the graph. NULL for error.
 *
 * An xclbin with the specified graph must have been loaded prior
 * to calling this function.
 */
xrtGraphHandle
xrtGraphOpen(xrtDeviceHandle handle, const uuid_t xclbinUUID, const char *graphName);

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
 * xrtGraphRun() - Start a graph execution
 *
 * @gh:             Handle to graph previously opened with xrtGraphOpen.
 * @iterations:     The run iteration to update to graph.
 *                  0 for default or previous set iterations
 *                  -1 for run forever
 * Return:          0 on success, -1 on error
 *
 * Note: Run by enable tiles and disable tile reset
 */
int
xrtGraphRun(xrtGraphHandle gh, int iterations);

/**
 * xrtGraphWaitDone() - Wait for graph to be done. If the graph is not
 *                      done in a given time, bail out with timeout.
 *
 * @gh:              Handle to graph previously opened with xrtGraphOpen.
 * @timeoutMilliSec: Timeout value to wait for graph done.
 *
 * Return:          0 on success, -ETIME on timeout, -1 on error.
 *
 * Note: Wait for done status of ALL the tiles
 */
int
xrtGraphWaitDone(xrtGraphHandle gh, int timeoutMilliSec);

/**
 * xrtGraphWait() -  Wait a given AIE cycle since the last xrtGraphRun and
 *                   then stop the graph. If cycle is 0, busy wait until graph
 *                   is done. If graph already run more than the given
 *                   cycle, stop the graph immediateley.
 *
 * @gh:              Handle to graph previously opened with xrtGraphOpen.
 * @cycle:           AIE cycle should wait since last xrtGraphRun. 0 for
 *                   wait until graph is done.
 *
 * Return:          0 on success, -1 on error.
 *
 * Note: This API with non-zero AIE cycle is for graph that is running
 * forever or graph that has multi-rate core(s).
 */
int
xrtGraphWait(xrtGraphHandle gh, uint64_t cycle);

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
 * xrtGraphEnd() - Wait a given AIE cycle since the last xrtGraphRun and
 *                 then end the graph. If cycle is 0, busy wait until graph
 *                 is done before end the graph. If graph already run more
 *                 than the given cycle, stop the graph immediately and end it.
 *
 * @gh:              Handle to graph previously opened with xrtGraphOpen.
 * @cycle:           AIE cycle should wait since last xrtGraphRun. 0 for
 *                   wait until graph is done.
 *
 * Return:          0 on success, -1 on timeout.
 *
 * Note: This API with non-zero AIE cycle is for graph that is running
 * forever or graph that has multi-rate core(s).
 */
int
xrtGraphEnd(xrtGraphHandle gh, uint64_t cycle);

/**
 * xrtGraphUpdateRTP() - Update RTP value of port with hierarchical name
 *
 * @gh:              Handle to graph previously opened with xrtGraphOpen.
 * @hierPathPort:    hierarchial name of RTP port.
 * @buffer:          pointer to the RTP value.
 * @size:            size in bytes of the RTP value.
 *
 * Return:          0 on success, -1 on error.
 */
int
xrtGraphUpdateRTP(xrtGraphHandle gh, const char *hierPathPort, const char *buffer, size_t size);

/**
 * xrtGraphReadRTP() - Read RTP value of port with hierarchical name
 *
 * @gh:              Handle to graph previously opened with xrtGraphOpen.
 * @hierPathPort:    hierarchial name of RTP port.
 * @buffer:          pointer to the buffer that RTP value is copied to.
 * @size:            size in bytes of the RTP value.
 *
 * Return:          0 on success, -1 on error.
 *
 * Note: Caller is reponsible for allocating enough memory for RTP value
 *       being copied to.
 */
int
xrtGraphReadRTP(xrtGraphHandle gh, const char *hierPathPort, char *buffer, size_t size);

/**
 * xrtSyncBOAIE() - Transfer data between DDR and Shim DMA channel
 *
 * @handle:          Handle to the device
 * @bohdl:           BO handle.
 * @gmioName:        GMIO name
 * @dir:             GM to AIE or AIE to GM
 * @size:            Size of data to synchronize
 * @offset:          Offset within the BO
 *
 * Return:          0 on success, -1 on error.
 *
 * Synchronize the buffer contents between GMIO and AIE.
 * Note: Upon return, the synchronization is done or error out
 */
int
xrtSyncBOAIE(xrtDeviceHandle handle, xrtBufferHandle bohdl, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset);

/**
 * xrtResetAIEArray() - Reset the AIE array
 *
 * @handle:         Handle to the device.
 *
 * Return:          0 on success, -1 on error.
 */
int
xrtResetAIEArray(xrtDeviceHandle handle);
#endif
