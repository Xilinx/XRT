// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef core_common_detail_syslog_h
#define core_common_detail_syslog_h

#ifdef _WIN32
# include "core/common/detail/windows/syslog.h"
#else
# include "core/common/detail/linux/syslog.h"
#endif

#endif
