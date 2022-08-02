// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef SHIM_INT_H_
#define SHIM_INT_H_

#include "core/include/xrt.h"
#include "core/common/cuidx_type.h"

#include <string>

namespace xrt {

class hw_context;
class xclbin;
class uuid;

namespace shim_int {

// This file defines internal shim APIs, which is not end user visible.
// This header file should not be published to xrt release include/ folder.

// open_by_bdf() - Open a device and obtain its handle by PCI BDF
//
// @bdf:           Deice PCE BDF
// Return:         Device handle
//
// Throws on error
XCL_DRIVER_DLLESPEC
xclDeviceHandle
open_by_bdf(const std::string& bdf);

// open_cu_context() - Open a shared/exclusive context on a named compute unit
//
// @handle:        Device handle
// @hwctx:         Hardware context in which to open the CU
// @cuname:        Name of compute unit to open
// Returns:        The cuidx assigned by the driver
//
// Throws on error
xrt_core::cuidx_type
open_cu_context(xclDeviceHandle handle, const xrt::hw_context& hwctx, const std::string& cuname);

// close_cu_context() - Close a previously opened CU context
//
// @handle:        Device handle
// @hwctx:         The hardware context in which this CU was opened
// @cuidx:         UUID of the xclbin image with the CU to open a context on
//
// Throws on error, e.g. the CU context was not opened previously
void
close_cu_context(xclDeviceHandle handle, const xrt::hw_context& hwctx, xrt_core::cuidx_type cuidx);

// create_hw_context() -
uint32_t // ctxhdl aka slotidx
create_hw_context(xclDeviceHandle handle, const xrt::uuid& xclbin_uuid, uint32_t qos);

// dsstroy_hw_context() -
void
destroy_hw_context(xclDeviceHandle handle, uint32_t ctxhdl);

// register_xclbin() -
void
register_xclbin(xclDeviceHandle handle, const xrt::xclbin& xclbin);

}} // shim_int, xrt

#endif
