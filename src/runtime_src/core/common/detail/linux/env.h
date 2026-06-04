// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

// POSIX environment variable access using std::getenv.

#ifndef core_common_detail_linux_env_h
#define core_common_detail_linux_env_h

#include <cstdlib>
#include <string>

namespace xrt_core::detail {

// Safe cross-platform environment variable retrieval.
// Returns empty string if variable is not set.
inline std::string
getenv(const char* name)
{
  if (const char* value = std::getenv(name))
    return std::string(value);
  return "";
}

} // namespace xrt_core::detail

#endif
