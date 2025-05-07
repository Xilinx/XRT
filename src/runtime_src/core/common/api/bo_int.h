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

// enum for different buffer use flags
// This is for internal use only
enum class use_type {
  normal = XRT_BO_USE_NORMAL, // Normal usage
  debug = XRT_BO_USE_DEBUG, // For debug data
  kmd = XRT_BO_USE_KMD, // Shared with kernel mode driver
  dtrace = XRT_BO_USE_DTRACE, // For dynamic trace data
  log = XRT_BO_USE_LOG, // For logging info
  debug_queue = XRT_BO_USE_DEBUG_QUEUE // For debug queue data
};

// create_bo() - Create a buffer object within a hwctx for specific use case
//
// Allocates a buffer object within a hwctx. All the public xrt::bo constructors
// doesnt use 64 bit flags. So this function acts as an extension to create buffer
// with specific use flag (debug/dtrace/log).
XRT_CORE_COMMON_EXPORT
xrt::bo
create_bo(const xrt::hw_context& hwctx, size_t sz, use_type type);

// config_bo() - Configure the buffer object for the given use case (based on
// the flag passed)
//
// Configure the buffer object to be used for debug, dtrace, log, debug queue
// purpose based on buffer type. The buffer object is tied to a slot using
// the hw ctx that is used to create the bo. A map of uc or column index and
// buffer size is used to split the buffer among the columns in the partition/slot.
XRT_CORE_COMMON_EXPORT
void
config_bo(const xrt::bo& bo, const std::map<uint32_t, size_t>& buf_sizes);

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
