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
  debug       = XRT_BO_USE_DEBUG,       // debug data
  kmd         = XRT_BO_USE_KMD,         // shared with kernel mode driver
  dtrace      = XRT_BO_USE_DTRACE,      // dynamic trace data
  log         = XRT_BO_USE_LOG,         // logging info
  debug_queue = XRT_BO_USE_DEBUG_QUEUE, // debug queue data
  uc_debug    = XRT_BO_USE_UC_DEBUG,    // microblaze debug data
  host_only   = XRT_BO_USE_HOST_ONLY,   // system memory buffer
  instruction = XRT_BO_USE_INSTRUCTION, // instruction (instructon buffer)
  preemption  = XRT_BO_USE_PREEMPTION,  // preemption data
  scratch_pad = XRT_BO_USE_SCRATCH_PAD, // scratch pad data
  pdi         = XRT_BO_USE_PDI,         // PDI data
  ctrlpkt     = XRT_BO_USE_CTRLPKT      // control packet
};

// create_bo() - Create a buffer object within a device for specific use case
//
// Allocates a buffer object in the given device. All the public
// xrt::bo constructors doesnt use 64 bit flags. So this function acts
// as an extension to create buffer with specific use flag
// (debug/dtrace/log). This API is useful for creating buffers that
// outlive hw context.
XRT_CORE_COMMON_EXPORT
xrt::bo
create_bo(const std::shared_ptr<xrt_core::device>& m_core_device, size_t sz, use_type type);

// create_bo() - Create a buffer object within a hwctx for specific use case
//
// Allocates a buffer object within a hwctx. All the public xrt::bo
// constructors doesnt use 64 bit flags. So this function acts as an
// extension to create buffer with specific use flag
// (debug/dtrace/log).
XRT_CORE_COMMON_EXPORT
xrt::bo
create_bo(const xrt::hw_context& hwctx, size_t sz, use_type type);

// config_bo() - Configure the buffer object for the given use case (based on
// the flag passed)
//
// Configure the buffer object to be used for debug, dtrace, log,
// debug queue purpose based on buffer type. The buffer object is tied
// to a slot using the hw ctx passed to this call. If the hwctx passed
// in null then hwctx that is used to create the bo is used. A map of
// uc or column index and buffer size is used to split the buffer
// among the columns in the partition/slot.
XRT_CORE_COMMON_EXPORT
void
config_bo(const xrt::bo& bo, const std::map<uint32_t, size_t>& buf_sizes,
          const xrt_core::hwctx_handle* ctx_handle = nullptr);

// unconfig_bo() - Unconfigure the buffer object configured earlier
//
// XRT Userspace can use this function to explicitly unconfigure the
// bo. It gives more control on when to unconfigure the bo instead of
// relying on buffer handle destruction NOTE: The caller must ensure
// that the same buffer object and context handle used in the
// corresponding config_bo() call are passed to this function.
XRT_CORE_COMMON_EXPORT
void
unconfig_bo(const xrt::bo& bo, const xrt_core::hwctx_handle* ctx_handle = nullptr);

} // bo_int, xrt_core

#endif
