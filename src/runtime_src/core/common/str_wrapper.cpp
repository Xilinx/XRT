// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

#include "core/common/str_wrapper.h"

#include <cstdio>
#include <cstring>
#include <cstdarg>

namespace xrt_core { namespace str_wrapper {
  // Pointer case - use std:: functions (cross-platform)
  char* strcpy(char* dest, const char* src)
  {
    return std::strcpy(dest, src);
  }

  char* strncpy(char* dest, const char* src, size_t n)
  {
    return std::strncpy(dest, src, n);
  }

  char* strcat(char* dest, const char* src)
  {
    return std::strcat(dest, src);
  }

  char* strncat(char* dest, const char* src, size_t n)
  {
    return std::strncat(dest, src, n);
  }

  int sprintf(char* buf, const char* format, ...)
  {
    va_list args;
    va_start(args, format);
    int result = std::vsprintf(buf, format, args);
    va_end(args);
    return result;
  }

  int vsnprintf(char* buf, size_t size, const char* format, va_list args)
  {
    return std::vsnprintf(buf, size, format, args);
  }
}} // str_wrapper, xrt_core