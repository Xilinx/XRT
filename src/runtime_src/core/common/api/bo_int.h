// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
//
#ifndef _XRT_COMMON_BO_INT_H_
#define _XRT_COMMON_BO_INT_H_

// This file defines implementation extensions to the XRT BO APIs.
#include "core/include/xrt/xrt_bo.h"
#include "core/common/shim/buffer_handle.h"

namespace xrt_core::bo_int {

const xrt_core::buffer_handle*
get_buffer_handle(const xrt::bo& bo);

size_t
get_offset(const xrt::bo& bo);

} // bo_int, xrt_core

#endif
