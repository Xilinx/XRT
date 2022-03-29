/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef _XRT_COMMON_BO_H_
#define _XRT_COMMON_BO_H_

// This file defines implementation extensions to the XRT BO APIs.
#include "core/include/xrt/xrt_bo.h"
#include "core/common/config.h"
#include "core/include/ert.h"

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

// Fill the ERT copy BO command packet
XRT_CORE_COMMON_EXPORT
void
fill_copy_pkt(const xrt::bo& dst, const xrt::bo& src, size_t sz,
              size_t dst_offset, size_t src_offset, ert_start_copybo_cmd* pkt);

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
