// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef ARTIFACTS_DETAIL_MMAP_H
#define ARTIFACTS_DETAIL_MMAP_H

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)
# include "windows/mmap.h"
#else
# include "linux/mmap.h"
#endif

#endif // ARTIFACTS_DETAIL_MMAP_H
