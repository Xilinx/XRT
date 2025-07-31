// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef core_common_detail_systime_h
#define core_common_detail_systime_h

#ifdef _WIN32
# include "core/common/detail/windows/systime.h"
#else
# include "core/common/detail/linux/systime.h"
#endif


#endif
