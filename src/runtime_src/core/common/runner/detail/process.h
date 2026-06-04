// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_RUNNER_DETAIL_PROCESS_H_
#define XRT_RUNNER_DETAIL_PROCESS_H_

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)
# include "windows/process_windows.h"
#else
# include "linux/process_linux.h"
#endif

#endif // XRT_RUNNER_DETAIL_PROCESS_H_
