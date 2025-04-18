// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021 Xilinx, Inc. All rights reserved.
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_DETAIL_ABI_H
#define XRT_DETAIL_ABI_H

// Generated version.h file is installed into include/xrt/detail/version.h
// but at build time it is picked up from compile include search path
#if defined(XRT_BUILD) && !defined(DISABLE_ABI_CHECK)
# include "version.h"
#elif !defined(XRT_BUILD)
# include "xrt/detail/version.h"
#endif

#ifdef __cplusplus

namespace xrt { namespace detail {

// Capture version of XRT at compile time.
//
// An object of this struct can be passed to implementation code by
// inline APIs. The implementation code will continue to see the
// version of XRT used when the binary was compiled even with later
// versions of XRT installed.
//
// The struct is used to guarantee schema compability between old
// version of XRT and new version.
struct abi {
#ifndef DISABLE_ABI_CHECK
  const unsigned int major {XRT_MAJOR(XRT_VERSION_CODE)};
  const unsigned int minor {XRT_MINOR(XRT_VERSION_CODE)};
  const unsigned int code  {XRT_VERSION_CODE};
#else
  const unsigned int major {0};
  const unsigned int minor {0};
  const unsigned int code  {0};
#endif
};

}} // detail, xrt

#endif

#endif
