// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_COMMON_RUNNER_ARTIFACTS_REPOSITORY_H
#define XRT_COMMON_RUNNER_ARTIFACTS_REPOSITORY_H

#include "xrt/detail/config.h"
#include "xrt/detail/pimpl.h"
#include "xrt/detail/span.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace xrt_core::artifacts {

template <typename T>
using span = xrt::detail::span<T>;

/**
 * class repository - A class for managing binary artifacts
 *
 * Manages binary artifacts by key. Data can be added from memory
 * (copy or reference) or from files (read or memory-mapped).
 * Consumers access data by key and receive a std::span; lifetime is
 * that of the repository or the application-managed buffer for
 * reference-backed artifacts.
 */
class repository_impl;
class repository : public xrt::detail::pimpl<repository_impl>
{
public:
  /**
   * @enum data_mode - Control how in-memory bytes are stored
   *
   * @var copy
   *   The bytes are copied into the repository
   * @var ref
   *   The bytes are stored as referecne to data owned by caller
   */
  enum class data_mode
  {
    copy,  // copy data
    ref    // reference caller owned data
  };

  /**
   * @enum file_mode - Control how a file is loaded
   *
   * @var read The file is read from disk and copied into the
   *   repository.
   * @var ref The file is memory mapped and stored mapped pointer is
   *   stored in repository.
   */
  enum class file_mode
  {
    read,  // read from disk
    mmap   // map into memory
  };
  
public:
  /**
   * ctor - Default repo
   */
  XRT_API_EXPORT
  repository();

  /**
   * ctor - Supply base directory for file data
   * aka file_repo
   */
  XRT_API_EXPORT
  explicit
  repository(const std::filesystem::path& artifacts_dir);

  /**
   * ctor - Supply in-memory map to data
   * aka ram_repo
   */
  XRT_API_EXPORT
  repository(const std::map<std::string, std::vector<char>>& repo);

  XRT_API_EXPORT
  ~repository();

  repository(repository&&) noexcept = default;
  repository& operator=(repository&&) noexcept = default;
  repository(const repository&) = default;
  repository& operator=(const repository&) = default;

  /**
   * get_uid() - Get a unique identifier for the repository
   */
  XRT_API_EXPORT
  uint64_t
  get_uid() const;

  /**
   * add_data() - Add artifacts from raw bytes
   *
   * @param key
   *   Key used to store and retrieve the data
   * @param data
   *   Span of the raw data
   * @param mode
   *   How the data is managed
   */
  XRT_API_EXPORT
  void
  add_data(const std::string& key, span<char> data, data_mode mode);

  /**
   * add_data() - Move string managed data into the repo
   *
   * @param key
   *   Key used to store and retrieve the data
   * @param data
   *   Rvalue string to move into the repo
   */
  XRT_API_EXPORT
  void
  add_data(const std::string& key, std::string&& data);

  /**
   * add_file() - Add file content
   *
   * @param key
   *   The key is a path to the file absolute or relative to
   *   current directory.  The key is used to store the data
   *   and later retrieve it from the repository.
   * @param mode
   *   How the file data is stored in the repository
   */
  XRT_API_EXPORT
  void
  add_file(const std::string& key, file_mode mode);

  /**
   * get() - Get binary artifact identified by key
   *
   * @param key
   *   The key that identifies the data in the repository
   *
   * Return a read-only span over the artifact data for \a key, or
   * throw out-of-range is no such key.
   */
  XRT_API_EXPORT
  span<char>
  get(const std::string& key) const;

  /**
   * get() - If the key exists, return its data else add file
   *
   * @param key
   *   The key that identifies the file data
   * @param hint
   *   The file moode if the key needs to be added.
   *
   * This function checks if key is present and returns corresponding
   * data.  If not present, then the key is treated as identifying a a
   * file, which is added to the repo under the specified hint mode.
   */
  span<char>
  get(const std::string& key, file_mode hint);
};

} // namespace xrt_core::artifacts

#endif  // XRT_COMMON_RUNNER_ARTIFACTS_REPOSITORY_H
