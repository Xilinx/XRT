// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_RUNNER_DETAIL_CAPTURE_FNFWD_H_
#define XRT_RUNNER_DETAIL_CAPTURE_FNFWD_H_

// This file forward declares all functions that are
// are invoked by XRT_REPLAY_CAPTURE
#include "xrt/detail/span.h"
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace xrt {
class bo;
class bo_impl;
class elf_impl;
class run_impl;
class runlist_impl;
}

namespace xrt_core::capture::detail {

template <typename T>
using span = xrt::detail::span<T>;

void
bo_sync(const xrt::bo_impl*, int direction);

void
run_set_arg_at_index(const xrt::run_impl*, size_t, span<const uint8_t>);

void
run_set_arg_at_index(const xrt::run_impl*, size_t, const xrt::bo&);

void
run_start(const xrt::run_impl*);

void
run_wait(const xrt::run_impl*);

void
runlist_add_run(const xrt::runlist_impl*, const xrt::run_impl*);

void
runlist_start(const xrt::runlist_impl*);

void
runlist_wait(const xrt::runlist_impl*);

void
elf_ctor(const xrt::elf_impl* hdl, const void* data, size_t size);

void
elf_ctor(const xrt::elf_impl* hdl, std::istream& istr);

void
elf_ctor(const xrt::elf_impl* hdl, const std::string& fnm);

} // namespace

#endif
