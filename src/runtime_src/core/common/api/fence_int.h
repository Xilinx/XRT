// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
//
#ifndef _XRT_COMMON_FENCE_INT_H_
#define _XRT_COMMON_FENCE_INT_H_

// This file defines implementation extensions to the XRT Kernel APIs.
#include "core/include/xrt/experimental/xrt_fence.h"
#include "core/common/shim/fence_handle.h"

namespace xrt_core::fence_int {

xrt_core::fence_handle*
get_fence_handle(const xrt::fence& fence);

} // fence_int, xrt_core

#endif
