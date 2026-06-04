// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_RUNNER_DETAIL_WINDOWS_PROCESS_H_
#define XRT_RUNNER_DETAIL_WINDOWS_PROCESS_H_

// This file is not to be included stand-alone

#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <sstream>

// Exclude rarely-used headers from <windows.h> for faster builds and fewer conflicts
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>

namespace xrt_core::detail {

inline int
execute_process(const std::vector<std::string>& args)
{
  if (args.empty())
    throw std::runtime_error("No command specified");

  // Build command line - Windows requires a single string
  std::ostringstream cmdline;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0)
      cmdline << " ";

    // Quote argument if it contains spaces
    const auto& arg = args[i];
    if (arg.find(' ') != std::string::npos)
      cmdline << "\"" << arg << "\"";
    else
      cmdline << arg;
  }

  std::string cmdline_str = cmdline.str();

  STARTUPINFOA si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};

  // CreateProcess requires non-const command line
  std::vector<char> cmdline_buf(cmdline_str.begin(), cmdline_str.end());
  cmdline_buf.push_back('\0');

  if (!CreateProcessA(
        nullptr,           // Application name (use command line)
        cmdline_buf.data(), // Command line
        nullptr,           // Process security attributes
        nullptr,           // Thread security attributes
        TRUE,              // Inherit handles
        0,                 // Creation flags
        nullptr,           // Environment
        nullptr,           // Current directory
        &si,               // Startup info
        &pi))              // Process information
  {
    DWORD error = GetLastError();
    throw std::runtime_error("Failed to create process: error " + std::to_string(error));
  }

  // Wait for process to complete
  WaitForSingleObject(pi.hProcess, INFINITE);

  // Get exit code
  DWORD exit_code = 0;
  GetExitCodeProcess(pi.hProcess, &exit_code);

  // Clean up
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return static_cast<int>(exit_code);
}

} // namespace xrt_core::detail

#endif // XRT_RUNNER_DETAIL_WINDOWS_PROCESS_H_
