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
#include <unordered_map>
#include <vector>

namespace xrt_core::artifacts {

using data_mode = repository::data_mode;
using file_mode = repository::file_mode;

namespace {

// Internal polymorphic artifact storage; not exposed in the API.
struct artifact_concept
{
  virtual ~artifact_concept() = default;

  virtual span<char>
  get_span() = 0;
};

struct owned_artifact : artifact_concept
{
  std::vector<char> m_data;

  span<char>
  get_span() override
  {
    return span<char>(m_data.data(), m_data.size());
  }
};

struct owned_string_artifact : artifact_concept
{
  std::string m_data;

  span<char>
  get_span() override
  {
    return span<char>(m_data.data(), m_data.size());
  }
};

struct ref_artifact : artifact_concept
{
  char* m_ptr = nullptr;
  std::size_t m_size = 0;

  span<char>
  get_span() override
  {
    return span<char>(m_ptr, m_size);
  }
};

struct mmap_artifact_holder : artifact_concept
{
  detail::mmap_artifact m_mapped;

  span<char>
  get_span() override
  {
    return m_mapped.get_span();
  }
};

}  // namespace

////////////////////////////////////////////////////////////////
// struct base_repo
// Base class for base_repo.  Type erased artifacts.
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
  add_file(const std::string& key, file_mode mode) const = 0;

  virtual span<char>
  get(const std::string& key) const = 0;

  virtual span<char>
  get(const std::string& key, file_mode hint) const = 0;
};
  
class base_repo : public repository_impl
{
private:
  // Store as type erased artifacts
  mutable std::unordered_map<std::string, std::unique_ptr<artifact_concept>> m_artifacts;

  void
  add_copy(const std::string& key, span<char> data) const
  {
    auto a = std::make_unique<owned_artifact>();
    a->m_data.assign(data.begin(), data.end());
    m_artifacts[key] = std::move(a);
  }

  void
  add_string(const std::string& key, std::string&& data) const
  {
    auto a = std::make_unique<owned_string_artifact>();
    a->m_data = std::move(data);
    m_artifacts[key] = std::move(a);
  }

  void
  add_ref(const std::string& key, span<char> data) const
  {
    auto a = std::make_unique<ref_artifact>();
    a->m_ptr = data.data();
    a->m_size = data.size();
    m_artifacts[key] = std::move(a);
  }

  void
  add_file_read(const std::string& key) const
  {
    std::ifstream ifs(key, std::ios::binary | std::ios::ate);
    if (!ifs)
      throw std::runtime_error("artifacts::repository: cannot open file: " + key);

    const auto size = static_cast<std::size_t>(ifs.tellg());
    ifs.seekg(0);
    auto a = std::make_unique<owned_artifact>();
    a->m_data.resize(size);
    if (size > 0 && !ifs.read(reinterpret_cast<char*>(a->m_data.data()), size))
      throw std::runtime_error("artifacts::repository: read failed: " + key);

    m_artifacts[key] = std::move(a);
  }

  void
  add_file_mmap(const std::string& key) const
  {
    auto a = std::make_unique<mmap_artifact_holder>();
    a->m_mapped = detail::mmap_artifact(key);
    m_artifacts[key] = std::move(a);
  }

public:
  ////////////////////////////////////////////////////////////////
  // Implementation of APIs
  ////////////////////////////////////////////////////////////////
  virtual
  ~base_repo() = default;

  void
  add_data(const std::string& key, span<char> data, data_mode mode) const override
  {
    switch (mode) {
    case data_mode::copy:
      add_copy(key, data);
      break;
    case data_mode::ref:
      add_ref(key, data);
      break;
    }
  }

  void
  add_data(const std::string& key, std::string&& data) const override
  {
    add_string(key, std::move(data));
  }

  void
  add_file(const std::string& key, file_mode mode) const override
  {
    switch (mode) {
    case file_mode::read:
      add_file_read(key);
      break;
    case file_mode::mmap:
      add_file_mmap(key);
      break;
    }
  }

  span<char>
  get(const std::string& key) const override
  {
    auto it = m_artifacts.find(key);
    if (it == m_artifacts.end())
      throw std::runtime_error{"Failed to find artifact: " + key};
    
    return it->second->get_span();
  }

  span<char>
  get(const std::string& key, file_mode hint) const override
  {
    auto it = m_artifacts.find(key);
    if (it == m_artifacts.end())
      base_repo::add_file(key, hint);

    return base_repo::get(key);
  }
};

////////////////////////////////////////////////////////////////
// struct file_impl - Implementation with base dir
// Resolves all keys by prepending base dir, even if data
// is not a file.
////////////////////////////////////////////////////////////////
class file_repo : public base_repo
{
  std::filesystem::path m_base_dir;

public:
  explicit
  file_repo(std::filesystem::path artifacts_dir)
   : m_base_dir(std::move(artifacts_dir))
  {}

  void
  add_data(const std::string& key, span<char> data, data_mode mode) const override
  {
    const std::filesystem::path resolved = m_base_dir / std::filesystem::path(key);
    base_repo::add_data(resolved.string(), data, mode);
  }

  void
  add_data(const std::string& key, std::string&& data) const override
  {
    const std::filesystem::path resolved = m_base_dir / std::filesystem::path(key);
    base_repo::add_data(resolved.string(), std::move(data));
  }

  void
  add_file(const std::string& key, file_mode mode) const override
  {
    const std::filesystem::path resolved = m_base_dir / std::filesystem::path(key);
    base_repo::add_file(resolved.string(), mode);
  }

  span<char>
  get(const std::string& key) const override
  {
    const std::filesystem::path resolved = m_base_dir / std::filesystem::path(key);
    return base_repo::get(resolved.string(), file_mode::mmap);
  }

  span<char>
  get(const std::string& key, file_mode hint) const override
  {
    const std::filesystem::path resolved = m_base_dir / std::filesystem::path(key);
    return base_repo::get(resolved.string(), hint);
  }
};

class ram_repo : public base_repo
{
  const std::map<std::string, std::vector<char>>& m_data;

  static span<char>
  to_span(const std::vector<char>& data)
  {
    auto ptr = reinterpret_cast<char*>(const_cast<char*>(data.data()));
    return span<char>{ptr, data.size()};
  }
  
public:
  ram_repo(const std::map<std::string, std::vector<char>>& repo)
    : m_data(repo)
  {}

  void
  add_data(const std::string&, span<char>, data_mode) const override
  {
    throw std::runtime_error("Cannot add artifacts to a ram repo");
  }

  void
  add_data(const std::string&, std::string&&) const override
  {
    throw std::runtime_error("Cannot add artifacts to a ram repo");
  }

  void
  add_file(const std::string&, file_mode) const override
  {
    throw std::runtime_error("Cannot add artifacts to a ram repo");
  }

  span<char>
  get(const std::string& key) const override
  {
    if (auto it = m_data.find(key); it != m_data.end())
      return to_span((*it).second);

    throw std::runtime_error{"Failed to find artifact: " + key};
    
  }

  span<char>
  get(const std::string& key, file_mode) const override
  {
    return get(key);
  }
};

////////////////////////////////////////////////////////////////
// Public API
////////////////////////////////////////////////////////////////
repository::
repository()
  : xrt::detail::pimpl<repository_impl>{std::make_unique<base_repo>()}
{}

repository::
repository(const std::filesystem::path& artifacts_dir)
  : xrt::detail::pimpl<repository_impl>{std::make_unique<file_repo>(artifacts_dir)}
{}

repository::
repository(const std::map<std::string, std::vector<char>>& repo)
  : xrt::detail::pimpl<repository_impl>{std::make_unique<ram_repo>(repo)}
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
