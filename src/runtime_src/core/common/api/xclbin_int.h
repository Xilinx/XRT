/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 */
#ifndef _XRT_COMMON_XCLBIN_INT_H_
#define _XRT_COMMON_XCLBIN_INT_H_

// This file defines implementation extensions to the XRT XCLBIN APIs.
#include "core/include/experimental/xrt_xclbin.h"

namespace xrt_core {
namespace xclbin_int {

// get_axlf() - Retrieve complete axlf from handle
const axlf*
get_axlf(xrtXclbinHandle);

// get_xclbin() - Convert handle to object
xrt::xclbin
get_xclbin(xrtXclbinHandle);

// get_axlf_section() - Retrieve specified section
std::pair<const char*, size_t>
get_axlf_section(const xrt::xclbin& xclbin, axlf_section_kind kind);

// read_xclbin() - Read specified xclbin file 
std::vector<char>
read_xclbin(const std::string& fnm);

} //xclbin_int
}; // xrt_core

#endif
