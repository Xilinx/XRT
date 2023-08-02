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

} // xrt_core::module_int

#endif
