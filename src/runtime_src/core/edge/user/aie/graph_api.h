// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _GRAPH_API_H_
#define _GRAPH_API_H_

typedef xclDeviceHandle xrtDeviceHandle;

namespace graph_api {

void
aie_open_context(xclDeviceHandle handle, xrt::aie::access_mode am);

void
sync_bo_aie(xclDeviceHandle handle, xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, 
                size_t size, size_t offset, zynqaie::Aie* aie_array);

void
sync_bo_aie_nb(xclDeviceHandle handle, xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, 
                   size_t size, size_t offset, zynqaie::Aie* aie_array);

void
reset_aie_array(xclDeviceHandle handle, zynqaie::Aie* aie_array);

void
gmio_wait(xclDeviceHandle handle, const char *gmioName, zynqaie::Aie* aie_array);

int
start_profiling(xclDeviceHandle handle, int option, const char* port1Name, 
                  const char* port2Name, uint32_t value, zynqaie::Aie* aie_array);

uint64_t
read_profiling(xclDeviceHandle handle, int phdl, zynqaie::Aie* aie_array);

void
stop_profiling(xclDeviceHandle handle, int phdl, zynqaie::Aie* aie_array);

} // graph_api

#endif