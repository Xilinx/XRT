/**
 * Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

// Cross-platform wrapper for banned C string APIs
// Provides secure replacements for Windows driver compliance

#ifndef SAFE_STR_WRAPPER_H_
#define SAFE_STR_WRAPPER_H_

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <cstring>
#include <stdarg.h>

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

#else
namespace xrt_str_safe {

inline errno_t strcpy_safe(char* dest, size_t dest_size, const char* src) {
  if (!dest || !src || dest_size == 0) return EINVAL;
  size_t src_len = strlen(src);
  if (src_len >= dest_size) return ERANGE;
  std::strcpy(dest, src);
  return 0;
}

inline errno_t strncpy_safe(char* dest, size_t dest_size, const char* src, size_t count) {
  if (!dest || !src || dest_size == 0) return EINVAL;
  size_t copy_len = (count < dest_size - 1) ? count : dest_size - 1;
  std::strncpy(dest, src, copy_len);
  dest[copy_len] = '\0';
  return 0;
}

inline errno_t strcat_safe(char* dest, size_t dest_size, const char* src) {
  if (!dest || !src || dest_size == 0) return EINVAL;
  size_t dest_len = strlen(dest);
  size_t src_len = strlen(src);
  if (dest_len + src_len >= dest_size) return ERANGE;
  std::strcat(dest, src);
  return 0;
}

inline errno_t strncat_safe(char* dest, size_t dest_size, const char* src, size_t count) {
  if (!dest || !src || dest_size == 0) return EINVAL;
  size_t dest_len = strlen(dest);
  size_t available = dest_size - dest_len - 1;
  size_t copy_len = (count < available) ? count : available;
  std::strncat(dest, src, copy_len);
  return 0;
}

} // namespace xrt_str_safe

#undef strcpy
#define strcpy(dest, src) (xrt_str_safe::strcpy_safe((dest), sizeof(dest), (src)), (dest))

#undef strncpy
#define strncpy(dest, src, n) (xrt_str_safe::strncpy_safe((dest), sizeof(dest), (src), (n)), (dest))

#undef strcat
#define strcat(dest, src) (xrt_str_safe::strcat_safe((dest), sizeof(dest), (src)), (dest))

#undef strncat
#define strncat(dest, src, n) (xrt_str_safe::strncat_safe((dest), sizeof(dest), (src), (n)), (dest))

#undef sprintf
#define sprintf(buf, format, ...) std::snprintf((buf), sizeof(buf), (format), ##__VA_ARGS__)

#endif // _WIN32

#endif // SAFE_STR_WRAPPER_H_
