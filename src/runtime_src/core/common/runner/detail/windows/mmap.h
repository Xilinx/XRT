// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef ARTIFACTS_DETAIL_WINDOWS_MMAP_H
#define ARTIFACTS_DETAIL_WINDOWS_MMAP_H

// This file is not to be included stand-alone

#include <cstddef>
#include <string>
#include <stdexcept>

// Exclude rarely-used headers (e.g. Winsock) from <windows.h> for faster builds and fewer conflicts.
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace xrt_core::artifacts::detail {

/// Holds a file mapped with CreateFileMapping/MapViewOfFile on Windows. Non-copyable, movable.
struct mmap_artifact
{
  HANDLE m_file_handle = INVALID_HANDLE_VALUE;
  std::size_t m_size = 0;
  HANDLE m_mapping_handle = nullptr;
  void* m_ptr = nullptr;

  mmap_artifact() = default;

  explicit
  mmap_artifact(const std::string& path)
   : m_file_handle{open_file(path)}
   , m_size{get_size(m_file_handle)}
   , m_mapping_handle{create_mapping(m_file_handle, m_size)}
   , m_ptr{map_view(m_file_handle, m_mapping_handle)}
  {}

  ~mmap_artifact()
  {
    if (m_ptr)
    {
      UnmapViewOfFile(m_ptr);
      m_ptr = nullptr;
    }
    if (m_mapping_handle)
    {
      CloseHandle(m_mapping_handle);
      m_mapping_handle = nullptr;
    }
    if (m_file_handle != INVALID_HANDLE_VALUE)
    {
      CloseHandle(m_file_handle);
      m_file_handle = INVALID_HANDLE_VALUE;
    }
  }

  mmap_artifact(mmap_artifact&& other) noexcept
   : m_file_handle(other.m_file_handle)
   , m_size(other.m_size)
   , m_mapping_handle(other.m_mapping_handle)
   , m_ptr(other.m_ptr)
  {
    other.m_file_handle = INVALID_HANDLE_VALUE;
    other.m_size = 0;
    other.m_mapping_handle = nullptr;
    other.m_ptr = nullptr;
  }
  mmap_artifact&
  operator=(mmap_artifact&& other) noexcept
  {
    this->~mmap_artifact();
    m_file_handle = other.m_file_handle;
    m_size = other.m_size;
    m_mapping_handle = other.m_mapping_handle;
    m_ptr = other.m_ptr;
    other.m_file_handle = INVALID_HANDLE_VALUE;
    other.m_size = 0;
    other.m_mapping_handle = nullptr;
    other.m_ptr = nullptr;
    return *this;
  }
  mmap_artifact(const mmap_artifact&) = delete;
  mmap_artifact& operator=(const mmap_artifact&) = delete;

  span<char>
  get_span() const
  {
    if (!m_ptr || m_size == 0)
      return {};

    return span<char>(static_cast<char*>(m_ptr), m_size);
  }

private:
  static HANDLE
  open_file(const std::string& path)
  {
    HANDLE h = CreateFileA(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (h == INVALID_HANDLE_VALUE)
      throw std::runtime_error("artifacts::repository: CreateFile failed: " + path);

    return h;
  }

  static std::size_t
  get_size(HANDLE file)
  {
    LARGE_INTEGER li;
    if (!GetFileSizeEx(file, &li))
    {
      CloseHandle(file);
      throw std::runtime_error("artifacts::repository: GetFileSizeEx failed");
    }

    std::size_t size = static_cast<std::size_t>(li.QuadPart);
    if (size == 0)
    {
      CloseHandle(file);
      throw std::runtime_error("artifacts::repository: cannot map empty file");
    }

    return size;
  }

  static HANDLE
  create_mapping(HANDLE file, std::size_t)
  {
    HANDLE h = CreateFileMappingA(
        file,
        nullptr,
        PAGE_READONLY,
        0,
        0,
        nullptr);
    if (!h)
    {
      CloseHandle(file);
      throw std::runtime_error("artifacts::repository: CreateFileMapping failed");
    }

    return h;
  }

  static void*
  map_view(HANDLE file, HANDLE mapping)
  {
    void* ptr = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (!ptr)
    {
      CloseHandle(mapping);
      CloseHandle(file);
      throw std::runtime_error("artifacts::repository: MapViewOfFile failed");
    }

    return ptr;
  }
};

}  // namespace artifacts::detail

#endif  // ARTIFACTS_DETAIL_WINDOWS_MMAP_H
