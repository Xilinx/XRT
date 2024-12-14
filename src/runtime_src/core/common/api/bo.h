// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_COMMON_BO_H_
#define XRT_COMMON_BO_H_

// This file defines implementation extensions to the XRT BO APIs.
#include "core/include/xrt/xrt_bo.h"
#include "core/include/xrt/detail/ert.h"
#include "core/common/config.h"

namespace xrt_core { namespace bo {

// address() - Get physical device address of argument bo
uint64_t
address(const xrt::bo& bo);
uint64_t
address(xrtBufferHandle handle);

// group_id() - Get memory bank index of argument bo
uint32_t
group_id(const xrt::bo& bo);

// xcl_device_handle() - Get xcl device handle from BO
xclDeviceHandle
device_handle(const xrt::bo& bo);

// get_flags() - Get the flags used when BO was created
xrt::bo::flags
get_flags(const xrt::bo& bo);

// clone() - Clone src bo into target memory bank
xrt::bo
clone(const xrt::bo& src, xrt::memory_group target_grp);

// Check if this BO has been imported from another device
XRT_CORE_COMMON_EXPORT
bool
is_imported(const xrt::bo& bo);

XRT_CORE_COMMON_EXPORT
bool
is_aligned_ptr(const void* ptr);

XRT_CORE_COMMON_EXPORT
size_t
alignment();

}} // namespace bo, xrt_core

#endif
