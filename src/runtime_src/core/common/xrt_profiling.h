/*
 * Copyright (C) 2019, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) APIs
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

#ifndef _XRT_PROFILING_H
#define _XRT_PROFILING_H

#include "core/include/xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * DOC: Performance Monitoring Operations
 *
 * These functions are used to read and write to the performance monitoring infrastructure.
 * OpenCL runtime will be using the BUFFER MANAGEMNT APIs described above to manage OpenCL buffers.
 * It would use these functions to initialize and sample the performance monitoring on the card.
 * Note that the offset is wrt the address space
 */

/* Write host event to device tracing (Zynq only) */

XCL_DRIVER_DLLESPEC void xclWriteHostEvent(xclDeviceHandle handle, enum xclPerfMonEventType type,
                                           enum xclPerfMonEventID id);

XCL_DRIVER_DLLESPEC size_t xclGetDeviceTimestamp(xclDeviceHandle handle);

XCL_DRIVER_DLLESPEC double xclGetDeviceClockFreqMHz(xclDeviceHandle handle);

XCL_DRIVER_DLLESPEC double xclGetReadMaxBandwidthMBps(xclDeviceHandle handle);

XCL_DRIVER_DLLESPEC double xclGetWriteMaxBandwidthMBps(xclDeviceHandle handle);

XCL_DRIVER_DLLESPEC void xclSetProfilingNumberSlots(xclDeviceHandle handle, enum xclPerfMonType type,
                                                            uint32_t numSlots);

XCL_DRIVER_DLLESPEC uint32_t xclGetProfilingNumberSlots(xclDeviceHandle handle, enum xclPerfMonType type);

XCL_DRIVER_DLLESPEC void xclGetProfilingSlotName(xclDeviceHandle handle, enum xclPerfMonType type,
                                                 uint32_t slotnum, char* slotName, uint32_t length);

XCL_DRIVER_DLLESPEC uint32_t xclGetProfilingSlotProperties(xclDeviceHandle handle, enum xclPerfMonType type,
                                                 uint32_t slotnum);

XCL_DRIVER_DLLESPEC size_t xclPerfMonClockTraining(xclDeviceHandle handle, enum xclPerfMonType type);

XCL_DRIVER_DLLESPEC void xclPerfMonConfigureDataflow(xclDeviceHandle handle, enum xclPerfMonType type, unsigned *ip_data);

XCL_DRIVER_DLLESPEC size_t xclPerfMonStartCounters(xclDeviceHandle handle, enum xclPerfMonType type);

XCL_DRIVER_DLLESPEC size_t xclPerfMonStopCounters(xclDeviceHandle handle, enum xclPerfMonType type);

#ifdef __cplusplus
XCL_DRIVER_DLLESPEC size_t xclPerfMonReadCounters(xclDeviceHandle handle, enum xclPerfMonType type,
                                                          xclCounterResults& counterResults);
#endif

#if 0
XCL_DRIVER_DLLESPEC size_t xclDebugReadIPStatus(xclDeviceHandle handle, enum xclDebugReadType type,
                                                                           void* debugResults);
#endif

XCL_DRIVER_DLLESPEC size_t xclPerfMonStartTrace(xclDeviceHandle handle, enum xclPerfMonType type,
                                                        uint32_t startTrigger);

XCL_DRIVER_DLLESPEC size_t xclPerfMonStopTrace(xclDeviceHandle handle, enum xclPerfMonType type);

XCL_DRIVER_DLLESPEC uint32_t xclPerfMonGetTraceCount(xclDeviceHandle handle, enum xclPerfMonType type);


#ifdef __cplusplus
XCL_DRIVER_DLLESPEC size_t xclPerfMonReadTrace(xclDeviceHandle handle, enum xclPerfMonType type,
                                                       xclTraceResultsVector& traceVector);
#endif


XCL_DRIVER_DLLESPEC int xclGetDebugIPlayoutPath(xclDeviceHandle handle, char* layoutPath, size_t size);

XCL_DRIVER_DLLESPEC int xclGetTraceBufferInfo(xclDeviceHandle handle, uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz);
XCL_DRIVER_DLLESPEC int xclReadTraceData(xclDeviceHandle handle, void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample);



#ifdef __cplusplus
}
#endif




#endif
