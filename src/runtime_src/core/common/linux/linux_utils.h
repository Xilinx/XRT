// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

/**
 * @file _linux_utils.h
 * @brief Helper functions for Linux-specific operations.
 *
 * This header file defines helper functions that facilitate interaction with
 * the Linux operating system. It includes functionality for retrieving
 * human-readable error messages corresponding to system error codes.
 *
 * @details
 * - The file provides:
 *   - a static inline function `sys_dep_get_linux_err_msg` to
 *     retrieve and format the last error message from the Linux API.
 *     It uses Linux-specific headers and APIs.
 *
 * @note This file is specific to Linux platforms and relies on the Linux
 *       API. It is not portable to other operating systems.
 *
 */

#ifndef linux_utils_h
#define linux_utils_h

#ifdef __linux__

#include <cerrno>
#include <cstring>
#include <string>

/**
 * @brief Retrieves the last error message from the Linux operating system.
 *
 * This function uses the Linux API to obtain a human-readable error message
 * corresponding to the last error code set by the system.
 *
 * @return A string containing the error message associated with the last error code.
 *         If no error has occurred, the returned string may be empty.
 */
inline std::string
sys_dep_get_last_err_msg()
{
  return  std::string(strerror(errno));
}

#endif // __linux__

#endif // linux_utils_h

