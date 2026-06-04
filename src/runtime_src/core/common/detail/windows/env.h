// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

// Windows-safe environment variable access using _dupenv_s.
// Avoids banned API warnings for getenv on Windows.

#ifndef core_common_detail_windows_env_h
#define core_common_detail_windows_env_h

#include <cstdlib>
#include <memory>
#include <string>

namespace xrt_core::detail {

// Safe cross-platform environment variable retrieval.
// Returns empty string if variable is not set or on error.
inline std::string
getenv(const char* name)
{
  char* value = nullptr;
  size_t len = 0;

  // _dupenv_s allocates memory for the value
  if (_dupenv_s(&value, &len, name) == 0 && value != nullptr) {
    // Use unique_ptr to ensure memory is freed even if string constructor throws
    std::unique_ptr<char, decltype(&std::free)> ptr(value, std::free);
    return std::string(ptr.get());
  }

  return "";
}

} // namespace xrt_core::detail

#endif
