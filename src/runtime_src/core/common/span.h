// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrtcore_span_h_
#define xrtcore_span_h_

#include <gsl/span>

namespace xrt_core {
template <typename T>
using span = gsl::span<T>;
}

#endif
