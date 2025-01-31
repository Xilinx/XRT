// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
//
#ifndef _XRT_COMMON_BO_INT_H_
#define _XRT_COMMON_BO_INT_H_

// This file defines implementation extensions to the XRT BO APIs.
#include "core/common/config.h"
#include "core/include/xrt/xrt_bo.h"
#include "core/common/shim/buffer_handle.h"

namespace xrt_core::bo_int {

XRT_CORE_COMMON_EXPORT  
xrt_core::buffer_handle*
get_buffer_handle(const xrt::bo& bo);

size_t
get_offset(const xrt::bo& bo);

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

} // bo_int, xrt_core

#endif
