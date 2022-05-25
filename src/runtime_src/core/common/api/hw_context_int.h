// PDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_COMMON_API_HW_CONTEXT_INT_H
#define XRT_COMMON_API_HW_CONTEXT_INT_H

// This file defines implementation extensions to the XRT XCLBIN APIs.
#include "core/include/experimental/xrt_hw_context.h"

#include <cstdint>

// Provide access to xrt::xclbin data that is not directly exposed
// to end users via xrt::xclbin.   These functions are used by
// XRT core implementation.
namespace xrt_core { namespace hw_context_int {

// get_xcl_handle() - Driver handle index
// Retrieve the driver handle index associated with the context
xcl_hwctx_handle
get_xcl_handle(const xrt::hw_context& ctx);

// Get a raw pointer to the core device associated with
// the hw context
xrt_core::device*
get_core_device_raw(const xrt::hw_context& ctx);

// Backdoor for changing qos of a hardware context after it has
// been constructed.  The new qos affects how compute units are
// within the context are opened.  This is used for legacy
// xrt::kernel objects associated with a mailbox
void
set_exclusive(xrt::hw_context& ctx);

}} // hw_context_int, xrt_core

#endif
