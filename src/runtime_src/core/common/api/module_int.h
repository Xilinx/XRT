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

#include "ert.h"

#include <string>

namespace xrt_core::patcher {
// enum with different buffer types that supports patching
// Some of the internal shim tests use this enum, so moving it to header
// Ideal place for this enum would be in a header that has patching logic
// TODO: Move this when patching logic is added to aiebu 
enum class buf_type {
  ctrltext = 0,        // control code
  ctrldata = 1,        // control packet
  preempt_save = 2,    // preempt_save
  preempt_restore = 3, // preempt_restore
  pdi = 4,             // pdi
  ctrlpkt_pm = 5,      // preemption ctrl pkt
  pad = 6,             // scratchpad/control packet section name for next gen aie devices
  dump = 7,            // dump section containing debug info for trace etc
  buf_type_count = 8   // total number of buf types
};
}

namespace xrt_core::module_int {
struct kernel_info {
  std::vector<xrt_core::xclbin::kernel_argument> args;
  xrt_core::xclbin::kernel_properties props;
};

// Elfs with no multi control code support use below id as
// grp index or control code id
static constexpr uint32_t no_ctrl_code_id = UINT32_MAX;

// create module object that will be used with run object
// The object created holds buffers for instruction/control-pkt
// These buffers are patched and sent to driver/firmware for execution
// If module has multiple control codes, ctrl_code_id is used to
// identify the control code that needs to be run.
// Also pre created ctrlpkt bo with data filled from ELF is passed
// to reduce overhead of bo creation during module object init
xrt::module
create_run_module(const xrt::module& parent, const xrt::hw_context& hwctx,
                  uint32_t ctrl_code_id, const xrt::bo& ctrlpkt_bo);

// Get control code id from kernel name given to construct xrt::kernel
// Throws exception if this kernel is not present in ELF
uint32_t
get_ctrlcode_id(const xrt::module& module, const std::string& kname);

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
get_patch_buf_size(const xrt::module&, xrt_core::patcher::buf_type, uint32_t id = no_ctrl_code_id);

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
      xrt_core::patcher::buf_type, uint32_t id = no_ctrl_code_id);

// Patch scalar into control code at given argument
XRT_CORE_COMMON_EXPORT
void
patch(const xrt::module&, const std::string& argnm, size_t index, const void* value, size_t size);

// Check that all arguments have been patched and sync the buffer
// to device if necessary.  Throw if not all arguments have been
// patched.
void
sync(const xrt::module&);

// Get the ERT command opcode in ELF flow
ert_cmd_opcode
get_ert_opcode(const xrt::module& module);

// Dump scratch pad mem buffer
void
dump_scratchpad_mem(const xrt::module& module);

// Returns vector of kernel info extracted from demangled kernel signatures
// kernel signature eg : DPU(void*, void*, void*)
// Each kernel info object holds kernel name (DPU), kernel args and kernel properties
// returns empty vector if ELF doesnt have kernel signatures
const std::vector<kernel_info>&
get_kernels_info(const xrt::module& module);

// Dump dynamic trace buffer
// Buffer is dumped after the kernel run is finished
void
dump_dtrace_buffer(const xrt::module& module);

// Returns buffer object associated with control scratchpad memory.
// This memory is created using ELF associated with run object.
// Throws if ELF doesn't contain scratchpad memory
xrt::bo
get_ctrl_scratchpad_bo(const xrt::module& module);

// Returns ctrlpkt section data in ELF
// This data is used to create ctrlpkt buffers ahead in xrt::kernel object
// returns empty vector if ctrlpkt section is not present
std::vector<uint8_t>
get_ctrlpkt_data(const xrt::module& module, uint32_t ctrl_code_id);

} // xrt_core::module_int

#endif
