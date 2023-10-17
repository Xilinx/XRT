// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef core_common_detail_trace_h
#define core_common_detail_trace_h

#ifdef _WIN32
# include "core/common/detail/windows/trace.h"
#else
# include "core/common/detail/linux/trace.h"
#endif

#endif
