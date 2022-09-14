// SPDX-License-Identifier: Apache-2.0
// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XCL_HWQUEUE_H_
#define XCL_HWQUEUE_H_

// Definitions related to HW context shared between user space XRT and
// Linux kernel driver.  The header file is exported as underlying
// types are exposed in xrt::hw_context::qos definition.

#ifdef __cplusplus
# include <cstdint>
extern "C" {
#else
# if defined(__KERNEL__)
#  include <linux/types.h>
# else
#  include <stdint.h>
# endif
#endif

// Underlying representation of a hardware queue handle.
//
// A hardware queue is create by shim.  The underlying representation
// is platform specific.
#if defined(_WIN32)
typedef void* xcl_hwqueue_handle;
# define XRT_NULL_HWQUEUE NULL
#else
typedef unsigned int xcl_hwqueue_handle;
# define XRT_NULL_HWQUEUE 0xffffffff
#endif

#ifdef __cplusplus
}
#endif

#endif
