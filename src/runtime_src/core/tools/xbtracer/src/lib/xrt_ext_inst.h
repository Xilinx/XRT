// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

using xrt_ext_bo_ctor_dev_up_s_a = xrt::ext::bo* (*)(void *, const xrt::device&, void*,\
        size_t, xrt::ext::bo::access_mode);
using xrt_ext_bo_ctor_dev_up_s = xrt::ext::bo* (*)(void *, const xrt::device&, void*,\
        size_t);
using xrt_ext_bo_ctor_dev_s_a = xrt::ext::bo* (*)(void *, const xrt::device&,\
        size_t, xrt::ext::bo::access_mode);
using xrt_ext_bo_ctor_dev_s = xrt::ext::bo* (*)(void *, const xrt::device&,\
        size_t);
using xrt_ext_bo_ctor_dev_pid_ehdl = xrt::bo* (*)(void*, const xrt::device&,\
        xrt::pid_type, xrt::bo::export_handle);
using xrt_ext_bo_ctor_cxt_s_a = xrt::ext::bo* (*)(void *, const xrt::hw_context& hwctx,\
        size_t sz, xrt::ext::bo::access_mode access);
using xrt_ext_bo_ctor_cxt_s = xrt::ext::bo* (*)(void *, const xrt::hw_context& hwctx,\
        size_t sz);
using xrt_ext_kernel_ctor_ctx_m_s = xrt::ext::kernel* (*)(void *, const xrt::hw_context&,\
        const xrt::module&, const std::string&);

struct xrt_ext_ftbl
{
  xrt_ext_bo_ctor_dev_up_s_a bo_ctor_dev_up_s_a;
  xrt_ext_bo_ctor_dev_up_s bo_ctor_dev_up_s;
  xrt_ext_bo_ctor_dev_s_a bo_ctor_dev_s_a;
  xrt_ext_bo_ctor_dev_s bo_ctor_dev_s;
  xrt_ext_bo_ctor_dev_pid_ehdl bo_ctor_dev_pid_ehdl;
  xrt_ext_bo_ctor_cxt_s_a bo_ctor_cxt_s_a;
  xrt_ext_bo_ctor_cxt_s bo_ctor_cxt_s;
  xrt_ext_kernel_ctor_ctx_m_s kernel_ctor_ctx_m_s;
};
