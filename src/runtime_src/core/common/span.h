// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrtcore_span_h_
#define xrtcore_span_h_

#include "core/include/xrt/detail/span.h"

namespace xrt_core {

// Use xrt_core::span in XRT internal code.
// std::span is C++20
template <typename T> using span = xrt::detail::span<T>;
  
}

#endif
