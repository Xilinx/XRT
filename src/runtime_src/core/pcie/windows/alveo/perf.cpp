/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

// For PCIe gen 3x16 or 4x8:
// Max BW = 16.0 * (128b/130b encoding) = 15.75385 GB/s
double xclGetHostReadMaxBandwidthMBps(xclDeviceHandle handle)
{
  return 15753.85;
}

// For PCIe gen 3x16 or 4x8:
// Max BW = 16.0 * (128b/130b encoding) = 15.75385 GB/s
double xclGetHostWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
  return 15753.85;
}

// For DDR4: Typical Max BW = 19.25 GB/s
double xclGetKernelReadMaxBandwidthMBps(xclDeviceHandle handle)
{
  return 19250.00;
}

// For DDR4: Typical Max BW = 19.25 GB/s
double xclGetKernelWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
  return 19250.00;
}

#if 0

void xclSetProfilingNumberSlots(xclDeviceHandle handle, enum xdp::MonitorType type,
                                uint32_t numSlots)
{
}

uint32_t xclGetProfilingNumberSlots(xclDeviceHandle handle, enum xdp::MonitorType type)
{
}

void xclGetProfilingSlotName(xclDeviceHandle handle, enum xdp::MonitorType type,
                             uint32_t slotnum, char* slotName, uint32_t length)
{
}

uint32_t xclGetProfilingSlotProperties(xclDeviceHandle handle, enum xdp::MonitorType type,
                                       uint32_t slotnum)
{
}

size_t xclPerfMonClockTraining(xclDeviceHandle handle, enum xdp::MonitorType type);

void xclPerfMonConfigureDataflow(xclDeviceHandle handle, enum xdp::MonitorType type, unsigned *ip_data);

size_t xclPerfMonStartCounters(xclDeviceHandle handle, enum xdp::MonitorType type);

size_t xclPerfMonStopCounters(xclDeviceHandle handle, enum xdp::MonitorType type);


size_t xclPerfMonReadCounters(xclDeviceHandle handle, enum xdp::MonitorType type,
                              xdp::CounterResults& counterResults);



size_t xclDebugReadIPStatus(xclDeviceHandle handle, enum xclDebugReadType type,
                                                       void* debugResults);


size_t xclPerfMonStartTrace(xclDeviceHandle handle, enum xdp::MonitorType type,
                                    uint32_t startTrigger);

size_t xclPerfMonStopTrace(xclDeviceHandle handle, enum xdp::MonitorType type);

uint32_t xclPerfMonGetTraceCount(xclDeviceHandle handle, enum xdp::MonitorType type);



size_t xclPerfMonReadTrace(xclDeviceHandle handle, enum xdp::MonitorType type,
                           xdp::TraceEventsVector& traceVector);

#endif
