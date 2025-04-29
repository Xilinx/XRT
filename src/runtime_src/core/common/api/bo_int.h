// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
//
#ifndef _XRT_COMMON_BO_INT_H_
#define _XRT_COMMON_BO_INT_H_

// This file defines implementation extensions to the XRT BO APIs.
#include "core/common/config.h"
#include "core/include/xrt/xrt_bo.h"
#include "core/common/shim/buffer_handle.h"
#include "core/common/shim/hwctx_handle.h"

#include <map>

namespace xrt_core::bo_int {

XRT_CORE_COMMON_EXPORT  
xrt_core::buffer_handle*
get_buffer_handle(const xrt::bo& bo);

size_t
get_offset(const xrt::bo& bo);

// create_bo_helper() - Create a buffer object within a hwctx for specific
// use case
//
// Allocates a buffer object within a hwctx. All the public xrt::bo constructors
// doesnt use 64 bit flags. So this function acts as an extension to create buffer
// with specific use flag (debug/dtrace/log).
xrt::bo
create_bo_helper(const xrt::hw_context& hwctx, size_t sz, uint32_t use_flag);

// TODO : cleanup below create_debug_bo and create_dtrace_bo APIs after metadata
// bo design changes are checked in.

// create_debug_bo() - Create a debug buffer object within a hwctx
//  
// Allocates a debug buffer object within a hwctx. The debug BO
// is for sharing of driver / firmware data with user space XRT.
// The shim allocation is through hwctx_handle::alloc_bo with
// the XRT_BO_USE_DEBUG flag captured in extension flags.
XRT_CORE_COMMON_EXPORT
xrt::bo
create_debug_bo(const xrt::hw_context& hwctx, size_t sz);

// create_dtrace_bo() - Create a trace buffer object within a hwctx
//
// Allocates a buffer object within a hwctx used for dynamic tracing.
// The BO is used by driver / firmware to fill dynamic tracing data
// and is shared with user space XRT.
// The shim allocation is through hwctx_handle::alloc_bo with
// the XRT_BO_USE_DTRACE flag captured in extension flags.
XRT_CORE_COMMON_EXPORT
xrt::bo
create_dtrace_bo(const xrt::hw_context& hwctx, size_t sz);

// config_bo() - Configure the buffer object for the given use case (based on
// the flag passed)
//
// Configure the buffer object to be used for debug, dtrace, log, debug queue
// purpose based on the flag passed. The buffer object is tied to a slot using
// the hw ctx that is used to create the bo. A map of uc or column index and
// buffer size is used to split the buffer among the columns in the partition/slot.
XRT_CORE_COMMON_EXPORT
void
config_bo(const xrt::bo& bo, uint32_t flag, const std::map<uint32_t, size_t>& buf_sizes);

// unconfig_bo() - Unconfigure the buffer object configured earlier
//
// XRT Userspace can use this function to explicitly unconfigure the bo. It gives
// more control on when to unconfigure the bo instead of relying on buffer handle
// destruction
XRT_CORE_COMMON_EXPORT
void
unconfig_bo(const xrt::bo& bo);

} // bo_int, xrt_core

#endif
