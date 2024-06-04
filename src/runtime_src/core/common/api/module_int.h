// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
//
// Xilinx Runtime (XRT) Experimental APIs

#ifndef _XRT_COMMON_MODULE_INT_H_
#define _XRT_COMMON_MODULE_INT_H_

// This file defines implementation extensions to the XRT Kernel APIs.
#include "core/include/experimental/xrt_bo.h"
#include "core/include/experimental/xrt_module.h"

#include <string>

namespace xrt_core::module_int {

// Fill in ERT command payload in ELF flow. The payload is after extra_cu_mask
// and before CU arguments.
uint32_t*
fill_ert_dpu_data(const xrt::module& module, uint32_t *payload);

// Patch buffer object into control code at given argument
void
patch(const xrt::module&, const std::string& argnm, size_t index, const xrt::bo& bo);

// Patch scalar into control code at given argument
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

} // xrt_core::module_int

#endif
