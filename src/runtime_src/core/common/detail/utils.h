// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef core_common_detail_utils_h
#define core_common_detail_utils_h

#ifdef _WIN32
# include "core/common/detail/windows/utils.h"
#else
# include "core/common/detail/linux/utils.h"
#endif

#endif
