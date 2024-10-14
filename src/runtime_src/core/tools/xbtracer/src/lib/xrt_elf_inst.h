// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "experimental/xrt_elf.h"

/*
 * elf class method aliases.
 * */
using xrt_elf_ctor_str = xrt::elf* (*) (void*, const std::string& fnm);
using xrt_elf_ctor_ist = xrt::elf* (*) (void*, std::istream& stream);
using xrt_elf_get_cfg_uuid = xrt::uuid (xrt::elf::*) (void) const;

/*
 * struct xrt_elf_dtor definition.
 */
struct xrt_elf_ftbl
{
  xrt_elf_ctor_str         ctor_str;
  xrt_elf_ctor_ist         ctor_ist;
  xrt_elf_get_cfg_uuid     get_cfg_uuid;
};
