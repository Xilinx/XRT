// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

// This file implements XRT pscontext internals as specified in
// core/include/experimental/xrt_pscontext.h
#include "core/include/experimental/xrt_pscontext.h"

namespace xrt {

// class pscontext_impl - XRT internal context for PS kernels
//
class pscontext_impl {
private:
  bool aie_profile_en = false;
};

} // xrt
