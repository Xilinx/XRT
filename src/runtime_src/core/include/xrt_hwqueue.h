// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_HWQUEUE_H_
#define XRT_HWQUEUE_H_

// Underlying representation of a hardware queue handle.
//
// A hardware queue is create by shim.  The underlying representation
// is platform specific.
typedef void* xrt_hwqueue_handle;
# define XRT_NULL_HWQUEUE NULL

#endif
