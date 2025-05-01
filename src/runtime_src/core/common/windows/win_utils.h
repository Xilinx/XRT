// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

/**
 * @file _win_utils.h
 * @brief Helper functions for Windows-specific operations.
 *
 * This header file defines helper functions that facilitate interaction with
 * the Windows operating system. It includes functionality for retrieving
 * human-readable error messages corresponding to system error codes.
 *
 * @details
 * - The file provides:
 *   - a static inline function `xbutils_get_win_err_msg` to
 *     retrieve and format the last error message from the Windows API.
 *     It uses Windows-specific headers and APIs such as `GetLastError` and
 *     `FormatMessage` to achieve this functionality.
 *
 * @note This file is specific to Windows platforms and relies on the Windows
 *       API. It is not portable to other operating systems.
 *
 */

#ifndef win_utils_h
#define win_utils_h

#ifdef _WIN32

#include <windows.h>
#include <string>

/**
 * @brief Retrieves the last error message from the Windows operating system.
 *
 * This function uses the Windows API to obtain a human-readable error message
 * corresponding to the last error code set by the system. It retrieves the error
 * code using `GetLastError()` and formats the message using `FormatMessage()`.
 *
 * @return A string containing the error message associated with the last error code.
 *         If no error has occurred, the returned string may be empty.
 */
inline std::string
sys_dep_get_last_err_msg()
{
  DWORD error_code = GetLastError();
  LPVOID error_msg;
  FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    error_code,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPTSTR)&error_msg,
    0,
    NULL);
  std::string error_message = static_cast<char*>(error_msg);
  LocalFree(error_msg);
  return error_message;
}

#endif // _WIN32

#endif // win_utils_h

