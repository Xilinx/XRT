// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
#ifndef SHIM_INT_H_
#define SHIM_INT_H_

#include "core/include/xrt.h"

// This file defines internal shim APIs, which is not end user visible.
// This header file should not be published to xrt release include/ folder.

#ifdef __cplusplus
extern "C" {
#endif

/**
 * xclOpenByBDF() - Open a device and obtain its handle by PCI BDF
 *
 * @bdf:           Deice PCE BDF
 * Return:         Device handle
 */
XCL_DRIVER_DLLESPEC
xclDeviceHandle
xclOpenByBDF(const char *bdf);

/**
 * xclOpenContextByName() - Open a shared/exclusive context on a named compute unit
 *
 * @handle:        Device handle
 * @slot:          Slot index of xclbin to service this context requiest
 * @xclbin_uuid:   UUID of the xclbin image with the CU to open a context on
 * @cuname:        Name of compute unit to open
 * @shared:        Shared access or exclusive access
 * Return:         0 on success, EAGAIN, or appropriate error number
 */
XCL_DRIVER_DLLESPEC
int
xclOpenContextByName(xclDeviceHandle handle, uint32_t slot, const xuid_t xclbin_uuid, const char* cuname, bool shared);

#ifdef __cplusplus
}
#endif

#endif
