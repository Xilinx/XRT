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

XCL_DRIVER_DLLESPEC size_t xclGetDeviceTimestamp(xclDeviceHandle handle);

XCL_DRIVER_DLLESPEC double xclGetDeviceClockFreqMHz(xclDeviceHandle handle);

XCL_DRIVER_DLLESPEC double xclGetReadMaxBandwidthMBps(xclDeviceHandle handle);

XCL_DRIVER_DLLESPEC double xclGetWriteMaxBandwidthMBps(xclDeviceHandle handle);

XCL_DRIVER_DLLESPEC void xclGetDebugIpLayout(xclDeviceHandle hdl, char* buffer, size_t size, size_t* size_ret);

XCL_DRIVER_DLLESPEC int xclGetDebugIPlayoutPath(xclDeviceHandle handle, char* layoutPath, size_t size);

XCL_DRIVER_DLLESPEC int xclGetTraceBufferInfo(xclDeviceHandle handle, uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz);

XCL_DRIVER_DLLESPEC int xclReadTraceData(xclDeviceHandle handle, void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample);

XCL_DRIVER_DLLESPEC int xclGetSubdevPath(xclDeviceHandle handle, const char* subdev, uint32_t idx, char* path, size_t size);

#ifdef __cplusplus
}
#endif




#endif
