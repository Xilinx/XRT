// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "experimental/xrt_module.h"

/*
 * module class method aliases.
 * */
using xrt_module_ctor_elf = xrt::module* (*) (void*, const xrt::elf& elf);
using xrt_module_ctor_usr_sz_uuid = xrt::module* (*) (void*, void* userptr, size_t sz, const xrt::uuid& uuid);
using xrt_module_ctor_mod_ctx = xrt::module* (*) (void*, const xrt::module& parent, const xrt::hw_context& hwctx);
using xrt_module_get_cfg_uuid = xrt::uuid (xrt::module::*) (void) const;
using xrt_module_get_hw_context = xrt::hw_context (xrt::module::*) (void) const;

/*
 * struct xrt_module_dtor definition.
 */
struct xrt_module_ftbl
{
  xrt_module_ctor_elf         ctor_elf;
  xrt_module_ctor_usr_sz_uuid ctor_usr_sz_uuid;
  xrt_module_ctor_mod_ctx     ctor_mod_ctx;
  xrt_module_get_cfg_uuid     get_cfg_uuid;
  xrt_module_get_hw_context   get_hw_context;
};
