// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
//
#ifndef XRT_COMMON_IP_INT_H_
#define XRT_COMMON_IP_INT_H_

// This file defines implementation extensions to the XRT BO APIs.
#include "core/common/config.h"
#include "core/include/xrt/experimental/xrt_ip.h"

namespace xrt_core::ip_int {

XCL_DRIVER_DLLESPEC
void
set_read_range(const xrt::ip& ip, uint32_t start, uint32_t size);

} // xrt_core::ip_int

#endif
