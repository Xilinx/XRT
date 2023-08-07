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

// Provide access to underlying xrt::bo representing the instruction
// buffer
const std::vector<std::pair<uint64_t, uint64_t>>&
get_ctrlcode_addr_and_size(const xrt::module& module);


// Patch buffer object into control code at given argument
void
patch(const xrt::module&, const std::string& argnm, const xrt::bo& bo);

// Patch scalar into control code at given argument
void
patch(const xrt::module&, const std::string& argnm, const void* value, size_t size);

// Check that all arguments have been patched and sync the buffer
// to device if necessary.  Throw if not all arguments have been
// patched.
void
sync(const xrt::module&);

} // xrt_core::module_int

#endif
