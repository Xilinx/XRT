/**
 * Copyright (C) 2019 Xilinx, Inc
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
#define XCL_DRIVER_DLL_EXPORT
#define XRT_CORE_PCIE_WINDOWS_SOURCE
#include "common/xrt_profiling.h"
#include "experimental/xrt-next.h"
#include <stdexcept>

#pragma warning(disable : 4100)

double
xclGetDeviceClockFreqMHz(xclDeviceHandle handle)
{
  return 0.0;
}

int
xclGetDebugIPlayoutPath(xclDeviceHandle handle, char* layoutPath, size_t size)
{
  return 1;
}

uint32_t
xclGetNumLiveProcesses(xclDeviceHandle handle)
{
  return 0;
}

size_t
xclGetDeviceTimestamp(xclDeviceHandle handle)
{
  return 0;
}

double xclGetReadMaxBandwidthMBps(xclDeviceHandle handle)
{
   return 0.0;
}

#if 0


double xclGetWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
}

void xclSetProfilingNumberSlots(xclDeviceHandle handle, enum xclPerfMonType type,
                                uint32_t numSlots)
{
}

uint32_t xclGetProfilingNumberSlots(xclDeviceHandle handle, enum xclPerfMonType type)
{
}

void xclGetProfilingSlotName(xclDeviceHandle handle, enum xclPerfMonType type,
                             uint32_t slotnum, char* slotName, uint32_t length)
{
}

uint32_t xclGetProfilingSlotProperties(xclDeviceHandle handle, enum xclPerfMonType type,
                                       uint32_t slotnum)
{
}

size_t xclPerfMonClockTraining(xclDeviceHandle handle, enum xclPerfMonType type);

void xclPerfMonConfigureDataflow(xclDeviceHandle handle, enum xclPerfMonType type, unsigned *ip_data);

size_t xclPerfMonStartCounters(xclDeviceHandle handle, enum xclPerfMonType type);

size_t xclPerfMonStopCounters(xclDeviceHandle handle, enum xclPerfMonType type);


size_t xclPerfMonReadCounters(xclDeviceHandle handle, enum xclPerfMonType type,
                                      xclCounterResults& counterResults);



size_t xclDebugReadIPStatus(xclDeviceHandle handle, enum xclDebugReadType type,
                                                       void* debugResults);


size_t xclPerfMonStartTrace(xclDeviceHandle handle, enum xclPerfMonType type,
                                    uint32_t startTrigger);

size_t xclPerfMonStopTrace(xclDeviceHandle handle, enum xclPerfMonType type);

uint32_t xclPerfMonGetTraceCount(xclDeviceHandle handle, enum xclPerfMonType type);



size_t xclPerfMonReadTrace(xclDeviceHandle handle, enum xclPerfMonType type,
                                   xclTraceResultsVector& traceVector);

#endif
