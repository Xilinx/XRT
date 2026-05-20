// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

// Cross-platform wrapper for banned C string APIs
// Provides secure replacements for Windows driver compliance

#ifndef XRT_CORE_COMMON_SAFE_STR_H
#define XRT_CORE_COMMON_SAFE_STR_H

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdarg>

namespace xrt_core { namespace str_wrapper {
  // Array case - use secure *_s functions (Windows only)
  #ifdef _WIN32
  template<size_t N>
  inline char* strcpy(char (&dest)[N], const char* src) {
    return strcpy_s(dest, N, src) == 0 ? dest : nullptr;
  }

  template<size_t N>
  inline char* strncpy(char (&dest)[N], const char* src, size_t n) {
    return strncpy_s(dest, N, src, n) == 0 ? dest : nullptr;
  }

  template<size_t N>
  inline char* strcat(char (&dest)[N], const char* src) {
    return strcat_s(dest, N, src) == 0 ? dest : nullptr;
  }

  template<size_t N>
  inline char* strncat(char (&dest)[N], const char* src, size_t n) {
    return strncat_s(dest, N, src, n) == 0 ? dest : nullptr;
  }

  template<size_t N>
  inline int sprintf(char (&buf)[N], const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsprintf_s(buf, N, format, args);
    va_end(args);
    return result;
  }

  template<size_t N>
  inline int vsnprintf(char (&buf)[N], size_t size, const char* format, va_list args) {
    return vsnprintf_s(buf, N, size, format, args);
  }
  #endif

  // Pointer case - use std:: functions (cross-platform)
  char* strcpy(char* dest, const char* src);
  char* strncpy(char* dest, const char* src, size_t n);
  char* strcat(char* dest, const char* src);
  char* strncat(char* dest, const char* src, size_t n);
  int sprintf(char* buf, const char* format, ...);
  int vsnprintf(char* buf, size_t size, const char* format, va_list args);
}} // str_wrapper, xrt_core

#endif // XRT_CORE_COMMON_SAFE_STR_H
