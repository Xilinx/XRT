// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <common/trace_utils.h>

const char*
get_func_mname_from_signature(const char* s)
{
  for (uint32_t i = 0; i < get_size_of_func_mangled_map(); i += 2)
  {
    if (!strcmp(s, func_mangled_map[i]))
      return func_mangled_map[++i];
  }
  return nullptr;
}

std::string
xbtracer_get_timestamp_str()
{
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm local_time{};
  localtime_os(local_time, t);
  std::ostringstream oss;
  oss << std::put_time(&local_time, "%Y%m%d_%H%M%S");
  return oss.str();
}
