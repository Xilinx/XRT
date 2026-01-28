// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.
//
// Xilinx Runtime (XRT) Experimental APIs

#ifndef _XRT_COMMON_MODULE_INT_H_
#define _XRT_COMMON_MODULE_INT_H_

// This file defines implementation extensions to the XRT Kernel APIs.
#include "core/common/xclbin_parser.h"
#include "core/include/xrt/xrt_bo.h"
#include "core/include/xrt/experimental/xrt_module.h"
#include "elf_int.h"

#include "ert.h"

#include <string>

namespace xrt_core::module_int {

// create module object that will be used with run object
// The object created holds buffers for instruction/control-pkt
// These buffers are patched and sent to driver/firmware for execution
// If module has multiple control codes, ctrl_code_id is used to
// identify the control code that needs to be run.
// Also pre created ctrlpkt bo with data filled from ELF is passed
// to reduce overhead of bo creation during module object init
XRT_CORE_COMMON_EXPORT
xrt::module
create_module_run(const xrt::elf& elf, const xrt::hw_context& hwctx,
                  uint32_t ctrl_code_id, const xrt::bo& ctrlpkt_bo);

// Get the underlying elf handle from module object
XRT_CORE_COMMON_EXPORT
std::shared_ptr<xrt::elf_impl>
get_elf_handle(const xrt::module& module);

// Fill in ERT command payload in ELF flow. The payload is after extra_cu_mask
// and before CU arguments.
uint32_t*
fill_ert_dpu_data(const xrt::module& module, uint32_t *payload);

// Patch buffer object into control code at given argument
XRT_CORE_COMMON_EXPORT
void
patch(const xrt::module&, const std::string& argnm, size_t index, const xrt::bo& bo);

// Returns patch buffer size of the given module based on buffer type passed
// This API may be useful for developing unit test case at SHIM level
// New ELfs pack multiple control codes info in it, to identify which control code
// to run we use ctrl code id
XRT_CORE_COMMON_EXPORT
XRT_CORE_UNUSED
size_t
get_patch_buf_size(const xrt::module&, xrt_core::elf_patcher::buf_type type,
                   uint32_t ctrl_code_id = xrt_core::elf_int::no_ctrl_code_id);

// Extract control code buffer and patch it with addresses from all arguments.
// This API may be useful for developing unit test case at SHIM level where
// you do not have access to device related "xrt::" objects, but still want
// to obtain a patched control code buffer for device to run.
// This API expects buffer type that needs to be patched to identify which buffer
// to patch (control code, control pkt, save/restore buffer etc)
// New ELfs pack multiple control codes info in it, to identify which control code
// to run we use ctrl code id (group index)
XRT_CORE_COMMON_EXPORT
XRT_CORE_UNUSED
void
patch(const xrt::module&, uint8_t*, size_t, const std::vector<std::pair<std::string, uint64_t>>*,
      xrt_core::elf_patcher::buf_type, uint32_t ctrl_code_id = xrt_core::elf_int::no_ctrl_code_id);

// Patch scalar into control code at given argument
XRT_CORE_COMMON_EXPORT
void
patch(const xrt::module&, const std::string& argnm, size_t index, const void* value, size_t size);

// Check that all arguments have been patched and sync the buffer
// to device if necessary.  Throw if not all arguments have been
// patched.
void
sync(const xrt::module&);

// Dump dynamic trace buffer
// Buffer is dumped after the kernel run is finished
// Optional run_id parameter to generate unique filenames for multiple runs
void
dump_dtrace_buffer(const xrt::module& module, uint32_t run_id = 0);

// Returns buffer object associated with control scratchpad memory.
// This memory is created using ELF associated with run object.
// Throws if ELF doesn't contain scratchpad memory
xrt::bo
get_ctrl_scratchpad_bo(const xrt::module& module);

} // xrt_core::module_int

#endif
