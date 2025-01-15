// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef _XRT_COMMON_ELF_INT_H_
#define _XRT_COMMON_ELF_INT_H_

// This file defines implementation extensions to the XRT ELF APIs.
#include "core/include/xrt/experimental/xrt_elf.h"
#include <cstdint>
#include <string>
#include <vector>

namespace ELFIO { class elfio; }

// Provide access to xrt::elf data that is not directly exposed
// to end users via xrt::elf.   These functions are used by
// XRT core implementation.
namespace xrt_core { namespace elf_int {

// Extract section data from ELF file
std::vector<uint8_t>
get_section(const xrt::elf& elf, const std::string& sname);

const ELFIO::elfio&
get_elfio(const xrt::elf& elf);
    

}} // xclbin_int, xrt_core

#endif
