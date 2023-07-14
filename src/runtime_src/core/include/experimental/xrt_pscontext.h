// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_PSCONTEXT_H_
#define XRT_PSCONTEXT_H_

#include "xrt/detail/config.h"
#include "xrt/detail/pimpl.h"
#include "xclhal2.h"

#ifdef __cplusplus
#include <memory>

/*
 * PS Context Data Structure included by user PS kernel code
 */

namespace xrt {

class pscontext_impl;
class pscontext : public detail::pimpl<pscontext_impl> {
public:
  pscontext() = default;
};

}
 
#else
# error xrt_pscontext is only implemented for C++
#endif // __cplusplus

#endif
