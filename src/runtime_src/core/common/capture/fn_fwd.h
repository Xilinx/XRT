// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_COMMON_CAPTURE_FNFWD_H_
#define XRT_COMMON_CAPTURE_FNFWD_H_
// This file forward declares all functions that are
// are invoked by XRT_RECIPE_CAPTURE

#include "xrt/detail/span.h"
#include <cstdint>

namespace xrt {
class bo;
class run_impl;
class runlist_impl;
}

namespace xrt_core::capture {

void
run_set_arg_at_index(const xrt::run_impl*, size_t, xrt::detail::span<const uint8_t>);

void
run_set_arg_at_index(const xrt::run_impl*, size_t, const xrt::bo&);

void
start_frame(const xrt::run_impl*);

void
runlist_add_run(const xrt::runlist_impl*, const xrt::run_impl*);

void
start_frame(const xrt::runlist_impl*);

} // namespace

#endif
