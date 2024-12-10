// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "xrt/xrt_hw_context.h"

/*
 * hw_context class method aliases.
 */
using cfg_param_type = std::map<std::string, uint32_t>;
using xrt_hw_context_ctor_frm_cfg = xrt::hw_context* (*) (void*,\
  const xrt::device&, const xrt::uuid&, const xrt::hw_context::cfg_param_type&);
using xrt_hw_context_ctor_frm_mode = xrt::hw_context* (*) (void*,\
  const xrt::device&, const xrt::uuid&, xrt::hw_context::access_mode mode);
using xrt_hw_context_update_qos = void (xrt::hw_context::*)\
  (const cfg_param_type&);
using xrt_hw_context_get_device = xrt::uuid (xrt::hw_context::*) (void) const;
using xrt_hw_context_get_xclbin = xrt::xclbin (xrt::hw_context::*) (void) const;
using xrt_hw_context_get_mode = xrt::hw_context::access_mode \
  (xrt::hw_context::*) (void) const;
using xrt_hw_context_dtor = void (xrt::hw_context::*) (void);

/*
 * struct xrt_hw_context_ftbl definition.
 */
struct xrt_hw_context_ftbl {
  xrt_hw_context_ctor_frm_cfg     ctor_frm_cfg;
  xrt_hw_context_ctor_frm_mode    ctor_frm_mode;
  xrt_hw_context_update_qos       update_qos;
  xrt_hw_context_get_device       get_device;
  xrt_hw_context_get_xclbin       get_xclbin;
  xrt_hw_context_get_mode         get_mode;
  xrt_hw_context_dtor             dtor;
};
