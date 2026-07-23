// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025-2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef core_common_detail_windows_utils_h
#define core_common_detail_windows_utils_h

// This file is not to be included stand-alone

#include <cstdlib>
#include <memory>
#include <string>

// Exclude rarely-used headers (e.g. Winsock) from <windows.h> for
// faster builds and fewer conflicts.
#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>

namespace xrt_core::utils::detail {

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

// Safe cross-platform environment variable retrieval.
// Returns empty string if variable is not set or on error.
inline std::string
getenv(const char* name)
{
  char* value = nullptr;
  size_t len = 0;

  // _dupenv_s allocates memory for the value
  if (_dupenv_s(&value, &len, name) != 0 || value == nullptr)
    return {};
  
  // Use unique_ptr to ensure memory is freed even if string constructor throws
  std::unique_ptr<char, decltype(&std::free)> guard(value, &std::free);
  return std::string(guard.get());
}

inline std::string
strerror(int err)
{
  char buffer[256];
  strerror_s(buffer, sizeof(buffer), err);
  return buffer;
}

// Returns true when the process token is UAC-elevated (i.e. running as
// Administrator). Checks token elevation rather than group membership so
// the result is correct even when UAC is active (SWSPLAT-39875 / CWE-427).
inline bool
is_elevated_process()
{
  HANDLE token = nullptr;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return false; // cannot determine — fail safe to non-elevated behaviour

  TOKEN_ELEVATION elevation = {};
  DWORD size = sizeof(elevation);
  bool elevated = ::GetTokenInformation(token, TokenElevation,
                                        &elevation, size, &size)
                  && elevation.TokenIsElevated;
  ::CloseHandle(token);
  return elevated;
}

} // xrt_core::utils::detail
#endif

