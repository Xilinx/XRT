// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_COMMON_RUNNER_DETAIL_ARTIFACTS_H_
#define XRT_COMMON_RUNNER_DETAIL_ARTIFACTS_H_
#include "core/common/config.h"
#include "core/common/debug.h"
#include "xrt/detail/span.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace xrt_core::capture::detail {

template <typename T>
using span = xrt::detail::span<T>;

// class artifacts - dump unique artifacts to disk
//
// This class manages captured data dumping to specified
// directory if and only if data is not already dumped.
class artifacts
{
  std::filesystem::path m_dir;
  std::unordered_map<uint64_t, std::string> m_hash_to_fnm;

  static std::string
  generate_fnm()
  {
    static std::atomic<uint64_t> counter {0};
    return "capture_" + std::to_string(counter.fetch_add(1)) + ".bin";
  }

  static std::filesystem::path
  init_dir(std::filesystem::path dir)
  {
    std::filesystem::create_directories(dir);
    return dir;
  }

  // FNV-1a hash function.
  // Should be low probability of collision.
  // Suggested by Gemini
  static uint64_t
  calculate_hash(span<const char> data)
  {
    auto hash = 0xcbf29ce484222325ULL; // NOLINT
    for (unsigned char c : data) {
      hash ^= c;
      hash *= 0x100000001b3ULL; // NOLINT
    }
    return hash;
  }

public:
  explicit
  artifacts(std::filesystem::path dir)
    : m_dir(init_dir(std::move(dir)))
  {}

  std::string
  dump(span<const char> data)
  {
    if (data.empty())
      return {};

    auto hash = calculate_hash(data);
    if (auto itr = m_hash_to_fnm.find(hash); itr != m_hash_to_fnm.end())
      return (*itr).second;

    auto fnm = generate_fnm();
    std::filesystem::path file_path = m_dir / fnm;
    XRT_DEBUGF("Dumping artifact data to %s\n", file_path.string().c_str());
    std::ofstream ostr(file_path, std::ios::binary);
    if (!ostr)
      throw std::runtime_error("Failed to open file for capture dump: " + file_path.string());

    ostr.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!ostr)
      throw std::runtime_error("Error writing capture dump to: " + file_path.string());

    m_hash_to_fnm.emplace(hash, fnm);
    return fnm;
  }

  std::string
  add(std::stringstream& sstr)
  {
    auto data = sstr.str();
    return dump({data.data(), data.size()});
  }
};

} // namespace xrt_core::capture::detail

#endif
