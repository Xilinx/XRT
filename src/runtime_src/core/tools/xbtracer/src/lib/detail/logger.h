// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#ifdef _WIN32
# include "windows/logger.h"
#else
# include "linux/logger.h"
#endif
