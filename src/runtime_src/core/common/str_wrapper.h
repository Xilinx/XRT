/******************************************************************************
* Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
* SPDX-License-Identifier: Apache-2.0
******************************************************************************/

// Cross-platform wrapper for banned C string APIs
// Provides secure replacements for Windows driver compliance

#ifndef XRT_CORE_COMMON_SAFE_STR_H
#define XRT_CORE_COMMON_SAFE_STR_H

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdarg>

#ifdef _WIN32

#undef strcpy // No bounds checking
#define strcpy(dest, src) strcpy_s((dest), sizeof(dest), (src))

#undef strncpy // Limited error detection
#define strncpy(dest, src, n) strncpy_s((dest), sizeof(dest), (src), (n))

#undef strcat // Limited error detection
#define strcat(dest, src) strcat_s((dest), sizeof(dest), (src))

#undef strncat // Limited error detection
#define strncat(dest, src, n) strncat_s((dest), sizeof(dest), (src), (n))

#undef sprintf // Limited error detection
#define sprintf(buf, format, ...) sprintf_s((buf), sizeof(buf), (format), ##__VA_ARGS__)

#undef vsnprintf // Limited error detection
#define vsnprintf(buf, size, format, args) vsnprintf_s((buf), sizeof(buf), (size), (format), (args))

#endif // _WIN32

#endif // XRT_CORE_COMMON_SAFE_STR_H
