// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef core_common_detail_sysinfo_h
#define core_common_detail_sysinfo_h

#ifdef _WIN32
# include "core/common/detail/windows/sysinfo.h"
#else
# include "core/common/detail/linux/sysinfo.h"
#endif

#endif
