// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include <iostream>

namespace xrt::tools::xbtracer {

constexpr std::string_view path_separator = "\\";

/*
 * Wrapper api for time formating
 * */
inline std::tm localtime_xp(std::time_t timer)
{
  std::tm bt{};
  localtime_s(&bt, &timer);
  return bt;
}

std::string get_env(const std::string& key)
{
  char *val = nullptr;
  size_t len = 0;
  errno_t err = _dupenv_s(&val, &len, key.c_str());

  // Use a unique_ptr with a lambda function as a custom deleter.
  auto free_deleter = [](char* ptr) { free(ptr); };
  std::unique_ptr<char, decltype(free_deleter)> val_ptr(val, free_deleter);

  if (err || !val)
      return std::string();

  std::string result(val);

  return result;
}

std::string wide_to_string(const std::wstring& wstr)
{
  if (wstr.empty())
    return std::string();

  int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                                        NULL, 0, NULL, NULL);
  std::string str(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0],
                      size_needed, NULL, NULL);
  return str;
}

std::string get_os_name_ver()
{
  // Define the function pointer for RtlGetVersion
  typedef LONG (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOEXW);

  std::string pretty_name = "Windows(unknown)";
  RTL_OSVERSIONINFOEXW osvi = {0};
  osvi.dwOSVersionInfoSize = sizeof(osvi);
  RtlGetVersionPtr RtlGetVersion
      = (RtlGetVersionPtr)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                                         "RtlGetVersion");

  if (RtlGetVersion)
  {
    RtlGetVersion(&osvi);

    if (osvi.dwMajorVersion == 10 && osvi.wProductType == VER_NT_WORKSTATION &&
        osvi.szCSDVersion[0] == 0)
      pretty_name = "Windows 11";
    else
    {
      std::string version_info
          = "Windows " + std::to_string(osvi.dwMajorVersion) + "."
            + std::to_string(osvi.dwMinorVersion);
      std::string csd_version = wide_to_string(osvi.szCSDVersion);

      if (csd_version.empty())
        pretty_name = version_info;
      else
        pretty_name = version_info + " " + csd_version;
    }
  }
  return pretty_name;
}

inline DWORD get_current_procces_id()
{
  return GetCurrentProcessId();
}

}
