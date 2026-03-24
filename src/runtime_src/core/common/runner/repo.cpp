// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#define XCL_DRIVER_DLL_EXPORT  // in same dll as exported xrt apis
#define XRT_CORE_COMMON_SOURCE // in same dll as coreutil
#define XRT_API_SOURCE         // in same dll as coreutil
#include "repo.h"
#include "detail/mmap.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

template <typename T>
using span = xrt_core::artifacts::span<T>;
  
// class artifact - Manage a repo artifact
// This erases the type of the specific storage type (e.g. vector,
// string, mmap) and provides a common interface to get a span<char>.
class artifact
{
  struct artifact_iholder
  {
    virtual ~artifact_iholder() = default;
    virtual span<char> get_span() = 0;
  };

  // Default holder for storage types that support data() and size()
  template <typename StorageType>
  struct artifact_type : artifact_iholder
  {
    StorageType m_storage;

    explicit
    artifact_type(StorageType&& storage)
      : m_storage(std::forward<StorageType>(storage))
    {}
        
    span<char>
    get_span() override
    {
      return span<char>{m_storage.data(), m_storage.size()};
    }
  };

  // Specialize for mmap_artifact
  template <>
  struct artifact_type<xrt_core::artifacts::detail::mmap_artifact> : artifact_iholder
  {
    xrt_core::artifacts::detail::mmap_artifact m_storage;

    explicit
    artifact_type(xrt_core::artifacts::detail::mmap_artifact&& storage)
      : m_storage(std::move(storage))
    {}
                
    span<char>
    get_span() override
    {
      return m_storage.get_span();
    }
  };

  struct artifact_ref : artifact_iholder
  {
    void* m_data;
    size_t m_size;

    artifact_ref(void* data, size_t size)
      : m_data(data), m_size(size)
    {}
                
    span<char>
    get_span() override
    {
      return span<char>{static_cast<char*>(m_data), m_size};
    }
  };

  std::unique_ptr<artifact_iholder> m_holder;

public:
  artifact() = default;
  artifact(artifact&&) = default;

  template <typename StorageType,
            typename = std::enable_if_t<!std::is_same_v<std::decay_t<StorageType>, artifact>>>
  explicit artifact(StorageType&& storage)
    : m_holder(std::make_unique<artifact_type<StorageType>>(std::forward<StorageType>(storage)))
  {}

  artifact(void* data, std::size_t size)
    : m_holder(std::make_unique<artifact_ref>(data, size))
  {}

  span<char>
  get_span()
  {
    return m_holder->get_span();
  }
}; // artifact

} // namespace

namespace xrt_core::artifacts {

using data_mode = repository::data_mode;
using file_mode = repository::file_mode;

////////////////////////////////////////////////////////////////
// class repository_impl - Repo implementation
// Base class for base_repo.
////////////////////////////////////////////////////////////////
class repository_impl
{
  uint64_t m_uid = 0;
public:
  repository_impl()
  {
    static std::atomic<uint64_t> next_uid{1};
    m_uid = next_uid.fetch_add(1, std::memory_order_relaxed);
  }
  
  virtual
  ~repository_impl() = default;

  uint64_t
  get_uid() const
  {
    return m_uid;
  }

  virtual void
  add_data(const std::string& key, span<char> data, data_mode mode) const = 0;

  virtual void
  add_data(const std::string& key, std::string&& data) const = 0;

  virtual void
  add_data(const std::string& key, std::vector<char>&& data) const = 0;

  virtual void
  add_file(const std::string& key, file_mode mode) const = 0;

  virtual span<char>
  get(const std::string& key) const = 0;

  virtual span<char>
  get(const std::string& key, file_mode hint) const = 0;
};

// class base_repo - Base implementation of repository_impl
class base_repo : public repository_impl
{
  // Store as type erased artifacts
  mutable std::unordered_map<std::string, artifact> m_artifacts;

  // The key to store artifacts is resolved by this function, which
  // is overridden by the file repository.
  virtual std::string
  resolve_key(const std::string& key) const
  {
    return key;
  }

private:
  ////////////////////////////////////////////////////////////////
  // Add artifact by different modes
  ////////////////////////////////////////////////////////////////
  void
  add_copy(const std::string& key, span<char> data) const
  {
    std::vector<char> vec{data.begin(), data.end()};
    m_artifacts.emplace(key, std::move(vec));
  }

  void
  add_ref(const std::string& key, span<char> data) const
  {
    m_artifacts.emplace(key, artifact{data.data(), data.size()});
  }

  void
  add_file_read(const std::string& key) const
  {
    std::ifstream ifs(key, std::ios::binary | std::ios::ate);
    if (!ifs)
      throw std::runtime_error("artifacts::repository: cannot open file: " + key);

    const auto size = static_cast<std::size_t>(ifs.tellg());
    ifs.seekg(0);
    std::vector<char> vec(size);
    if (size > 0
        && size <= static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())
        && !ifs.read(reinterpret_cast<char*>(vec.data()), static_cast<std::streamsize>(size)))
      throw std::runtime_error("artifacts::repository: read failed: " + key);

    m_artifacts.emplace(key, std::move(vec));
  }

  void
  add_file_mmap(const std::string& key) const
  {
    m_artifacts.emplace(key, detail::mmap_artifact{key});
  }

public:
  ////////////////////////////////////////////////////////////////
  // Implementation of repository_impl interface
  ////////////////////////////////////////////////////////////////

  // add_data() - Add data by copy or reference
  void
  add_data(const std::string& key, span<char> data, data_mode mode) const override
  {
    switch (mode) {
    case data_mode::copy:
      add_copy(resolve_key(key), data);
      break;
    case data_mode::ref:
      add_ref(resolve_key(key), data);
      break;
    }
  }

  // add_data() - Add data by stealing string
  void
  add_data(const std::string& key, std::string&& data) const override
  {
    m_artifacts.emplace(resolve_key(key), std::move(data));
  }

  void
  add_data(const std::string& key, std::vector<char>&& data) const override
  {
    m_artifacts.emplace(resolve_key(key), std::move(data));
  }

  // add_file() - Add file by reading or memory mapping
  void
  add_file(const std::string& key, file_mode mode) const override
  {
    switch (mode) {
    case file_mode::read:
      add_file_read(resolve_key(key));
      break;
    case file_mode::mmap:
      add_file_mmap(resolve_key(key));
      break;
    }
  }

  // get() - Get artifact by key
  span<char>
  get(const std::string& key) const override
  {
    auto rkey = resolve_key(key);

    auto it = m_artifacts.find(rkey);
    if (it == m_artifacts.end())
      throw std::runtime_error{"Failed to find artifact: " + rkey};
    
    return it->second.get_span();
  }

  // get() - Get artifact by key, with file mode hint for on-demand loading
  span<char>
  get(const std::string& key, file_mode hint) const override
  {
    auto it = m_artifacts.find(resolve_key(key));
    if (it == m_artifacts.end())
      base_repo::add_file(key, hint); // add_file() resolves key

    return base_repo::get(key);       // get() resolves key as well
  }
};

////////////////////////////////////////////////////////////////
// struct file_impl - Implementation with base dir
// Resolves key by prepending base dir.
////////////////////////////////////////////////////////////////
class file_repo : public base_repo
{
  std::filesystem::path m_base_dir;

  std::string
  resolve_key(const std::string& key) const override
  {
    return (m_base_dir / std::filesystem::path(key)).string();
  }
  
public:
  explicit
  file_repo(std::filesystem::path artifacts_dir)
   : m_base_dir(std::move(artifacts_dir))
  {}

  // get() - Get artifact by key
  span<char>
  get(const std::string& key) const override
  {
    return base_repo::get(key, file_mode::read);
  }
  
};

////////////////////////////////////////////////////////////////
// class repository - Public API implementation
////////////////////////////////////////////////////////////////
repository::
repository()
  : xrt::detail::pimpl<repository_impl>{std::make_unique<base_repo>()}
{}

repository::
repository(const std::filesystem::path& artifacts_dir)
  : xrt::detail::pimpl<repository_impl>{std::make_unique<file_repo>(artifacts_dir)}
{}

repository::~repository() = default;

uint64_t
repository::
get_uid() const
{
  return handle->get_uid();
}

void
repository::
add_data(const std::string& key, span<char> data, data_mode mode)
{
  handle->add_data(key, data, mode);
}

void
repository::
add_data(const std::string& key, std::string&& data)
{
  handle->add_data(key, std::move(data));
}

void
repository::
add_data(const std::string& key, std::vector<char>&& data)
{
  handle->add_data(key, std::move(data));
}


void
repository::
add_file(const std::string& key, file_mode mode)
{
  handle->add_file(key, mode);
}

span<char>
repository::
get(const std::string& key) const
{
  return handle->get(key);
}

span<char>
repository::
get(const std::string& key, file_mode hint)
{
  return handle->get(key, hint);
}

}  // namespace xrt_core::artifacts
