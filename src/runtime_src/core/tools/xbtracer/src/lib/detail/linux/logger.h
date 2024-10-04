// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include <iostream>
#include <unistd.h>

namespace xrt::tools::xbtracer {

constexpr std::string_view path_separator = "/";

/*
 * Wrapper api for time formating
 * */
inline std::tm localtime_xp(std::time_t timer)
{
  std::tm bt{};
  localtime_r(&timer, &bt);
  return bt;
}

std::string get_env(std::string key)
{
  const char* val = std::getenv(key.c_str());
  return !val ? std::string() : std::string(val);
}

std::string get_os_name_ver()
{
  std::string pretty_name;

  std::ifstream os_release("/etc/os-release");
  std::string line;

  if (os_release.is_open())
  {
    while (std::getline(os_release, line))
    {
      if (line.find("PRETTY_NAME=") != std::string::npos)
      {
        pretty_name = line.substr(line.find('=') + 1);
        break;
      }
    }
    os_release.close();
  }
  else
  {
    std::cerr << "Failed to open /etc/os-release" << std::endl;
    pretty_name = "Linux-unknown-dist";
  }
  return pretty_name;
}

inline pid_t get_current_procces_id()
{
  return getpid();
}

}
