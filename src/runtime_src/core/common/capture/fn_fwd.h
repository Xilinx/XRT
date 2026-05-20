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

template <typename T>
using span = xrt::detail::span<T>;

void
bo_sync(const xrt::bo_impl*, xclBOSyncDirection);

void
run_set_arg_at_index(const xrt::run_impl*, size_t, span<const uint8_t>);

void
run_set_arg_at_index(const xrt::run_impl*, size_t, const xrt::bo&);

void
start_frame(const xrt::run_impl*);

void
runlist_add_run(const xrt::runlist_impl*, const xrt::run_impl*);

void
start_frame(const xrt::runlist_impl*);

void
elf_ctor(const xrt::elf_impl* hdl, const void* data, size_t size);

void
elf_ctor(const xrt::elf_impl* hdl, std::istream& istr);

void
elf_ctor(const xrt::elf_impl* hdl, const std::string& fnm);

} // namespace

#endif
