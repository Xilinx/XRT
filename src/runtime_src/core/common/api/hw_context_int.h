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

// get_xcl_handle() - Retrieve the driver handle index associated with the context
xcl_hwctx_handle
get_xcl_handle(const xrt::hw_context& ctx);

}} // hw_context_int, xrt_core

#endif
