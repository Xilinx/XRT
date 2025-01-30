// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef SHIM_INT_H_
#define SHIM_INT_H_

#include "core/include/xrt.h"
#include "core/include/xrt_hwqueue.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "core/common/cuidx_type.h"
#include "core/common/shim/buffer_handle.h"
#include "core/common/shim/hwctx_handle.h"
#include "core/common/shim/hwqueue_handle.h"
#include "core/common/shim/shared_handle.h"

#include <string>

namespace xrt {

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

// alloc_bo()
std::unique_ptr<xrt_core::buffer_handle>
alloc_bo(xclDeviceHandle, size_t size, unsigned int flags);

// alloc_userptr_bo()
std::unique_ptr<xrt_core::buffer_handle>
alloc_bo(xclDeviceHandle, void* userptr, size_t size, unsigned int flags);

// import_bo
std::unique_ptr<xrt_core::buffer_handle>
import_bo(xclDeviceHandle, xrt_core::shared_handle::export_handle);

// create_hw_context() -
std::unique_ptr<xrt_core::hwctx_handle>
create_hw_context(xclDeviceHandle handle,
                  const xrt::uuid& xclbin_uuid,
                  const xrt::hw_context::cfg_param_type& cfg_param,
                  xrt::hw_context::access_mode mode);

std::unique_ptr<xrt_core::hwctx_handle>
create_hw_context(xclDeviceHandle handle, uint32_t partition_size);

// get_hw_queue() -
xrt_core::hwqueue_handle*
get_hw_queue(xclDeviceHandle handle, xrt_core::hwctx_handle* ctxhdl);

// register_xclbin() -
void
register_xclbin(xclDeviceHandle handle, const xrt::xclbin& xclbin);

// submit_command() -
void
submit_command(xclDeviceHandle handle, xrt_core::hwqueue_handle* qhdl, xrt_core::buffer_handle* cmdbo);

// wait_command() -
int
wait_command(xclDeviceHandle handle, xrt_core::hwqueue_handle* qhdl, xrt_core::buffer_handle* cmdbo, int timeout_ms);

// exec_buf() - Exec Buf with hw ctx handle.
void
exec_buf(xclDeviceHandle handle, xrt_core::buffer_handle* bohdl, xrt_core::hwctx_handle* ctxhdl);

// get_buffer_handle - get xrt_core::buffer handle from
// raw handle returned by shim, this function is implemented
// only in edge shim
std::unique_ptr<xrt_core::buffer_handle>
get_buffer_handle(xclDeviceHandle handle, unsigned int bhdl);
}} // shim_int, xrt

#endif
