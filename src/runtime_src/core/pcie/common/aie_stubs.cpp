/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) Advanced Micro Devices, Inc.
 */
#include "core/include/xcl_graph.h"
#include "core/include/xrt/xrt_aie.h"
#include "core/include/xrt/xrt_graph.h"
#include "core/common/error.h"

// This file implements stubs for shim level AIE graph functions.  The
// file is used to expand the shim layer to include all graph level
// functions.  It is compiled into each shim core library that does
// not itself define these functions.
void*
xclGraphOpen(xclDeviceHandle, const xuid_t, const char*, xrt::graph::access_mode)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

void
xclGraphClose(xclGraphHandle)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclGraphReset(xclGraphHandle)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

uint64_t
xclGraphTimeStamp(xclGraphHandle)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclGraphRun(xclGraphHandle, int)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclGraphWaitDone(xclGraphHandle, int)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclGraphWait(xclGraphHandle, uint64_t)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclGraphSuspend(xclGraphHandle)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclGraphResume(xclGraphHandle)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclGraphEnd(xclGraphHandle, uint64_t)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclGraphUpdateRTP(xclGraphHandle, const char*, const char*, size_t)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclGraphReadRTP(xclGraphHandle, const char*, char*, size_t)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclAIEOpenContext(xclDeviceHandle, xrt::aie::access_mode)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclSyncBOAIE(xclDeviceHandle, xrt::bo&, const char*, enum xclBOSyncDirection, size_t, size_t)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclResetAIEArray(xclDeviceHandle)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclSyncBOAIENB(xclDeviceHandle, xrt::bo&, const char*, enum xclBOSyncDirection, size_t, size_t)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclGMIOWait(xclDeviceHandle, const char*)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclStartProfiling(xclDeviceHandle, int, const char*, const char*, uint32_t)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

uint64_t
xclReadProfiling(xclDeviceHandle, int)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclStopProfiling(xclDeviceHandle, int)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

int
xclLoadXclBinMeta(xclDeviceHandle, const xclBin*)
{
  throw xrt_core::error(std::errc::not_supported, __func__);
}

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
)
{
  throw xrt_core::error(std::errc::not_supported, __func__);;
}

int
xclEnqueueTask(xclDeviceHandle handle, int tileType, uint8_t column, uint8_t row, int dir, uint8_t channel, uint32_t repeatCount, bool enableTaskCompleteToken, uint8_t startBdId)
{
  throw xrt_core::error(std::errc::not_supported, __func__);;
}

int
xclWaitDMAChannelTaskQueue(xclDeviceHandle handle, int tileType, uint8_t column, uint8_t row, int dir, uint8_t channel)
{
  throw xrt_core::error(std::errc::not_supported, __func__);;
}

int
xclWaitDMAChannelDone(xclDeviceHandle handle, int tileType, uint8_t column, uint8_t row, int dir, uint8_t channel)
{
  throw xrt_core::error(std::errc::not_supported, __func__);;
}

int
xclInitializeLock(xclDeviceHandle handle, int tileType, uint8_t column, uint8_t row, unsigned short lockId, int8_t initVal)
{
  throw xrt_core::error(std::errc::not_supported, __func__);;
}

int
xclAcquireLock(xclDeviceHandle handle, int tileType, uint8_t column, uint8_t row, unsigned short lockId, int8_t acqVal)
{
  throw xrt_core::error(std::errc::not_supported, __func__);;
}

int
xclReleaseLock(xclDeviceHandle handle, int tileType, uint8_t column, uint8_t row, unsigned short lockId, int8_t relVal)
{
  throw xrt_core::error(std::errc::not_supported, __func__);;
}
