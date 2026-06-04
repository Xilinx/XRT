// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025-2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef core_common_detail_linux_utils_h
#define core_common_detail_linux_utils_h

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>

namespace xrt_core::utils::detail {

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

// Safe cross-platform environment variable retrieval.
// Returns empty string if variable is not set.
inline std::string
getenv(const char* name)
{
  if (const char* value = std::getenv(name))
    return value;

  return {};
}

} // xrt_core::utils::detail
#endif

