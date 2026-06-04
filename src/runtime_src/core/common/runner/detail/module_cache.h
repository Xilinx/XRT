// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_COMMON_RUNNER_DETAIL_MODULE_CACHE_H_
#define XRT_COMMON_RUNNER_DETAIL_MODULE_CACHE_H_
#include "core/common/runner/repo.h"
#include "core/common/runner/detail/streambuf.h"

#include "core/include/xrt/experimental/xrt_elf.h"
#include "core/include/xrt/experimental/xrt_module.h"

#include <map>
#include <string>

namespace xrt_core::detail::module_cache {

using repo_type = xrt_core::artifacts::repository;

// Cache of elf files to modules to avoid recreating modules
// referring to the same elf file.
static std::map<std::string, xrt::elf> s_path2elf; // NOLINT
static std::map<xrt::elf, xrt::module> s_elf2mod;  // NOLINT

inline xrt::module
get(const xrt::elf& elf)
{
  if (auto it = s_elf2mod.find(elf); it != s_elf2mod.end())
    return (*it).second;

  xrt::module mod{elf};
  s_elf2mod.emplace(elf, mod);
  return mod;
}

inline xrt::module
get(const std::string& path, const repo_type& repo)
{
  //auto key = repo->get_id() + path; // must be unique to repo
  auto id = std::to_string(repo.get_uid());
  auto key = id + path; // must be unique to repo
  if (auto it = s_path2elf.find(key); it != s_path2elf.end())
    return get((*it).second);

  auto data = repo.get(path);
  streambuf buf{data.data(), data.data() + data.size()};
  std::istream is{&buf};
  xrt::elf elf{is};
  s_path2elf.emplace(key, elf);

  return get(elf);
}

} // xrt_core::runner::detail::module_cache

#endif
