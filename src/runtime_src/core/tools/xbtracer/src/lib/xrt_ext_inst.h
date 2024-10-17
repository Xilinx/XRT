// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

using xrt_ext_bo_ctor_cxt_s_a = xrt::ext::bo* (*)(void *, const xrt::hw_context& hwctx,\
        size_t sz, xrt::ext::bo::access_mode access);

using xrt_ext_kernel_ctor_ctx_m_s = xrt::ext::kernel* (*)(void *, const xrt::hw_context& hwctx,\
        const xrt::module& mod, const std::string& name);

struct xrt_ext_ftbl
{
  xrt_ext_bo_ctor_cxt_s_a bo_ctor_cxt_s_a;
  xrt_ext_kernel_ctor_ctx_m_s kernel_ctor_ctx_m_s;
};
