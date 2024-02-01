/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
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
